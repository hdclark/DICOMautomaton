//ConvenienceRoutines.h.

#pragma once

#include <functional>
#include <string>

#include "YgorImages.h"

namespace Stats {
template <class C> class Running_MinMax;
}  // namespace Stats


void UpdateImageDescription(
        std::reference_wrapper<planar_image<float,double>> img_refw,
        const std::string &Description);

void UpdateImageDescription(
        planar_image_collection<float,double>::images_list_it_t img_it,
        const std::string &Description);


//These routines update the window-level using the provided values.
// They are useful to tune the window-level to a specific sub-region.
void UpdateImageWindowCentreWidth(
        std::reference_wrapper<planar_image<float,double>> img_refw,
        const Stats::Running_MinMax<float> &RMM );

void UpdateImageWindowCentreWidth(
        planar_image_collection<float,double>::images_list_it_t img_it,
        const Stats::Running_MinMax<float> &RMM );


//These routines update the window-level by computing coverage for the whole image.
void UpdateImageWindowCentreWidth(
        std::reference_wrapper<planar_image<float,double>> img_refw);

void UpdateImageWindowCentreWidth(
        planar_image_collection<float,double>::images_list_it_t img_it);

