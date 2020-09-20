// ExtractRadiomicFeatures.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocExtractRadiomicFeatures();

Drover ExtractRadiomicFeatures(Drover DICOM_data,
                               const OperationArgPkg& /*OptArgs*/,
                               const std::map<std::string, std::string>& /*InvocationMetadata*/,
                               const std::string& /*FilenameLex*/);
