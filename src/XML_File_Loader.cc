//XML_File_Loader.cc - A part of DICOMautomaton 2023. Written by hal clark.
//
// This program attempts to load data from XML files. It is basic and can only currently deal with XML files 
// containing a well-defined, rigid schema.
//

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
//#include <cfenv>              //Needed for std::feclearexcept(FE_ALL_EXCEPT).

#include <filesystem>
#include <cstdlib>            //Needed for exit() calls.

#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorTime.h"

#include "XML_Tools.h"
#include "Metadata.h"
#include "String_Parsing.h"
#include "Structs.h"
#include "GIS.h"


// Helper structure to store track point data for speed-based splitting.
struct gpx_track_point_t {
    vec3<double> projected;  // Mercator-projected position (x, y, 0).
    bool has_valid_position = false;  // Whether projected coordinates are valid.
    std::optional<double> time;  // UNIX timestamp, if available.
    std::optional<double> elevation;  // Elevation, if available.
};

// Split a GPX trace into multiple activity segments based on speed changes.
//
// This function analyzes a sequence of track points with time data and detects major speed changes
// that may indicate different activities (e.g., stopped vs. moving, walking vs. running).
//
// The algorithm works as follows:
// 1. Compute speeds between consecutive points that have valid time data.
// 2. Compute a reference speed (median of non-zero speeds).
// 3. Identify split points where speed drops significantly (e.g., below a fraction of the reference)
//    or where there is a significant time gap.
// 4. Create separate contours for each detected activity segment.
//
// Parameters:
// - points: The track points with position and time data.
// - base_name: The base name to use for naming split activities.
//
// Returns:
// - A list of contour collections, one for each detected activity segment.
//   Each contour collection contains a single contour representing the activity.
//
static std::list<contour_collection<double>>
split_gpx_by_speed(const std::vector<gpx_track_point_t> &points,
                   const std::optional<std::string> &base_name){

    std::list<contour_collection<double>> split_contours;

    // Need at least 2 points with time data to compute speeds.
    if(points.size() < 2){
        return split_contours;
    }

    // Check if we have enough time data to perform speed-based splitting.
    size_t points_with_time = 0;
    for(const auto &pt : points){
        if(pt.time.has_value()){
            ++points_with_time;
        }
    }
    if(points_with_time < 2){
        return split_contours;
    }

    // Compute speeds between consecutive points with valid time data and valid positions.
    // Store (index, speed) pairs for analysis.
    std::vector<std::pair<size_t, double>> speeds;
    speeds.reserve(points.size());

    for(size_t i = 1; i < points.size(); ++i){
        // Skip speed calculation if either point lacks valid projected coordinates.
        if(!points[i].has_valid_position || !points[i-1].has_valid_position){
            continue;
        }
        if(points[i].time.has_value() && points[i-1].time.has_value()){
            const double dt = points[i].time.value() - points[i-1].time.value();
            if(dt > 0.0){
                const double dx = points[i].projected.x - points[i-1].projected.x;
                const double dy = points[i].projected.y - points[i-1].projected.y;
                const double dist = std::sqrt(dx*dx + dy*dy);
                const double speed = dist / dt;  // metres per second.
                speeds.emplace_back(i, speed);
            }
        }
    }

    if(speeds.empty()){
        return split_contours;
    }

    // Build a map from point index to speed for O(1) lookups.
    std::map<size_t, double> speed_map;
    for(const auto &s : speeds){
        speed_map[s.first] = s.second;
    }

    // Compute a reference speed (median of non-zero speeds).
    std::vector<double> speed_values;
    speed_values.reserve(speeds.size());
    for(const auto &s : speeds){
        if(s.second > 0.0){
            speed_values.push_back(s.second);
        }
    }

    if(speed_values.empty()){
        return split_contours;
    }

    std::sort(speed_values.begin(), speed_values.end());
    double median_speed = 0.0;
    const size_t n = speed_values.size();
    if(n % 2 == 0){
        // Even number of elements: average of two middle values.
        median_speed = (speed_values[n / 2 - 1] + speed_values[n / 2]) / 2.0;
    }else{
        // Odd number of elements: middle value.
        median_speed = speed_values[n / 2];
    }

    // Define thresholds for detecting activity splits.
    // - Speed drop threshold: speed below 10% of median suggests a stop/pause.
    // - Time gap threshold: gaps > 60 seconds suggest separate activities.
    const double speed_drop_fraction = 0.10;
    const double speed_drop_threshold = median_speed * speed_drop_fraction;
    const double time_gap_threshold = 60.0;  // seconds.

    // Find split points where speed drops significantly or there's a time gap.
    std::vector<size_t> split_indices;
    split_indices.push_back(0);  // Start of first segment.

    for(size_t i = 1; i < points.size(); ++i){
        bool should_split = false;

        // Check for time gap.
        if(points[i].time.has_value() && points[i-1].time.has_value()){
            const double dt = points[i].time.value() - points[i-1].time.value();
            if(dt > time_gap_threshold){
                should_split = true;
            }
        }

        // Check for speed drop using the speed map for O(1) lookup.
        auto it = speed_map.find(i);
        if(it != speed_map.end() && it->second < speed_drop_threshold){
            should_split = true;
        }

        if(should_split){
            split_indices.push_back(i);
        }
    }

    // Only create split contours if we found multiple segments.
    if(split_indices.size() < 2){
        return split_contours;
    }

    // First pass: count valid segments (with at least 2 points that have valid positions).
    size_t valid_segment_count = 0;
    for(size_t seg = 0; seg < split_indices.size(); ++seg){
        const size_t start_idx = split_indices[seg];
        const size_t end_idx = (seg + 1 < split_indices.size()) 
                               ? split_indices[seg + 1] 
                               : points.size();
        size_t valid_points_in_segment = 0;
        for(size_t i = start_idx; i < end_idx; ++i){
            if(points[i].has_valid_position){
                ++valid_points_in_segment;
            }
        }
        if(valid_points_in_segment >= 2){
            ++valid_segment_count;
        }
    }

    // Only create split contours if we have at least 2 valid segments.
    if(valid_segment_count < 2){
        return split_contours;
    }

    // Second pass: create contours for each valid segment with proper numbering.
    size_t segment_number = 0;
    for(size_t seg = 0; seg < split_indices.size(); ++seg){
        const size_t start_idx = split_indices[seg];
        const size_t end_idx = (seg + 1 < split_indices.size()) 
                               ? split_indices[seg + 1] 
                               : points.size();

        contour_collection<double> cc;
        cc.contours.emplace_back();

        // Only add points that have valid projected coordinates.
        for(size_t i = start_idx; i < end_idx; ++i){
            if(points[i].has_valid_position){
                cc.contours.back().points.push_back(points[i].projected);
            }
        }

        // After filtering, ensure the segment still has at least 2 valid points.
        if(cc.contours.back().points.size() < 2){
            continue;
        }

        ++segment_number;

        // Set metadata for the split segment using sequential segment numbering.
        if(base_name.has_value()){
            const std::string segment_name = base_name.value() + "_activity_" + std::to_string(segment_number);
            insert_if_new(cc.contours.back().metadata, "ROIName", segment_name);
        }
        insert_if_new(cc.contours.back().metadata, "ActivitySegment", std::to_string(segment_number));
        insert_if_new(cc.contours.back().metadata, "TotalActivitySegments", std::to_string(valid_segment_count));

        split_contours.push_back(std::move(cc));
    }

    return split_contours;
}

