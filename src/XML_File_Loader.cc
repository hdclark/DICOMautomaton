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
//#include <cfenv>              //Needed for std::feclearexcept(FE_ALL_EXCEPT).

#include <filesystem>
#include <cstdlib>            //Needed for exit() calls.

#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"

#include "XML_Tools.h"
#include "Metadata.h"
#include "Structs.h"
#include "GIS.h"

bool contains_xml_signature(dcma::xml::node &root){
    bool contains_an_xml_named_node = false;

    const bool permit_recursive_search = false;
    std::vector<std::string> names;
    names.push_back("xml");

    dcma::xml::search_callback_t f_xml = [&](const dcma::xml::node_chain_t &nc) -> bool {
        contains_an_xml_named_node = true;
        return false;
    };
    dcma::xml::search_by_names(root, std::begin(names), std::end(names), f_xml, permit_recursive_search);
    return contains_an_xml_named_node;
}

std::list<contour_collection<double>>
contains_gpx_gps_coords(dcma::xml::node &root){
    std::list<contour_collection<double>> out;

    const bool permit_recursive_search = false;

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
        permit_recursive_search );

    dcma::xml::search_by_names(root,
        { "gpx", "metadata", "link", "text" },
        [&](const dcma::xml::node_chain_t &nc) -> bool {
            const auto &n = nc.back().get().content;
            if( !n.empty() ){
                name_opt = n;
            }
            return true;
        },
        permit_recursive_search );

    // This callback is for handling individual track points (i.e., vertices).
    dcma::xml::search_callback_t f_trkpts = [&](const dcma::xml::node_chain_t &nc) -> bool {
        const auto lat_opt = get_as<double>(nc.back().get().metadata, "lat");
        const auto lon_opt = get_as<double>(nc.back().get().metadata, "lon");

        if(lat_opt && lon_opt){
            // Mercator projection.
            const auto [x, y] = dcma::gis::project_mercator(lat_opt.value(), lon_opt.value());
            out.back().contours.back().points.emplace_back( x, y, 0.0 );
        }
        return true;
    };

    // Callback for processing each track segment.
    //
    // For each track segment, create a new contour and then recursively search for track points (i.e., vertices).
    dcma::xml::search_callback_t f_trksegs = [&](const dcma::xml::node_chain_t &nc) -> bool {
        out.emplace_back();
        out.back().contours.emplace_back();
        dcma::xml::search_by_names(nc.back().get(),
                                   { "trkpt" },
                                   f_trkpts,
                                   permit_recursive_search);
        return true;
    };

    // Process each track separately, one at a time.
    //
    // A track can hold metadata and multiple track segments, which are converted to contours separately.
    dcma::xml::search_by_names(root,
        { "gpx", "trk" },
        [&](const dcma::xml::node_chain_t &nc) -> bool {
            decltype(out) shtl;
            std::swap(out,shtl);

            // Extract track data as contours.
            dcma::xml::search_by_names(nc.back().get(),
                                       { "trkseg" },
                                       f_trksegs,
                                       permit_recursive_search);

            // Search for track metadata, and assign it to the extracted contours.
            std::optional<std::string> l_name_opt;
            dcma::xml::search_by_names(root,
                { "name" },
                [&](const dcma::xml::node_chain_t &nc) -> bool {
                    const auto &n = nc.back().get().content;
                    if( !n.empty() ){
                        l_name_opt = n;
                    }
                    return true;
                },
                permit_recursive_search );

            if(l_name_opt){
                for(auto &cc : out){
                    for(auto &c : cc.contours){
                        insert_if_new(c.metadata, "ROIName", name_opt.value());
                    }
                }
            }

            // Merge the temporary buffer into the outgoing list.
            std::swap(out,shtl);
            shtl.remove_if([](const contour_collection<double> &cc){
                return cc.contours.empty();
            });
            out.merge(shtl);
            return true;
        },
        permit_recursive_search);

    // Inject top-level metadata if nothing more specific has been found yet.
    if(name_opt){
        for(auto &cc : out){
            for(auto &c : cc.contours){
                insert_if_new(c.metadata, "ROIName", name_opt.value());
            }
        }
    }
        
    return out;
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
            auto cc = contains_gpx_gps_coords(root);
            if(!cc.empty()){
                auto cm = cc.back().get_common_metadata({}, {});

                for(auto& c : cc.back().contours){
                    auto l_meta = c.metadata;
                    insert_if_new(l_meta, "ROIName", Filename.string());

                    l_meta = coalesce_metadata_for_rtstruct(l_meta);
                    l_meta["Fullpath"] = Filename.string();
                    l_meta["Filename"] = Filename.filename().string();

                    inject_metadata(c.metadata, std::move(l_meta));
                }

                // Inject the data.
                DICOM_data.Ensure_Contour_Data_Allocated();
                DICOM_data.contour_data->ccs.splice( std::end(DICOM_data.contour_data->ccs), cc );
                cc.clear();

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
