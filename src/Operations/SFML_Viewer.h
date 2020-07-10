// SFML_Viewer.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"

OperationDoc OpArgDocSFML_Viewer();

Drover
SFML_Viewer(Drover DICOM_data, const OperationArgPkg& /*OptArgs*/, const std::map<std::string, std::string>& /*InvocationMetadata*/, const std::string& /*FilenameLex*/);
