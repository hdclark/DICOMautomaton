// GroupImages.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocGroupImages(void);

Drover GroupImages(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                     std::map<std::string, std::string> /*InvocationMetadata*/,
                     std::string /*FilenameLex*/);
