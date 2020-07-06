// BuildLexiconInteractively.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocBuildLexiconInteractively(void);

Drover
BuildLexiconInteractively(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string, std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/);


