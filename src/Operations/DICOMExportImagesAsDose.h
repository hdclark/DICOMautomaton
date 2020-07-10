// DICOMExportImagesAsDose.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocDICOMExportImagesAsDose();

Drover
DICOMExportImagesAsDose(Drover DICOM_data, 
                        const OperationArgPkg& /*OptArgs*/,
                        const std::map<std::string, std::string>& /*InvocationMetadata*/,
                        const std::string& /*FilenameLex*/);
