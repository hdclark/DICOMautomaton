// DumpROISNR.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocDumpROISNR();

bool DumpROISNR(Drover &DICOM_data,
                  const OperationArgPkg& /*OptArgs*/,
                  std::map<std::string, std::string>& /*InvocationMetadata*/,
                  const std::string& /*FilenameLex*/);
