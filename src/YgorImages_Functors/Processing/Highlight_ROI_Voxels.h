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


typedef enum { // Controls how voxels are computed to be 'within' a contour.
    // Consider only the central-most point of the voxel. (This is typically how voxels are handled.)
    centre,

    // Consider the corners of the 2D pixels corresponding to the intersection of a plane with the voxel.
    // The plane intersects the central-most point of the voxel and is orthogonal to the row and column unit vector.
    planar_corners_inclusive, // Consider 'within' if any corners are interior to the contour.
    planar_corners_exclusive  // Consider 'within' if all corners are interior to the contour.

} HighlightInclusionMethod;

struct HighlightROIVoxelsUserData {

    // Algorithmic changes.
    HighlightInclusionMethod inclusivity = HighlightInclusionMethod::centre;

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





