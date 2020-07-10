// DumpPerROIParams_KineticModel_1Compartment2Input_5Param.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocDumpPerROIParams_KineticModel_1Compartment2Input_5Param();

Drover DumpPerROIParams_KineticModel_1Compartment2Input_5Param(Drover DICOM_data, const OperationArgPkg& /*OptArgs*/,
                             const std::map<std::string, std::string>& /*InvocationMetadata*/,
                             const std::string& /*FilenameLex*/);
