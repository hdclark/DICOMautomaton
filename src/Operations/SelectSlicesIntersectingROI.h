// SelectSlicesIntersectingROI.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocSelectSlicesIntersectingROI();

bool SelectSlicesIntersectingROI(Drover &DICOM_data,
                                   const OperationArgPkg& /*OptArgs*/,
                                   const std::map<std::string, std::string>& /*InvocationMetadata*/,
                                   const std::string& /*FilenameLex*/);
