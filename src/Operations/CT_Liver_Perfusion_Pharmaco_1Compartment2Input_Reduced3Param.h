// CT_Liver_Perfusion_Pharmaco_1Compartment2Input_Reduced3Param.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocCT_Liver_Perfusion_Pharmaco_1C2I_Reduced3Param();

Drover CT_Liver_Perfusion_Pharmaco_1C2I_Reduced3Param(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                                   std::map<std::string, std::string> /*InvocationMetadata*/,
                                   std::string /*FilenameLex*/);
