// PreFilterEnormousCTValues.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocPreFilterEnormousCTValues();

Drover PreFilterEnormousCTValues(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                                 std::map<std::string, std::string> /*InvocationMetadata*/,
                                 std::string /*FilenameLex*/);
