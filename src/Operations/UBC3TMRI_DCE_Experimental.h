// UBC3TMRI_DCE_Experimental.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocUBC3TMRI_DCE_Experimental(void);

Drover UBC3TMRI_DCE_Experimental(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                                 std::map<std::string, std::string> /*InvocationMetadata*/,
                                 std::string /*FilenameLex*/);
