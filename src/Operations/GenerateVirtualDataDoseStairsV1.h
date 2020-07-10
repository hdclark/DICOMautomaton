// GenerateVirtualDataDoseStairsV1.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocGenerateVirtualDataDoseStairsV1();

Drover GenerateVirtualDataDoseStairsV1(Drover DICOM_data, const OperationArgPkg& /*OptArgs*/,
                      const std::map<std::string, std::string>& /*InvocationMetadata*/,
                      const std::string& /*FilenameLex*/);
