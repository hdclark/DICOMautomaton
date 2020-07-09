// DetectShapes3D.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocDetectShapes3D();

Drover DetectShapes3D(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                                   std::map<std::string, std::string> /*InvocationMetadata*/,
                                   std::string /*FilenameLex*/);
