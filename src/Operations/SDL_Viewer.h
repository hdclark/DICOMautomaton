// SDL_Viewer.h.

#pragma once

#include <array>
#include <map>
#include <string>

#include "../Structs.h"

OperationDoc OpArgDocSDL_Viewer();

std::map<std::string, std::array<float, 4>> ParseSDLViewerKeywordColours(const std::string &);

bool SDL_Viewer(Drover &DICOM_data,
                  const OperationArgPkg& /*OptArgs*/,
                  std::map<std::string, std::string>& /*InvocationMetadata*/,
                  const std::string& /*FilenameLex*/);
