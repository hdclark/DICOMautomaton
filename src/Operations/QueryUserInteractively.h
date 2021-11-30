// QueryUserInteractively.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocQueryUserInteractively();

bool QueryUserInteractively(Drover &DICOM_data,
                            const OperationArgPkg& /*OptArgs*/,
                            std::map<std::string, std::string>& /*InvocationMetadata*/,
                            const std::string& /*FilenameLex*/);
