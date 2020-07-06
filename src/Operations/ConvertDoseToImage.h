// ConvertDoseToImage.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocConvertDoseToImage(void);

Drover
ConvertDoseToImage(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string, std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/);

