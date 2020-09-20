// DetectShapes3D.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocDetectShapes3D();

Drover DetectShapes3D(Drover DICOM_data,
                      const OperationArgPkg& /*OptArgs*/,
                      const std::map<std::string, std::string>& /*InvocationMetadata*/,
                      const std::string& /*FilenameLex*/);
