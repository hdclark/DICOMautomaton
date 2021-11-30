// DetectShapes3D.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocDetectShapes3D();

bool DetectShapes3D(Drover &DICOM_data,
                      const OperationArgPkg& /*OptArgs*/,
                      std::map<std::string, std::string>& /*InvocationMetadata*/,
                      const std::string& /*FilenameLex*/);
