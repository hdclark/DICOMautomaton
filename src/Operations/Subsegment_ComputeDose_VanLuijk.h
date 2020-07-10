// Subsegment_ComputeDose_VanLuijk.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocSubsegment_ComputeDose_VanLuijk();

Drover Subsegment_ComputeDose_VanLuijk(Drover DICOM_data, const OperationArgPkg& /*OptArgs*/,
                             const std::map<std::string, std::string>& /*InvocationMetadata*/,
                             const std::string& /*FilenameLex*/);