bool contains_xml_signature(dcma::xml::node &root){
    bool contains_an_xml_named_node = false;

    const bool disable_recursive_search = false;
    std::vector<std::string> names;
    names.push_back("xml");

    dcma::xml::search_callback_t f_xml = [&](const dcma::xml::node_chain_t &nc) -> bool {
        contains_an_xml_named_node = true;
        return false;
    };
    dcma::xml::search_by_names(root, std::begin(names), std::end(names), f_xml, disable_recursive_search);
    return contains_an_xml_named_node;
}

std::pair< std::list<contour_collection<double>>, std::list<samples_1D<double>>>
contains_gpx_gps_coords(dcma::xml::node &root){
    std::list<contour_collection<double>> contours_out;
    std::list<samples_1D<double>> lines_out;

    const bool disable_recursive_search = false;
    const bool enable_recursive_search  = true;

    // Look for some sort of top-level identifier (e.g., name or description).
    std::optional<std::string> name_opt;

    dcma::xml::search_by_names(root,
        { "gpx", "metadata", "name" },
        [&](const dcma::xml::node_chain_t &nc) -> bool {
            const auto &n = nc.back().get().content;
            if( !n.empty() ){
                name_opt = n;
            }
            return true;
        },
        disable_recursive_search );

    dcma::xml::search_by_names(root,
        { "gpx", "metadata", "link", "text" },
        [&](const dcma::xml::node_chain_t &nc) -> bool {
            const auto &n = nc.back().get().content;
            if( !n.empty() ){
                name_opt = n;
            }
            return true;
        },
        disable_recursive_search );

    // Temporary storage for track points with time data, used for speed-based splitting.
    std::vector<gpx_track_point_t> track_points;

    // This callback is for handling individual track points (i.e., vertices).
    dcma::xml::search_callback_t f_trkpts = [&](const dcma::xml::node_chain_t &nc) -> bool {
        const auto lat_opt = get_as<double>(nc.back().get().metadata, "lat");
        const auto lon_opt = get_as<double>(nc.back().get().metadata, "lon");

        gpx_track_point_t pt;

        if(lat_opt && lon_opt){
            // Mercator projection.
            const auto [x, y] = dcma::gis::project_mercator(lat_opt.value(), lon_opt.value());
            pt.projected = vec3<double>(x, y, 0.0);
            pt.has_valid_position = true;
            contours_out.back().contours.back().points.emplace_back( x, y, 0.0 );
        }

        // Look for an optional elevation.
        std::optional<double> ele_opt;
        dcma::xml::search_by_names(nc.back().get(),
            { "ele" },
            [&](const dcma::xml::node_chain_t &nc) -> bool {
                const auto &c = nc.back().get().content;
                const auto l_ele_opt = get_as<double>(c);
                if( !ele_opt
                &&  l_ele_opt ){
                    ele_opt = l_ele_opt;
                }
                return true;
            },
            disable_recursive_search);

        // Look for an optional datetime.
        std::optional<double> time_opt;
        dcma::xml::search_by_names(nc.back().get(),
            { "time" },
            [&](const dcma::xml::node_chain_t &nc) -> bool {
                const auto &c = nc.back().get().content;

                time_mark t;
                double t_frac = 0.0;
                if( !time_opt
                &&  t.Read_from_string(c, &t_frac)){
                    time_opt = t.As_UNIX_time() + t_frac;
                }
                return true;
            },
            disable_recursive_search);

        if( ele_opt && time_opt ){
            const bool inhibit_sort = true;
            lines_out.back().push_back( time_opt.value(), ele_opt.value(), inhibit_sort );
        }

        // Store track point data for speed-based splitting only if it has valid position.
        // Points without valid lat/lon are not useful for activity splitting.
        if(pt.has_valid_position){
            pt.time = time_opt;
            pt.elevation = ele_opt;
            track_points.push_back(pt);
        }

        return true;
    };

    // Callback for processing each track segment.
    //
    // For each track segment, create a new contour and then recursively search for track points (i.e., vertices).
    // Additionally, analyze track points for speed-based activity splitting and create additional contours
    // for detected activity segments.
    dcma::xml::search_callback_t f_trksegs = [&](const dcma::xml::node_chain_t &nc) -> bool {
        contours_out.emplace_back();
        contours_out.back().contours.emplace_back();

        lines_out.emplace_back();

        // Clear track points from previous segment.
        track_points.clear();

        dcma::xml::search_by_names(nc.back().get(),
                                   { "trkpt" },
                                   f_trkpts,
                                   disable_recursive_search);

        // Attempt to split the trace based on speed changes.
        // This creates additional contours for detected activity segments.
        auto split_ccs = split_gpx_by_speed(track_points, name_opt);
        for(auto &scc : split_ccs){
            contours_out.push_back(std::move(scc));
        }

        return true;
    };

    // Process each track separately, one at a time.
    //
    // A track can hold metadata and multiple track segments, which are converted to contours separately.
    dcma::xml::search_by_names(root,
        { "gpx", "trk" },
        [&](const dcma::xml::node_chain_t &nc) -> bool {
            // Stow any prior extracted objects.
            decltype(contours_out) contour_shtl;
            std::swap(contours_out, contour_shtl);

            decltype(lines_out) lines_shtl;
            std::swap(lines_out, lines_shtl);

            // Extract track data as contours and line samples.
            dcma::xml::search_by_names(nc.back().get(),
                                       { "trkseg" },
                                       f_trksegs,
                                       disable_recursive_search);

            // Search for track metadata, and assign it to the extracted objects.
            if(name_opt){
                for(auto &cc : contours_out){
                    for(auto &c : cc.contours){
                        insert_if_new(c.metadata, "ROIName", name_opt.value());
                    }
                }

                for(auto &l : lines_out){
                    insert_if_new(l.metadata, "LineName", name_opt.value());
                }
            }

            // Merge the temporary buffer into the outgoing list.
            contours_out.remove_if([](const contour_collection<double> &cc){
                return cc.contours.empty();
            });

            lines_out.remove_if([](const samples_1D<double> &ls){
                return ls.samples.empty();
            });
            for(auto &l : lines_out){
                l.stable_sort();
            }

            // Roll the extracted objects into the broader collection.
            std::swap(contours_out, contour_shtl);
            contours_out.merge(contour_shtl);
            contour_shtl.clear();

            std::swap(lines_out, lines_shtl);
            lines_out.splice(std::end(lines_out), lines_shtl);
            lines_shtl.clear();
            return true;
        },
        disable_recursive_search);

    // Inject top-level metadata if nothing more specific has been found yet.
    if(name_opt){
        for(auto &cc : contours_out){
            for(auto &c : cc.contours){
                insert_if_new(c.metadata, "ROIName", name_opt.value());
            }
        }

        for(auto &l : lines_out){
            insert_if_new(l.metadata, "LineName", name_opt.value());
        }
    }
        
    return { contours_out, lines_out };
}


