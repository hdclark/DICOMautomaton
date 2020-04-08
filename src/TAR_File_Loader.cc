//TAR_File_Loader.cc - A part of DICOMautomaton 2020. Written by hal clark.
//
// This program loads files that are encapsulated in TAR files.
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

#include <boost/filesystem.hpp>
#include <cstdlib>            //Needed for exit() calls.

#include "Structs.h"
#include "File_Loader.h"

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for SplitStringToVector, Canonicalize_String2, SplitVector functions.
#include "YgorTAR.h"



bool Load_From_TAR_Files( Drover &DICOM_data,
                          std::map<std::string,std::string> &InvocationMetadata,
                          std::string &FilenameLex,
                          std::list<boost::filesystem::path> &Filenames ){

    //This routine will attempt to load TAR-format files. Files that are not successfully loaded
    // are not consumed so that they can be passed on to the next loading stage as needed. 
    //
    if(Filenames.empty()) return true;

    size_t i = 0;
    const size_t N = Filenames.size();

    auto bfit = Filenames.begin();
    while(bfit != Filenames.end()){
        FUNCINFO("Parsing file #" << i+1 << "/" << N << " = " << 100*(i+1)/N << "%");
        ++i;
        const auto Filename = bfit->string();

        try{
            //////////////////////////////////////////////////////////////
            // Attempt to load the file.
            long int N_encapsulated_files = 0;
            long int N_successfully_loaded = 0;

            const auto file_handler = [&]( std::istream &is,
                                           std::string fname,
                                           long int fsize,
                                           std::string fmode,
                                           std::string fuser,
                                           std::string fgroup,
                                           long int ftime,
                                           std::string o_name,
                                           std::string g_name,
                                           std::string fprefix) -> void {

                // Indicate that a file was detected.
                ++N_encapsulated_files;

                // Write the stream to a temporary file.
                const std::string fname_tmp = Get_Unique_Sequential_Filename("/tmp/dcma_TAR_loading_temporary_");
                {
                    std::ofstream ofs_tmp(fname_tmp);
                    ofs_tmp << is.rdbuf();
                    ofs_tmp.flush();
                }

                // Attempt to load the file.
                std::list<boost::filesystem::path> path_tmp;
                path_tmp.emplace_back(fname_tmp);
                if(Load_Files(DICOM_data, InvocationMetadata, FilenameLex, path_tmp )){
                    // Iff successful, indicate the success.
                    ++N_successfully_loaded;
                }

                // Remove the temporary file.
                if(!RemoveFile(fname_tmp)){
                    FUNCERR("Unable to remove temporary file '" << fname_tmp << "'. Refusing to continue");
                }

                return;
            };

            std::ifstream FI(Filename.c_str(), std::ios::in);
            read_ustar(FI, file_handler); // Will throw if TAR file cannot be processed.
            FI.close();

            // If the same number of files detected were also loaded, the TAR file was successfully read.
            // Otherwise, one or more files could not be processed so indicate failure.
            if( N_encapsulated_files != N_successfully_loaded ){
                throw std::runtime_error("Unable to load all encapsulated files.");
            }

            FUNCINFO("Loaded TAR file containing " << N_encapsulated_files << " encapsulated files");
            bfit = Filenames.erase( bfit ); 
            continue;
        }catch(const std::exception &e){
            FUNCINFO("Unable to load as TAR file");
        };

        //Skip the file. It might be destined for some other loader.
        ++bfit;
    }

    return true;
}

