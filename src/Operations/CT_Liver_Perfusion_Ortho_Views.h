// CT_Liver_Perfusion_Ortho_Views.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocCT_Liver_Perfusion_Ortho_Views(void);

Drover CT_Liver_Perfusion_Ortho_Views(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                                      std::map<std::string, std::string> /*InvocationMetadata*/,
                                      std::string /*FilenameLex*/);
