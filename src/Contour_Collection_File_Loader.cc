//Contour_Collection_File_Loader.cc - A part of DICOMautomaton 2022. Written by hal clark.
//
// This program loads contour collections from plaintext files.
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

#include <filesystem>
#include <cstdlib>            //Needed for exit() calls.

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for SplitStringToVector, Canonicalize_String2, SplitVector functions.

#include "Structs.h"
#include "Metadata.h"


bool
Write_Contour_Collections( const std::list<std::reference_wrapper<contour_collection<double>>> &cc_ROIs,
                           std::ostream &os ){

    os << "DCMA_plaintext_contours_v1" << '\n';

    for(auto &cc_refw : cc_ROIs){
        os << "start_collection" << '\n';

        for(auto &c : cc_refw.get().contours){
            os << "start_contour" << '\n';
            
            for(const auto &mp : c.metadata){
                os << "metadata_item " << encode_metadata_kv_pair(mp) << "\n";
            }
            os << c.write_to_string() << '\n';
        }
    }

    return !!os;
}

std::list<contour_collection<double>>
Read_Contour_Collections( std::istream &is ){
    std::list<contour_collection<double>> ccs;
    ccs.emplace_back();
    ccs.back().contours.emplace_back();

    // Check the magic bytes.
    const std::string expected_magic("DCMA_plaintext_contours_v1");
    std::string magic( expected_magic.length(), '\0');
    if(!is.read(magic.data(), magic.length())){
        throw std::invalid_argument("Unable to read from file");
    }
    if(magic != expected_magic){
        throw std::invalid_argument("File not in the expected format");
    }

    // Read the rest of the file one line at a time.
    std::map<std::string,std::string> metadata;
    std::string line;
    try{
        while(std::getline(is, line)){
            if(line.empty()) continue;

            // Check if the entire line is whitespace. If so, ignore it.
            const auto p_nonws = line.find_first_not_of(" \t");
            if(p_nonws == std::string::npos) continue;

            const auto p_space = line.find(" "); // or npos.
            const auto keyword = line.substr(static_cast<size_t>(0), p_space);

            if(false){
            }else if(keyword == expected_magic){
                // Ignoring repeated the magic header will allow files to be concatenated.
                continue;

            }else if(keyword == "start_collection"){
                ccs.emplace_back();
                metadata.clear();

            }else if(keyword == "start_contour"){
                metadata.clear();

            }else if(keyword == "metadata_item"){
                auto kvp_opt = decode_metadata_kv_pair(line);
                if(kvp_opt){
                    metadata.insert(kvp_opt.value());
                }else{
                    continue;
                }

            }else if(keyword == "{"){
                ccs.back().contours.emplace_back();
                if( !ccs.back().contours.back().load_from_string(line) ){
                    throw std::runtime_error("Unable to parse contour");
                }
                ccs.back().contours.back().metadata = metadata;

            }else if(keyword == "#"){
                // Ignore comments.
                continue;

            }else{
                throw std::invalid_argument("Unrecognized line '"_s + line + "'");
            }
        }
    }catch( const std::exception &e ){
        throw;
    }

    // Purge all empty contours and collections.
    for(auto& cc : ccs){
        cc.contours.remove_if([](const contour_of_points<double>& cop){
                                  return cop.points.empty();
                              });
    }
    ccs.remove_if([](const contour_collection<double>& cc){
                      return cc.contours.empty();
                  });

    return ccs;
}


bool Load_From_Contour_Collection_Files( Drover &DICOM_data,
                                         std::map<std::string,std::string> & /* InvocationMetadata */,
                                         const std::string &,
                                         std::list<std::filesystem::path> &Filenames ){

    //This routine will attempt to load plaintext-format files. Files that are not successfully loaded
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
        FUNCINFO("Parsing file #" << i+1 << "/" << N << " = " << 100*(i+1)/N << "%");
        ++i;
        const auto Filename = *bfit;

        try{
            //////////////////////////////////////////////////////////////
            // Attempt to load the file.
            std::ifstream FI(Filename, std::ios::in);
            auto ccs = Read_Contour_Collections( FI );
            FI.close();
            //////////////////////////////////////////////////////////////

            // Reject the file if the file format is not valid.
            const auto N_ccs = ccs.size();
            if( N_ccs == static_cast<size_t>(0) ){
                throw std::runtime_error("No contour collections were loaded, assuming file type is not valid.");
            }
            size_t N_cs = 0U;
            for(const auto &cc : ccs){
                const auto l_N_cs = cc.contours.size();
                if( l_N_cs == static_cast<size_t>(0) ){
                    throw std::runtime_error("No contours were loaded, assuming file type is not valid.");
                }
                N_cs += l_N_cs;
            }

            // Supply generic minimal metadata iff it is needed.
            for(auto &cc : ccs){
                auto cm = cc.get_common_metadata({}, {});

                for(auto& c : cc.contours){
                    auto l_meta = c.metadata;
                    l_meta = coalesce_metadata_for_rtstruct(l_meta);
                    l_meta["Filename"] = Filename.string();
                    inject_metadata(c.metadata, std::move(l_meta));
                }
            }

            // Inject the data.
            DICOM_data.Ensure_Contour_Data_Allocated();
            DICOM_data.contour_data->ccs.splice( std::end(DICOM_data.contour_data->ccs), ccs );

            FUNCINFO("Loaded " << N_ccs << " contour collections with a total of " << N_cs << " contours");
            bfit = Filenames.erase( bfit ); 
            continue;
        }catch(const std::exception &e){
            FUNCINFO("Unable to load as plaintext contour collection file: '" << e.what() << "'");
        };

        //Skip the file. It might be destined for some other loader.
        ++bfit;
    }

    return true;
}

