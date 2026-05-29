// ModelIVIM.h.

#pragma once

#include <map>
#include <string>
#include <array>

#include "../Structs.h"


OperationDoc OpArgDocModelIVIM();

bool ModelIVIM(Drover &DICOM_data,
               const OperationArgPkg& /*OptArgs*/,
               std::map<std::string, std::string>& /*InvocationMetadata*/,
               const std::string& /*FilenameLex*/);

