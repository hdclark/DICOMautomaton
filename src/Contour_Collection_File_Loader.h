//Contour_Collection_File_Loader.h.

#pragma once

#include <string>    
#include <map>
#include <list>

#include <fstream>
#include <filesystem>

#include "YgorMath.h"

#include "Structs.h"

bool
Write_Contour_Collections( const std::list<std::reference_wrapper<contour_collection<double>>> &,
                           std::ostream & );

std::list<contour_collection<double>>
Read_Contour_Collections( std::istream & );


bool Load_From_Contour_Collection_Files( Drover &DICOM_data,
                                         std::map<std::string,std::string> &InvocationMetadata,
                                         const std::string &FilenameLex,
                                         std::list<std::filesystem::path> &Filenames );
