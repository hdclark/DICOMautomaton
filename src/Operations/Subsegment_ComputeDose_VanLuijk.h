// Subsegment_ComputeDose_VanLuijk.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocSubsegment_ComputeDose_VanLuijk();

bool Subsegment_ComputeDose_VanLuijk(Drover &DICOM_data,
                                       const OperationArgPkg& /*OptArgs*/,
                                       std::map<std::string, std::string>& /*InvocationMetadata*/,
                                       const std::string& /*FilenameLex*/);
