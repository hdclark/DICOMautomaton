//STB_Shim.h.

#pragma once

#include <string>    

#include "YgorImages.h"

planar_image_collection<float, double>
ReadImageUsingSTB(const std::string &fname);

