// DumpPixelValuesOverTimeForAnEncompassedPoint.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocDumpPixelValuesOverTimeForAnEncompassedPoint();

Drover DumpPixelValuesOverTimeForAnEncompassedPoint(Drover DICOM_data, const OperationArgPkg& /*OptArgs*/,
                                                    const std::map<std::string, std::string>& /*InvocationMetadata*/,
                                                    const std::string& /*FilenameLex*/);
