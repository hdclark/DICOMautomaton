//Volumetric_Spatial_Blur.h.
#pragma once

#include <cmath>
#include <any>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"

template <class T, class R> class planar_image_collection;
template <class T> class contour_collection;

typedef enum { // Controls which blur is computed.

    Gaussian // Numerically-approximated Gaussian with fixed (3-sigma) extent.

} VolumetricSpatialBlurEstimator;

struct ComputeVolumetricSpatialBlurUserData {

    VolumetricSpatialBlurEstimator estimator = VolumetricSpatialBlurEstimator::Gaussian;

    // The channel to analyze. If negative, all channels are analyzed.
    long int channel = -1;

};

bool ComputeVolumetricSpatialBlur(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::any ud );

