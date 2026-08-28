// MaskContours.h - A part of DICOMautomaton 2026. Written by hal clark and OpenAI.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"

OperationDoc OpArgDocMaskContours();

bool MaskContours(Drover &DICOM_data,
                  const OperationArgPkg& OptArgs,
                  std::map<std::string, std::string>& InvocationMetadata,
                  const std::string& FilenameLex);
