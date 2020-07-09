// GenerateVirtualDataContourViaThresholdTestV1.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocGenerateVirtualDataContourViaThresholdTestV1();

Drover GenerateVirtualDataContourViaThresholdTestV1(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                      std::map<std::string, std::string> /*InvocationMetadata*/,
                      std::string /*FilenameLex*/);
