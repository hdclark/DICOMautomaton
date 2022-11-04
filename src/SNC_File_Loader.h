//SNC_File_Loader.h.

#pragma once

#include <string>    
#include <map>
#include <list>

#include <filesystem>

#include "Structs.h"

bool read_snc_file( std::istream &is, planar_image_collection<float,double> &imgs );

bool write_snc_file( std::ostream &os, const planar_image<float,double> &img );

bool Load_From_SNC_Files( Drover &DICOM_data,
                          std::map<std::string,std::string> &InvocationMetadata,
                          const std::string &FilenameLex,
                          std::list<std::filesystem::path> &Filenames );
