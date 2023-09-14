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
    std::vector<std::string> names;
    names.push_back("gpx");
    names.push_back("trk");
    names.push_back("trkseg");
    names.push_back("trkpt");


    dcma::xml::search_callback_t f_all = [&](const dcma::xml::node_chain_t &nc) -> bool {
        const auto lat_opt = get_as<double>(nc.back().get().metadata, "lat");
        const auto lon_opt = get_as<double>(nc.back().get().metadata, "lon");

        if(lat_opt && lon_opt){
            // Mercator projection.
            const double pi = 3.141592653;
            const double R = 6'371'000; // mean radius of Earth, in metres.
            const double l = lon_opt.value() * (pi / 180.0); // converted from degrees to radians.
            const double t = lat_opt.value() * (pi / 180.0); // converted from degrees to radians.
            const double x = R * l;
            const double y = -R * std::log( std::tan( (pi * 0.25) + (t * 0.5) ) );

            if(out.empty()){
                out.emplace_back();
            }
            if(out.back().contours.empty()){
                out.back().contours.emplace_back();
            }
            out.back().contours.back().points.emplace_back( x, y, 0.0 );
        }
        return true;
    };
    dcma::xml::search_by_names(root, std::begin(names), std::end(names), f_all, permit_recursive_search);
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
                    l_meta = coalesce_metadata_for_rtstruct(l_meta);
                    l_meta["Filename"] = Filename.string();
                    l_meta["ROIName"] = Filename.string();
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
