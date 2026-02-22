//STB_Shim.h.

#pragma once

#include <string>
#include <vector>

#include "YgorImages.h"

planar_image_collection<float, double>
ReadImageUsingSTB(const std::string &fname);


planar_image_collection<float, double>
ReadImageUsingSTB(const std::vector<uint8_t> &blob);


// Write a planar_image to a PNG file. Returns true on success, false on failure.
// The image pixel values are clamped to [0, 255] and converted to 8-bit unsigned.
//
// TODO: support image metadata embedded into the PNG.
bool
WritePNGImageUsingSTB(const planar_image<float, double> &img,
                      const std::string &fname);


// Write a planar_image to a PNG blob in memory. Returns true on success, false on failure.
// The image pixel values are clamped to [0, 255] and converted to 8-bit unsigned.
//
// TODO: support image metadata embedded into the PNG.
bool
WritePNGImageUsingSTB(const planar_image<float, double> &img,
                      std::vector<uint8_t> &out_blob);
