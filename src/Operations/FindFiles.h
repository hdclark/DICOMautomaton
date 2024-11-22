// FindFiles.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocFindFiles();

bool FindFiles(Drover &DICOM_data,
               const OperationArgPkg& /*OptArgs*/,
               std::map<std::string, std::string>& /*InvocationMetadata*/,
               const std::string& /*FilenameLex*/);
