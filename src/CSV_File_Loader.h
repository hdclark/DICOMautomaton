//CSV_File_Loader.h.

#pragma once

#include <string>    
#include <map>
#include <list>

#include <filesystem>

#include "Tables.h"
#include "Structs.h"

bool Load_From_CSV_Files( Drover &DICOM_data,
                          std::map<std::string,std::string> &InvocationMetadata,
                          const std::string &FilenameLex,
                          std::list<std::filesystem::path> &Filenames );
