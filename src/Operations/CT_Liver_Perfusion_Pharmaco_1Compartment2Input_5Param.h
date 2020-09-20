// CT_Liver_Perfusion_Pharmaco_1Compartment2Input_5Param.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocCT_Liver_Perfusion_Pharmaco_1C2I_5Param();

Drover CT_Liver_Perfusion_Pharmaco_1C2I_5Param(Drover DICOM_data,
                                               const OperationArgPkg& /*OptArgs*/,
                                               const std::map<std::string, std::string>& /*InvocationMetadata*/,
                                               const std::string& /*FilenameLex*/);
