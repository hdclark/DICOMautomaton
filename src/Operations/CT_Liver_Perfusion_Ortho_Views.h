// CT_Liver_Perfusion_Ortho_Views.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocCT_Liver_Perfusion_Ortho_Views();

bool CT_Liver_Perfusion_Ortho_Views(Drover &DICOM_data,
                                      const OperationArgPkg& /*OptArgs*/,
                                      const std::map<std::string, std::string>& /*InvocationMetadata*/,
                                      const std::string& /*FilenameLex*/);
