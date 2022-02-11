// UBC3TMRI_DCE_Differences.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocUBC3TMRI_DCE_Differences();

bool UBC3TMRI_DCE_Differences(Drover &DICOM_data,
                                const OperationArgPkg& /*OptArgs*/,
                                std::map<std::string, std::string>& /*InvocationMetadata*/,
                                const std::string& /*FilenameLex*/);
