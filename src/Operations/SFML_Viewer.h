// SFML_Viewer.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"

OperationDoc OpArgDocSFML_Viewer();

bool SFML_Viewer(Drover &DICOM_data,
                   const OperationArgPkg& /*OptArgs*/,
                   const std::map<std::string, std::string>&
                   /*InvocationMetadata*/,
                   const std::string& /*FilenameLex*/);
