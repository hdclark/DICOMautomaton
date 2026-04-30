// SDL_Viewer_Brushes.h - Brush types and painting functions for SDL_Viewer.
//
// A part of DICOMautomaton 2020-2025. Written by hal clark.
//

#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include "YgorImages.h"
#include "YgorMath.h"


// Brush types for 2D and 3D painting operations.
enum class brush_t {
    // 2D brushes.
    rigid_circle,
    rigid_square,
    gaussian_2D,
    tanh_2D,
    median_circle,
    median_square,
    mean_circle,
    mean_square,

    // 3D brushes.
    rigid_sphere,
    rigid_cube,
    gaussian_3D,
    tanh_3D,
    median_sphere,
    median_cube,
    mean_sphere,
    mean_cube,
};


// Apply a brush stroke to a set of images.
//
// The brush is applied along the line segments specified by 'lss'.
// 'radius' controls the size of the brush.
// 'intensity' controls the strength/value of the brush stroke.
// 'channel' specifies which image channel to modify.
// 'intensity_min' and 'intensity_max' clamp the resulting pixel values.
// 'is_additive' controls whether the brush adds to or erases from the image.
void draw_with_brush( const decltype(planar_image_collection<float,double>().get_all_images()) &img_its,
                      const std::vector<line_segment<double>> &lss,
                      brush_t brush,
                      float radius,
                      float intensity,
                      int64_t channel,
                      float intensity_min = std::numeric_limits<float>::infinity() * -1.0,
                      float intensity_max = std::numeric_limits<float>::infinity(),
                      bool is_additive = true);

