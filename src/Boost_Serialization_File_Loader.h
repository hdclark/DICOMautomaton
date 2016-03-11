//Boost_Serialization_File_Loader.h.

#pragma once

#include <string>    
#include <map>
#include <list>

#include <boost/filesystem.hpp>

#include "Structs.h"

bool Load_From_Boost_Serialization_Files( Drover &DICOM_data,
                                          std::map<std::string,std::string> &InvocationMetadata,
                                          std::string &FilenameLex,
                                          std::list<boost::filesystem::path> &Filenames );
