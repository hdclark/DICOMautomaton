// DecimatePixels.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocDecimatePixels(void);

Drover DecimatePixels(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                      std::map<std::string, std::string> /*InvocationMetadata*/,
                      std::string /*FilenameLex*/);
