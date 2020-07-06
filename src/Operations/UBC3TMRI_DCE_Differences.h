// UBC3TMRI_DCE_Differences.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocUBC3TMRI_DCE_Differences(void);

Drover UBC3TMRI_DCE_Differences(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                                std::map<std::string, std::string> /*InvocationMetadata*/,
                                std::string /*FilenameLex*/);