bool Load_From_XML_Files( Drover &DICOM_data,
                          std::map<std::string,std::string> & /* InvocationMetadata */,
                          const std::string &,
                          std::list<std::filesystem::path> &Filenames ){

    //This routine will attempt to load XML files on an individual file basis. Files that are not successfully loaded
    // are not consumed so that they can be passed on to the next loading stage as needed. 
    //
    // Note: This routine returns false only iff a file is suspected of being suited for this loader, but could not be
    //       loaded (e.g., the file seems appropriate, but a parsing failure was encountered).
    //
    if(Filenames.empty()) return true;

    size_t i = 0;
    const size_t N = Filenames.size();

    auto bfit = Filenames.begin();
    while(bfit != Filenames.end()){
        YLOGINFO("Parsing file #" << i+1 << "/" << N << " = " << 100*(i+1)/N << "%");
        ++i;
        const auto Filename = *bfit;


        std::ifstream ifs(Filename, std::ios::binary);
        dcma::xml::node root;
        bool read_successfully = false;
        try{
            dcma::xml::read_node(ifs, root);
            read_successfully = true;
        }catch(const std::exception &e){
            YLOGINFO("Unable to load as XML file: '" << e.what() << "'");
        }

        // Search for an XML node.
        bool contains_an_xml_named_node = contains_xml_signature(root);

        // Decide if this is an XML file.
        bool read_and_parsed_successfully = false;

        if( read_successfully && contains_an_xml_named_node){
            // Parse the tree and try to extract information from it.

            // Contours in 'GPX' (GPS coordinate) format.
            //std::list<contour_collection<double>> contours_out;
            //std::list<samples_1D<double>> lines_out;

            auto [ cc, ls ] = contains_gpx_gps_coords(root);
            if(!cc.empty()){
                auto cm = cc.back().get_common_metadata({}, {});

                for(auto& c : cc.back().contours){
                    auto l_meta = c.metadata;
                    insert_if_new(l_meta, "ROIName", Filename.string());

                    l_meta = coalesce_metadata_for_rtstruct(l_meta);
                    l_meta["Fullpath"] = Filename.string();
                    l_meta["Filename"] = Filename.filename().string();

                    inject_metadata(c.metadata, std::move(l_meta), metadata_preprocessing::none);
                }

                // Inject the data.
                DICOM_data.Ensure_Contour_Data_Allocated();
                DICOM_data.contour_data->ccs.splice( std::end(DICOM_data.contour_data->ccs), cc );
                cc.clear();

                read_and_parsed_successfully = true;
                // or
                //return false;
            }
            if(!ls.empty()){

                // Inject the data.
                for(auto & l_ls : ls){
                    auto l_meta = l_ls.metadata;
                    insert_if_new(l_meta, "LineName", Filename.string());

                    l_meta = coalesce_metadata_for_lsamp(l_meta);
                    l_meta["Fullpath"] = Filename.string();
                    l_meta["Filename"] = Filename.filename().string();

                    l_meta["Abscissa"] = "Time";
                    l_meta["Ordinate"] = "Elevation";

                    inject_metadata(l_ls.metadata, std::move(l_meta), metadata_preprocessing::none);

                    DICOM_data.lsamp_data.emplace_back( std::make_shared<Line_Sample>() );
                    DICOM_data.lsamp_data.back()->line = l_ls;
                }
                ls.clear();

                read_and_parsed_successfully = true;
                // or
                //return false;
            }

            // ... TODO other here ...


        }else if( contains_an_xml_named_node ){
            // This appears to be an XML file, but it was either malformed or has a structure we don't understand.
            YLOGWARN("File contains XML fingerprint, but could not be parsed");
            return false;

        }else{
            // This does not appear to be a valid XML file.
            read_and_parsed_successfully = false;
        }

        if(read_and_parsed_successfully){
            // Remove the file.
            bfit = Filenames.erase( bfit ); 
        }else{
            //Skip the file. It might be destined for some other loader.
            ++bfit;
        }
    }
            
    // Post-process.
    const size_t N2 = Filenames.size();
    if(N == N2){
        // Nothing added, so do not bother post-processing.
        return true;
    }

    // ... post process here ...

    return true;
}
