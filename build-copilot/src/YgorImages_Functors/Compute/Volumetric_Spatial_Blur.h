//Volumetric_Spatial_Blur.h.
#pragma once

#include <any>
#include <functional>
#include <list>
#include <cstdint>


template <class T, class R> class planar_image_collection;
template <class T> class contour_collection;

typedef enum { // Controls which blur is computed.

    Gaussian // Numerically-approximated Gaussian with fixed (3-sigma) extent.

} VolumetricSpatialBlurEstimator;

struct ComputeVolumetricSpatialBlurUserData {

    VolumetricSpatialBlurEstimator estimator = VolumetricSpatialBlurEstimator::Gaussian;

    // The channel to analyze. If negative, all channels are analyzed.
    int64_t channel = -1;

};

bool ComputeVolumetricSpatialBlur(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::any ud );

