//STL_File_Loader.h.

#pragma once

#include <string>    
#include <map>
#include <list>

#include <boost/filesystem.hpp>

#include "Structs.h"

bool Load_Mesh_From_ASCII_STL_Files( Drover &DICOM_data,
                                     std::map<std::string,std::string> &InvocationMetadata,
                                     const std::string &FilenameLex,
                                     std::list<boost::filesystem::path> &Filenames );

bool Load_Mesh_From_Binary_STL_Files( Drover &DICOM_data,
                                      std::map<std::string,std::string> &InvocationMetadata,
                                      const std::string &FilenameLex,
                                      std::list<boost::filesystem::path> &Filenames );
