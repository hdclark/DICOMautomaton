// DumpPixelValuesOverTimeForAnEncompassedPoint.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocDumpPixelValuesOverTimeForAnEncompassedPoint();

Drover DumpPixelValuesOverTimeForAnEncompassedPoint(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                                                    std::map<std::string, std::string> /*InvocationMetadata*/,
                                                    std::string /*FilenameLex*/);
