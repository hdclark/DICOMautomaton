//Highlight_ROI_Voxels.h.
#pragma once


#include <cmath>
#include <experimental/any>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <set>

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"

template <class T> class contour_collection;


typedef enum { // Controls how voxels are computed to be 'within' a contour.
    // Consider only the central-most point of the voxel. (This is typically how voxels are handled.)
    centre,

    // Consider the corners of the 2D pixels corresponding to the intersection of a plane with the voxel.
    // The plane intersects the central-most point of the voxel and is orthogonal to the row and column unit vector.
    planar_corners_inclusive, // Consider 'within' if any corners are interior to the contour.
    planar_corners_exclusive  // Consider 'within' if all corners are interior to the contour.

} HighlightInclusionMethod;

typedef enum { // Controls how contours that overlap are treated.
    // Treat overlapping contours as if they have no effect on one another or jointly-bounded voxels.
    ignore,

    // Overlapping contours with opposite orientation cancel. Note that orientation for non-overlapping contours is
    // still ignored.
    opposite_orientations_cancel,

    // Ignore orientation and consider all regions of contour overlap to cancel one another.
    overlapping_contours_cancel

} ContourOverlapMethod;

struct HighlightROIVoxelsUserData {

    // Algorithmic changes.
    HighlightInclusionMethod inclusivity = HighlightInclusionMethod::centre;
    ContourOverlapMethod overlap = ContourOverlapMethod::ignore;

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





