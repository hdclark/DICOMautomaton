// BoostSerializeDrover.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocBoost_Serialize_Drover();

Drover
Boost_Serialize_Drover(const Drover &DICOM_data, 
                       const OperationArgPkg& /*OptArgs*/,
                       const std::map<std::string, std::string>& /*InvocationMetadata*/,
                       const std::string& /*FilenameLex*/ );
