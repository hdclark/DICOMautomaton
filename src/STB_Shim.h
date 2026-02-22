//STB_Shim.h.

#pragma once

#include <string>
#include <vector>

#include "YgorImages.h"

planar_image_collection<float, double>
ReadImageUsingSTB(const std::string &fname);


planar_image_collection<float, double>
ReadImageUsingSTB(const std::vector<uint8_t> &blob);


// Write image data to a PNG file. Returns true on success, false on failure.
bool
WriteImageUsingSTB(const std::string &fname,
                   int width,
                   int height,
                   int channels,
                   const unsigned char *pixels);


// Write image data to a PNG blob in memory. Returns true on success, false on failure.
bool
WriteImageUsingSTB(std::vector<uint8_t> &out_blob,
                   int width,
                   int height,
                   int channels,
                   const unsigned char *pixels);

