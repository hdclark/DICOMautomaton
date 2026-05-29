// ExportPointClouds.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocExportPointClouds();

bool ExportPointClouds(Drover &DICOM_data,
                         const OperationArgPkg& /*OptArgs*/,
                         std::map<std::string, std::string>& /*InvocationMetadata*/,
                         const std::string& /*FilenameLex*/);
