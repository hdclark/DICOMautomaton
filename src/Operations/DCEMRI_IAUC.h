// DCEMRI_IAUC.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocDCEMRI_IAUC();

Drover DCEMRI_IAUC(Drover DICOM_data,
                   const OperationArgPkg& /*OptArgs*/,
                   const std::map<std::string, std::string>&
                   /*InvocationMetadata*/,
                   const std::string& /*FilenameLex*/);
