// CT_Liver_Perfusion_First_Run.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocCT_Liver_Perfusion_First_Run();

Drover CT_Liver_Perfusion_First_Run(Drover DICOM_data, const OperationArgPkg& /*OptArgs*/,
                                    const std::map<std::string, std::string>& /*InvocationMetadata*/,
                                    const std::string& /*FilenameLex*/);
