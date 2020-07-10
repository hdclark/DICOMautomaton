// DCEMRI_Nonparametric_CE.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocDCEMRI_Nonparametric_CE();

Drover
DCEMRI_Nonparametric_CE(Drover DICOM_data, const OperationArgPkg& /*OptArgs*/, const std::map<std::string, std::string>& /*InvocationMetadata*/, const std::string& /*FilenameLex*/);
