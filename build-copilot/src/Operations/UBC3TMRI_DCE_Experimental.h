// UBC3TMRI_DCE_Experimental.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocUBC3TMRI_DCE_Experimental();

bool UBC3TMRI_DCE_Experimental(Drover &DICOM_data,
                                 const OperationArgPkg& /*OptArgs*/,
                                 std::map<std::string, std::string>& /*InvocationMetadata*/,
                                 const std::string& /*FilenameLex*/);
