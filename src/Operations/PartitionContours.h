// PartitionContours.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocPartitionContours();

Drover PartitionContours(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                          std::map<std::string, std::string> /*InvocationMetadata*/,
                          std::string /*FilenameLex*/);
