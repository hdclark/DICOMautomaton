//Boost_Serialization_File_Loader.cc - A part of DICOMautomaton 2016. Written by hal clark.
//
// This program loads data from files which have been serialized in Protobuf format.
//

#include <iostream>
#include <fstream>
#include <string>    
#include <map>
#include <list>

#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <experimental/optional>

#include <boost/algorithm/string.hpp> //For boost:iequals().

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorImages.h"
#include "YgorImagesIOBoostSerialization.h"

#include "Structs.h"
#include "Common_Boost_Serialization.h"


bool Load_From_Boost_Serialization_Files( Drover &DICOM_data,
                                          std::map<std::string,std::string> & /* InvocationMetadata */,
                                          std::string & /* FilenameLex */,
                                          std::list<boost::filesystem::path> &Filenames ){

    std::list<boost::filesystem::path> Filenames_Copy(Filenames);
    Filenames.clear();
    for(const auto &fn : Filenames_Copy){

        Drover A;
        const bool res = Common_Boost_Deserialize_Drover(A, fn);
        if(res){

            //Concatenate loaded data to the existing data.

            //  ... TODO ...


//FOR TESTING ONLY - Replace the existing data - FOR TESTING ONLY. TODO
DICOM_data = A;

            //Consume the input -- do not add it to the 'unsuccessful pile.'
            continue;
        }

        //Not a (known) file we can parse. Leave it as-is.
        Filenames.emplace_back(fn);
        continue;
    }

    return true;
}
