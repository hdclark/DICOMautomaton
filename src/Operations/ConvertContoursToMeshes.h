// ConvertContoursToMeshes.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocConvertContoursToMeshes();

bool ConvertContoursToMeshes(Drover &DICOM_data,
                               const OperationArgPkg& /*OptArgs*/,
                               std::map<std::string, std::string>& /*InvocationMetadata*/,
                               const std::string& /*FilenameLex*/);
