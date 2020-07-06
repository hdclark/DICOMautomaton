// GiveWholeImageArrayAnAlphaBetaWindowLevel.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocGiveWholeImageArrayAnAlphaBetaWindowLevel(void);

Drover GiveWholeImageArrayAnAlphaBetaWindowLevel(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                                           std::map<std::string, std::string> /*InvocationMetadata*/,
                                           std::string /*FilenameLex*/);
