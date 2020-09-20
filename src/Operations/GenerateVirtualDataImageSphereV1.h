// GenerateVirtualDataImageSphereV1.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocGenerateVirtualDataImageSphereV1();

Drover GenerateVirtualDataImageSphereV1(Drover DICOM_data,
                                        const OperationArgPkg& /*OptArgs*/,
                                        const std::map<std::string, std::string>& /*InvocationMetadata*/,
                                        const std::string& /*FilenameLex*/);
