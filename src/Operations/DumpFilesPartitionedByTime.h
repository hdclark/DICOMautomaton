// DumpFilesPartitionedByTime.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocDumpFilesPartitionedByTime();

Drover DumpFilesPartitionedByTime(Drover DICOM_data, const OperationArgPkg& /*OptArgs*/,
                                  const std::map<std::string, std::string>& /*InvocationMetadata*/,
                                  const std::string& /*FilenameLex*/);
