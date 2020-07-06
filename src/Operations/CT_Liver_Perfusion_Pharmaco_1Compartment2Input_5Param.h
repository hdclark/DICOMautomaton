// CT_Liver_Perfusion_Pharmaco_1Compartment2Input_5Param.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocCT_Liver_Perfusion_Pharmaco_1C2I_5Param(void);

Drover CT_Liver_Perfusion_Pharmaco_1C2I_5Param(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                                   std::map<std::string, std::string> /*InvocationMetadata*/,
                                   std::string /*FilenameLex*/);
