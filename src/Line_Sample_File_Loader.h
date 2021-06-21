//Line_Sample_File_Loader.h.

#pragma once

#include <string>    
#include <map>
#include <list>

#include <boost/filesystem.hpp>

#include "Structs.h"

bool Load_From_Line_Sample_Files( Drover &DICOM_data,
                                  const std::map<std::string,std::string> &InvocationMetadata,
                                  const std::string &FilenameLex,
                                  std::list<boost::filesystem::path> &Filenames );
