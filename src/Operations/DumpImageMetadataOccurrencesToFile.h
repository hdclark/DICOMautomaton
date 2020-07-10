// DumpImageMetadataOccurrencesToFile.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocDumpImageMetadataOccurrencesToFile();

Drover DumpImageMetadataOccurrencesToFile(Drover DICOM_data, const OperationArgPkg& /*OptArgs*/,
                                          const std::map<std::string, std::string>& /*InvocationMetadata*/,
                                          const std::string& /*FilenameLex*/);
