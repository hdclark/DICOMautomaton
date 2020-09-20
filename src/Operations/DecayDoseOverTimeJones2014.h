// DecayDoseOverTimeJones2014.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocDecayDoseOverTimeJones2014();

Drover DecayDoseOverTimeJones2014(Drover DICOM_data,
                                  const OperationArgPkg& /*OptArgs*/,
                                  const std::map<std::string, std::string>& /*InvocationMetadata*/,
                                  const std::string& /*FilenameLex*/);
