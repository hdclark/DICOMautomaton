// PurgeContours.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocPurgeContours();

Drover PurgeContours(Drover DICOM_data, const OperationArgPkg& /*OptArgs*/,
                             const std::map<std::string, std::string>& /*InvocationMetadata*/,
                             const std::string& /*FilenameLex*/);
