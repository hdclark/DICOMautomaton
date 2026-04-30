// SDL_Viewer_Utils.h - Non-OpenGL utility functions for SDL_Viewer.
//
// A part of DICOMautomaton 2020-2025. Written by hal clark.
//
// This file contains utility functions that do not require OpenGL or graphical context.
//

#pragma once

#include <cstdint>
#include <tuple>
#include <vector>

#include "YgorImages.h"
#include "YgorMath.h"


// Compute an axis-aligned bounding box in pixel coordinates.
//
// Given a set of points in world coordinates, this function computes the 
// pixel-space bounding box that encompasses all points, plus extra_space
// padding in all directions.
//
// Returns: (row_min, row_max, col_min, col_max) tuple, all values clamped
// to valid image coordinates.
std::tuple<int64_t, int64_t, int64_t, int64_t>
get_pixelspace_axis_aligned_bounding_box(const planar_image<float, double> &img,
                                         const std::vector<vec3<double>> &points,
                                         double extra_space);

