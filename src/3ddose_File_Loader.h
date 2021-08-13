//3ddose_File_Loader.h.

#pragma once

#include <string>    
#include <map>
#include <list>

#include <filesystem>

#include "Structs.h"

bool Load_From_3ddose_Files( Drover &DICOM_data,
                          const std::map<std::string,std::string> &InvocationMetadata,
                          const std::string &FilenameLex,
                          std::list<std::filesystem::path> &Filenames );
