// CT_Liver_Perfusion.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocCT_Liver_Perfusion();

Drover CT_Liver_Perfusion(Drover DICOM_data, const OperationArgPkg& /*OptArgs*/,
                          const std::map<std::string, std::string>& /*InvocationMetadata*/,
                          const std::string& /*FilenameLex*/);
