// OrderImages.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocOrderImages(void);

Drover OrderImages(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                     std::map<std::string, std::string> /*InvocationMetadata*/,
                     std::string /*FilenameLex*/);
