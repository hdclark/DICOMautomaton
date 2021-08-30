//TAR_File_Loader.h.

#pragma once

#include <string>    
#include <map>
#include <list>

#include <filesystem>

#include "Structs.h"

bool Load_From_TAR_Files( Drover &DICOM_data,
                          const std::map<std::string,std::string> &InvocationMetadata,
                          const std::string &FilenameLex,
                          std::list<OperationArgPkg> &Operations,
                          std::list<std::filesystem::path> &Filenames );
