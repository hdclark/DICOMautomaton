// ConvertMeshesToContours.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocConvertMeshesToContours();

Drover ConvertMeshesToContours(Drover DICOM_data,
                               const OperationArgPkg& /*OptArgs*/,
                               const std::map<std::string, std::string>& /*InvocationMetadata*/,
                               const std::string& /*FilenameLex*/);
