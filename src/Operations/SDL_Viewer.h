// SDL_Viewer.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"

OperationDoc OpArgDocSDL_Viewer();

Drover SDL_Viewer(Drover DICOM_data,
                  const OperationArgPkg& /*OptArgs*/,
                  const std::map<std::string, std::string>&
                  /*InvocationMetadata*/,
                  const std::string& /*FilenameLex*/);
