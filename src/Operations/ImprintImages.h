// ImprintImages.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocImprintImages();

Drover ImprintImages(Drover DICOM_data,
                     const OperationArgPkg& /*OptArgs*/,
                     const std::map<std::string, std::string>& /*InvocationMetadata*/,
                     const std::string& /*FilenameLex*/);
