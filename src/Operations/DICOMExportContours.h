// DICOMExportContours.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocDICOMExportContours();

Drover DICOMExportContours(const Drover& DICOM_data,
                           const OperationArgPkg& /*OptArgs*/,
                           const std::map<std::string, std::string>& /*InvocationMetadata*/,
                           const std::string& /*FilenameLex*/,

                           const std::list<OperationArgPkg>& /*Children*/);
