// DecayDoseOverTimeJones2014.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocDecayDoseOverTimeJones2014();

bool DecayDoseOverTimeJones2014(Drover &DICOM_data,
                                  const OperationArgPkg& /*OptArgs*/,
                                  std::map<std::string, std::string>& /*InvocationMetadata*/,
                                  const std::string& /*FilenameLex*/);
