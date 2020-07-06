// SFML_Viewer.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"

OperationDoc OpArgDocSFML_Viewer(void);

Drover
SFML_Viewer(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string, std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/);
