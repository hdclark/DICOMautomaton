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
#include <filesystem>


#include <filesystem>
#include <boost/iostreams/filter/gzip.hpp>
//#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>

#include <cstdlib>            //Needed for exit() calls.

#include "Structs.h"
#include "File_Loader.h"

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorString.h"       //Needed for SplitStringToVector, Canonicalize_String2, SplitVector functions.
#include "YgorFilesDirs.h"
#include "YgorTAR.h"



bool Load_From_TAR_Files( Drover &DICOM_data,
                          std::map<std::string,std::string> &InvocationMetadata,
                          const std::string &FilenameLex,
                          std::list<OperationArgPkg> &Operations,
                          std::list<std::filesystem::path> &Filenames ){

    // This routine will attempt to load TAR-format files. Files that are not successfully loaded
    // are not consumed so that they can be passed on to the next loading stage as needed. 
    //
    if(Filenames.empty()) return true;

    size_t i = 0;
    const size_t N = Filenames.size();

    auto bfit = Filenames.begin();
    while(bfit != Filenames.end()){
        YLOGINFO("Parsing file #" << i+1 << "/" << N << " = " << 100*(i+1)/N << "%");
        ++i;
        const auto Filename = *bfit;

        // Encapsulated file handler.
        //
        // This routine merely writes the file to a temporary and invokes the generic file loader routine.
        long int N_encapsulated_files = 0;
        long int N_successfully_loaded = 0;

        const auto file_handler = [&]( std::istream &is,
                                       std::string fname,
                                       long int /*fsize*/,
                                       std::string /*fmode*/,
                                       std::string /*fuser*/,
                                       std::string /*fgroup*/,
                                       long int /*ftime*/,
                                       std::string /*o_name*/,
                                       std::string /*g_name*/,
                                       std::string /*fprefix*/) -> void {

            // Indicate that a file was detected.
            ++N_encapsulated_files;

            // Write the stream to a temporary file, attempting to honour the extension.
            const auto dir = (std::filesystem::temp_directory_path() / "dcma_TAR_temp_file").string();
            const auto ext = std::filesystem::path(fname).extension().string();
            const auto fname_tmp = Get_Unique_Filename(dir, 6, ext);
            if( 0 <= std::filesystem::temp_directory_path().compare( std::filesystem::path(fname_tmp)) ){
                // Note: If you get here, it's possible that there was an attempt to access the filesystem maliciously!
                YLOGERR("Temporary name is not contained within temporary directory. Refusing to continue");
            }
            {
                std::ofstream ofs_tmp(fname_tmp, std::ios::out | std::ios::binary);
                ofs_tmp << is.rdbuf();
                ofs_tmp.flush();
            }

            // Attempt to load the file.
            std::list<std::filesystem::path> path_tmp;
            path_tmp.emplace_back(fname_tmp);
            if(Load_Files(DICOM_data, InvocationMetadata, FilenameLex, Operations, path_tmp )){
                // Iff successful, indicate the success.
                ++N_successfully_loaded;
            }

            // Remove the temporary file.
            if(!RemoveFile(fname_tmp)){
                YLOGERR("Unable to remove temporary file '" << fname_tmp << "'. Refusing to continue");
            }

            return;
        };

        // un-compressed case.
        try{
            std::ifstream ifs(Filename, std::ios::in | std::ios::binary);

            N_encapsulated_files = 0;
            N_successfully_loaded = 0;
            read_ustar(ifs, file_handler); // Will throw if TAR file cannot be processed.

            if( N_encapsulated_files == 0L ){
                throw std::runtime_error("Unable to load as a TAR file.");
            }
            if( N_encapsulated_files != N_successfully_loaded ){
                throw std::runtime_error("Unable to load all encapsulated files inside TAR file.");
            }

            YLOGINFO("Loaded TAR file containing " << N_encapsulated_files << " encapsulated files");
            bfit = Filenames.erase( bfit ); 
            continue;

        }catch(const std::exception &e){
            YLOGINFO(e.what());
        };

        // gzip-compressed case.
        try{
            std::ifstream ifs(Filename, std::ios::in | std::ios::binary);

            boost::iostreams::filtering_istream ifsb;
            ifsb.push(boost::iostreams::gzip_decompressor());
            ifsb.push(ifs);

            N_encapsulated_files = 0;
            N_successfully_loaded = 0;
            read_ustar(ifsb, file_handler); // Will throw if TAR file cannot be processed.

            if( N_encapsulated_files == 0L ){
                throw std::runtime_error("Unable to load as a gzipped-TAR file.");
            }
            if( N_encapsulated_files != N_successfully_loaded ){
                throw std::runtime_error("Unable to load all encapsulated files inside gzipped-TAR file.");
            }

            YLOGINFO("Loaded gzipped TAR file containing " << N_encapsulated_files << " encapsulated files");
            bfit = Filenames.erase( bfit ); 
            continue;

        }catch(const std::exception &e){
            YLOGINFO(e.what());
        };

        // Skip the file. It might be destined for some other loader.
        ++bfit;
    }

    return true;
}

