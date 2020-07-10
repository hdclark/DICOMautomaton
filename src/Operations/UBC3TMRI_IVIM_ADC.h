// UBC3TMRI_IVIM_ADC.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocUBC3TMRI_IVIM_ADC();

Drover UBC3TMRI_IVIM_ADC(Drover DICOM_data, const OperationArgPkg& /*OptArgs*/,
                         const std::map<std::string, std::string>& /*InvocationMetadata*/,
                         const std::string& /*FilenameLex*/);
