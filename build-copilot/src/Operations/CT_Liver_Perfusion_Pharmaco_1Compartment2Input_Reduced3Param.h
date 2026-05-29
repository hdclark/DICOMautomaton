// CT_Liver_Perfusion_Pharmaco_1Compartment2Input_Reduced3Param.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocCT_Liver_Perfusion_Pharmaco_1C2I_Reduced3Param();

bool CT_Liver_Perfusion_Pharmaco_1C2I_Reduced3Param(Drover &DICOM_data,
                                                      const OperationArgPkg& /*OptArgs*/,
                                                      std::map<std::string, std::string>& /*InvocationMetadata*/,
                                                      const std::string& /*FilenameLex*/);
