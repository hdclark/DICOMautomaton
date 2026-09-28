//Volumetric_Spatial_Blur.h.
#pragma once

#include <any>
#include <functional>
#include <list>
#include <set>
#include <cstdint>


template <class T, class R> class planar_image_collection;
template <class T> class contour_collection;

typedef enum { // Controls which blur is computed.

    Gaussian // Numerically-approximated Gaussian with fixed (3-sigma) extent.

} VolumetricSpatialBlurEstimator;

struct ComputeVolumetricSpatialBlurUserData {

    VolumetricSpatialBlurEstimator estimator = VolumetricSpatialBlurEstimator::Gaussian;

    // The channels to analyze. Negative values or an empty set will select all channels.
    std::set<int64_t> channels;

};

bool ComputeVolumetricSpatialBlur(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::any ud );

