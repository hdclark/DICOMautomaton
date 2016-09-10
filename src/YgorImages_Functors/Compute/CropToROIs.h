//CropToROIs.h.
#pragma once

#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>

#include <experimental/any>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"


struct CropToROIsUserData {

    //The amount of space (in DICOM coordinate system) to leave surrounding the ROI(s).
    //
    // NOTE: Negative margin is allowed.
    //
    double row_margin = 0.5; // Along row_unit.
    double col_margin = 0.5; // Along col_unit.
    double ort_margin = 0.5; // Along direction normal to image plane.

};

bool ComputeCropToROIs(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                          std::experimental::any ud );

