// BoostSerializeDrover.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


std::list<OperationArgDoc> OpArgDocBoost_Serialize_Drover(void);

Drover
Boost_Serialize_Drover(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string, std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/);
