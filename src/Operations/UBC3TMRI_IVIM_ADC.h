// UBC3TMRI_IVIM_ADC.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocUBC3TMRI_IVIM_ADC();

bool UBC3TMRI_IVIM_ADC(Drover &DICOM_data,
                         const OperationArgPkg& /*OptArgs*/,
                         std::map<std::string, std::string>& /*InvocationMetadata*/,
                         const std::string& /*FilenameLex*/);
