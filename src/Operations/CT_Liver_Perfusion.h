// CT_Liver_Perfusion.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocCT_Liver_Perfusion(void);

Drover CT_Liver_Perfusion(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                          std::map<std::string, std::string> /*InvocationMetadata*/,
                          std::string /*FilenameLex*/);
