//GenerateSyntheticDataPointCloudV1.h - A part of DICOMautomaton 2026. Written by hal clark.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocGenerateSyntheticDataPointCloudV1();

bool GenerateSyntheticDataPointCloudV1(Drover &DICOM_data,
                                        const OperationArgPkg& /*OptArgs*/,
                                        std::map<std::string, std::string>& /*InvocationMetadata*/,
                                        const std::string& /*FilenameLex*/);
