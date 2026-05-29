//RegisterImagesDemons.h

#pragma once

#include <map>
#include <string>

#include "../Structs.h"
#include "../Regex_Selectors.h"

#include "Explicator.h"

OperationDoc OpArgDocRegisterImagesDemons();

bool RegisterImagesDemons(Drover &DICOM_data,
                          const OperationArgPkg& /*OptArgs*/,
                          std::map<std::string, std::string>& /*InvocationMetadata*/,
                          const std::string& /*FilenameLex*/);

