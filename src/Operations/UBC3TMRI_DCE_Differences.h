// UBC3TMRI_DCE_Differences.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocUBC3TMRI_DCE_Differences();

Drover UBC3TMRI_DCE_Differences(Drover DICOM_data, const OperationArgPkg& /*OptArgs*/,
                                const std::map<std::string, std::string>& /*InvocationMetadata*/,
                                const std::string& /*FilenameLex*/);
