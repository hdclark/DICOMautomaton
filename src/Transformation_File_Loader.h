//Transformation_File_Loader.h.

#pragma once

#include <string>    
#include <map>
#include <list>

#include <filesystem>

#include "Structs.h"


// Read the transformation from a custom file format.
bool
ReadTransform3(Transform3 &t3,
               std::istream &is );

// Write the transformation to a custom file format.
bool
WriteTransform3(const Transform3 &t3,
                std::ostream &os );


bool Load_Transforms_From_Files( Drover &DICOM_data,
                                 std::map<std::string,std::string> &InvocationMetadata,
                                 const std::string &FilenameLex,
                                 std::list<std::filesystem::path> &Filenames );
