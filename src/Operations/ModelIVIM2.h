// ModelIVIM2.h.

#pragma once

#include <map>
#include <string>
#include <array>

#include "../Structs.h"


OperationDoc OpArgDocModelIVIM2();

bool ModelIVIM2(Drover &DICOM_data,
                const OperationArgPkg& /*OptArgs*/,
                std::map<std::string, std::string>& /*InvocationMetadata*/,
                const std::string& /*FilenameLex*/);

