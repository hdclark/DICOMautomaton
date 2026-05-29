// GenerateVirtualDataContourViaThresholdTestV1.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocGenerateVirtualDataContourViaThresholdTestV1();

bool GenerateVirtualDataContourViaThresholdTestV1(Drover &DICOM_data,
                                                    const OperationArgPkg& /*OptArgs*/,
                                                    std::map<std::string, std::string>& /*InvocationMetadata*/,
                                                    const std::string& /*FilenameLex*/);
