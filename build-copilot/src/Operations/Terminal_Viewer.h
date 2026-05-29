// Terminal_Viewer.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"

OperationDoc OpArgDocTerminal_Viewer();

bool Terminal_Viewer(Drover &DICOM_data,
                     const OperationArgPkg& /*OptArgs*/,
                     std::map<std::string, std::string>& /*InvocationMetadata*/,
                     const std::string& /*FilenameLex*/);
