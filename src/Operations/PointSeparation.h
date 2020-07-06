// PointSeparation.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocPointSeparation(void);

Drover PointSeparation(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                         std::map<std::string, std::string> /*InvocationMetadata*/,
                         std::string /*FilenameLex*/);
