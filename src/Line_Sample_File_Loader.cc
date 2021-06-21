//Line_Sample_File_Loader.cc - A part of DICOMautomaton 2019. Written by hal clark.
//
// This program loads line samples from files containing an exported samples_1D.
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
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for SplitStringToVector, Canonicalize_String2, SplitVector functions.
#include "YgorMathIOOBJ.h"


bool Load_From_Line_Sample_Files( Drover &DICOM_data,
                                  const std::map<std::string,std::string> & /* InvocationMetadata */,
                                  const std::string &,
                                  std::list<boost::filesystem::path> &Filenames ){

    //This routine will attempt to load an exported samples_1D. Both serialized and stringified formats are tested.
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
        const auto Filename = bfit->string();

        DICOM_data.lsamp_data.emplace_back( std::make_shared<Line_Sample>() );

        try{
            //////////////////////////////////////////////////////////////
            // Attempt to load the file.
            bool read_ok = true;
            {
                // Stringified version.
                std::ifstream FI(Filename.c_str(), std::ios::in);
                read_ok = DICOM_data.lsamp_data.back()->line.Read_From_Stream(FI);
            }
            if(!read_ok){
                // Serialized version.
                std::ifstream FI(Filename.c_str(), std::ios::in);
                read_ok = !!(FI >> DICOM_data.lsamp_data.back()->line);
            }
            if(!read_ok){
                throw std::runtime_error("Unable to read line sample from file.");
            }
            //////////////////////////////////////////////////////////////

            // Reject the file if the data is not valid.
            const auto N_samples = DICOM_data.lsamp_data.back()->line.samples.size();
            const auto N_metadata = DICOM_data.lsamp_data.back()->line.metadata.size();
            if( (N_samples == 0)
            &&  (N_metadata == 0) ){
                throw std::runtime_error("Unable to read line sample from file.");
            }

            FUNCINFO("Loaded line sample with " 
                     << N_samples << " datum and "
                     << N_metadata << " metadata keys");
            bfit = Filenames.erase( bfit ); 
            continue;
        }catch(const std::exception &e){
            FUNCINFO("Unable to load as line sample file");
            DICOM_data.lsamp_data.pop_back();
        };

        //Skip the file. It might be destined for some other loader.
        ++bfit;
    }

    return true;
}
