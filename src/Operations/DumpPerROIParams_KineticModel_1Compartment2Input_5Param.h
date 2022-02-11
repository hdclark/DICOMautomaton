// DumpPerROIParams_KineticModel_1Compartment2Input_5Param.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocDumpPerROIParams_KineticModel_1Compartment2Input_5Param();

bool DumpPerROIParams_KineticModel_1Compartment2Input_5Param(
    Drover &DICOM_data,
    const OperationArgPkg& /*OptArgs*/,
    std::map<std::string, std::string>& /*InvocationMetadata*/,
    const std::string& /*FilenameLex*/);
