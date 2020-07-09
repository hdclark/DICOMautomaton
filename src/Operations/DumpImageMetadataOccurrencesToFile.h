// DumpImageMetadataOccurrencesToFile.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocDumpImageMetadataOccurrencesToFile();

Drover DumpImageMetadataOccurrencesToFile(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                                          std::map<std::string, std::string> /*InvocationMetadata*/,
                                          std::string /*FilenameLex*/);
