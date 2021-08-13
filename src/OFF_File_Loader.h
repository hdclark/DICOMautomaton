//OFF_File_Loader.h.

#pragma once

#include <string>    
#include <map>
#include <list>

#include <filesystem>

#include "Structs.h"

bool Load_Points_From_OFF_Files( Drover &DICOM_data,
                                 const std::map<std::string,std::string> &InvocationMetadata,
                                 const std::string &FilenameLex,
                                 std::list<std::filesystem::path> &Filenames );

bool Load_Mesh_From_OFF_Files( Drover &DICOM_data,
                               const std::map<std::string,std::string> &InvocationMetadata,
                               const std::string &FilenameLex,
                               std::list<std::filesystem::path> &Filenames );
