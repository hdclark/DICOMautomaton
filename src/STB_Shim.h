//STB_Shim.h.

#pragma once

#include <string>
#include <vector>

#include "YgorImages.h"

planar_image_collection<float, double>
ReadImageUsingSTB(const std::string &fname);


planar_image_collection<float, double>
ReadImageUsingSTB(const std::vector<uint8_t> &blob);

