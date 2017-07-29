//Highlight_ROI_Voxels.h.
#pragma once


#include <list>
#include <set>
#include <functional>
#include <limits>
#include <map>
#include <cmath>

#include <experimental/any>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"

struct HighlightROIVoxelsUserData {

    // Actions.
    bool overwrite_interior = true; // Whether to alter voxels within the specific ROI(s).
    bool overwrite_exterior = true; // Whether to alter voxels not within the specific ROI(s).

    // Output values.
    float outgoing_interior_val = 1.0f; // New value for voxels within the specific ROI(s).
    float outgoing_exterior_val = 0.0f; // New value for voxels not within the specified ROI(s).

    long int channel = -1; // The image channel to use. Zero-based. Use -1 for all channels. 
};


bool HighlightROIVoxels(planar_image_collection<float,double>::images_list_it_t first_img_it,
                        std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                        std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                        std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                        std::experimental::any );





