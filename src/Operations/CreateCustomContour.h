// CreateCustomContour.h.

#pragma once

#include <list>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocCreateCustomContour();

bool CreateCustomContour(Drover &DICOM_data,
                    const OperationArgPkg& /*OptArgs*/,
                    std::map<std::string, std::string>& /*InvocationMetadata*/,
                    const std::string& FilenameLex);

std::vector<std::string> SplitString(std::string input, std::string delimiter);
