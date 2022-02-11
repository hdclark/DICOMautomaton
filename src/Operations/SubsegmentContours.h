// SubsegmentContours.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocSubsegmentContours();

bool SubsegmentContours(Drover &DICOM_data,
                          const OperationArgPkg& /*OptArgs*/,
                          std::map<std::string, std::string>& /*InvocationMetadata*/,
                          const std::string& /*FilenameLex*/);
