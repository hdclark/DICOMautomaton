// BoostSerializeDrover.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"

Drover
Boost_Serialize_Drover(Drover DICOM_data, std::map<std::string, std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/);
