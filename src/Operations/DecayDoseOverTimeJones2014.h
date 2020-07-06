// DecayDoseOverTimeJones2014.h.

#pragma once

#include <string>
#include <map>

#include "../Structs.h"


OperationDoc OpArgDocDecayDoseOverTimeJones2014(void);

Drover DecayDoseOverTimeJones2014(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                             std::map<std::string, std::string> /*InvocationMetadata*/,
                             std::string /*FilenameLex*/);
