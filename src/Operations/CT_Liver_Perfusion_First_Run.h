// CT_Liver_Perfusion_First_Run.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocCT_Liver_Perfusion_First_Run();

bool CT_Liver_Perfusion_First_Run(Drover &DICOM_data,
                                    const OperationArgPkg& /*OptArgs*/,
                                    std::map<std::string, std::string>& /*InvocationMetadata*/,
                                    const std::string& /*FilenameLex*/);
