// UBC3TMRI_DCE.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocUBC3TMRI_DCE();

Drover UBC3TMRI_DCE(Drover DICOM_data,
                    const OperationArgPkg& /*OptArgs*/,
                    const std::map<std::string, std::string>&
                    /*InvocationMetadata*/,
                    const std::string& /*FilenameLex*/);
