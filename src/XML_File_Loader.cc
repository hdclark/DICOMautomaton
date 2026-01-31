//XML_File_Loader.cc - A part of DICOMautomaton 2023. Written by hal clark.
//
// This program attempts to load data from XML files. It is basic and can only currently deal with XML files 
// containing a well-defined, rigid schema.
//

#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
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

    // Temporary storage for time data associated with each track point.
    // This enables speed-based splitting after all points are collected.
    std::vector<std::optional<double>> track_times;

    // This callback is for handling individual track points (i.e., vertices).
    dcma::xml::search_callback_t f_trkpts = [&](const dcma::xml::node_chain_t &nc) -> bool {
        const auto lat_opt = get_as<double>(nc.back().get().metadata, "lat");
        const auto lon_opt = get_as<double>(nc.back().get().metadata, "lon");

        if(lat_opt && lon_opt){
            // Mercator projection.
            const auto [x, y] = dcma::gis::project_mercator(lat_opt.value(), lon_opt.value());
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

        // Store time data for speed-based splitting.
        track_times.push_back(time_opt);

        if( ele_opt && time_opt ){
            const bool inhibit_sort = true;
            lines_out.back().push_back( time_opt.value(), ele_opt.value(), inhibit_sort );
        }

        return true;
    };

    // Callback for processing each track segment.
    //
    // For each track segment, create a new contour and then recursively search for track points (i.e., vertices).
    dcma::xml::search_callback_t f_trksegs = [&](const dcma::xml::node_chain_t &nc) -> bool {
        contours_out.emplace_back();
        contours_out.back().contours.emplace_back();

        lines_out.emplace_back();

        // Clear time data for this track segment.
        track_times.clear();

        dcma::xml::search_by_names(nc.back().get(),
                                   { "trkpt" },
                                   f_trkpts,
                                   disable_recursive_search);

        // After collecting all points, analyze speeds and split based on major changes.
        // The original contour is kept as-is. Additional contours are created for each detected activity segment.
        auto &original_contour = contours_out.back().contours.back();
        const auto &points = original_contour.points;
        const size_t N_points = points.size();

        if(N_points >= 2 && track_times.size() == N_points){
            // Calculate instantaneous speeds between consecutive points.
            std::vector<double> speeds;
            speeds.reserve(N_points - 1);

            for(size_t i = 0; i + 1 < N_points; ++i){
                if(track_times[i] && track_times[i+1]){
                    const auto &p1 = points[i];
                    const auto &p2 = points[i+1];
                    const double dx = p2.x - p1.x;
                    const double dy = p2.y - p1.y;
                    const double dist = std::sqrt(dx*dx + dy*dy); // Distance in meters (Mercator projection)
                    const double dt = track_times[i+1].value() - track_times[i].value(); // Time in seconds

                    if(dt > 0.0){
                        const double speed = dist / dt; // Speed in m/s
                        speeds.push_back(speed);
                    }else{
                        speeds.push_back(0.0);
                    }
                }else{
                    speeds.push_back(0.0);
                }
            }

            // Detect split points based on major speed changes.
            // Use a threshold-based approach: split when speed changes by more than a factor.
            std::vector<size_t> split_indices;
            const double speed_change_threshold = 3.0; // Split when speed changes by 3x or more
            const double min_speed_threshold = 0.5; // Minimum speed (m/s) to consider for splitting

            for(size_t i = 0; i + 1 < speeds.size(); ++i){
                const double s1 = speeds[i];
                const double s2 = speeds[i+1];

                // Avoid division by very small speeds, and check for significant changes
                if(s1 > min_speed_threshold && s2 > min_speed_threshold){
                    const double ratio = (s1 > s2) ? (s1 / s2) : (s2 / s1);
                    if(ratio >= speed_change_threshold){
                        split_indices.push_back(i + 2); // Split at point i+2 (start of new speed segment)
                    }
                }else if(s1 <= min_speed_threshold && s2 > min_speed_threshold * speed_change_threshold){
                    // Transition from stationary to moving
                    split_indices.push_back(i + 2);
                }else if(s1 > min_speed_threshold * speed_change_threshold && s2 <= min_speed_threshold){
                    // Transition from moving to stationary
                    split_indices.push_back(i + 2);
                }
            }

            // Create additional contours for each detected segment.
            if(!split_indices.empty()){
                // Sort and remove duplicates
                std::sort(split_indices.begin(), split_indices.end());
                split_indices.erase(std::unique(split_indices.begin(), split_indices.end()), split_indices.end());

                // Filter out invalid indices
                split_indices.erase(
                    std::remove_if(split_indices.begin(), split_indices.end(),
                        [N_points](size_t idx){ return idx >= N_points; }),
                    split_indices.end());

                if(!split_indices.empty()){
                    size_t start_idx = 0;
                    size_t segment_num = 0;

                    for(size_t split_idx : split_indices){
                        if(split_idx > start_idx + 1){ // Ensure segment has at least 2 points
                            contour_of_points<double> segment_contour;
                            segment_contour.closed = false;
                            segment_contour.metadata = original_contour.metadata;
                            segment_contour.metadata["ActivitySegment"] = std::to_string(segment_num);

                            for(size_t i = start_idx; i < split_idx; ++i){
                                segment_contour.points.push_back(points[i]);
                            }

                            contours_out.back().contours.push_back(segment_contour);
                            ++segment_num;
                        }
                        start_idx = split_idx;
                    }

                    // Add the final segment.
                    if(start_idx < N_points - 1){
                        contour_of_points<double> segment_contour;
                        segment_contour.closed = false;
                        segment_contour.metadata = original_contour.metadata;
                        segment_contour.metadata["ActivitySegment"] = std::to_string(segment_num);

                        for(size_t i = start_idx; i < N_points; ++i){
                            segment_contour.points.push_back(points[i]);
                        }

                        contours_out.back().contours.push_back(segment_contour);
                    }
                }
            }
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
