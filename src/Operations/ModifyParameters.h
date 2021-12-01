// ModifyParameters.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocModifyParameters();

bool ModifyParameters(Drover &DICOM_data,
                     const OperationArgPkg& /*OptArgs*/,
                     std::map<std::string, std::string>& /*InvocationMetadata*/,
                     const std::string& /*FilenameLex*/);
