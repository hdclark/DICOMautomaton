//Volumetric_Spatial_Derivative.h.
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

typedef enum { // Controls which image derivatives are computed.

    // Centered first-order finite-difference derivatives.
    first, //Simple cartesian-aligned.

    Sobel_3x3x3

} VolumetricSpatialDerivativeEstimator;

typedef enum { // Controls how image derivatives are computed and combined.
    row_aligned,    //Where applicable.
    column_aligned, //Where applicable.
    image_aligned,  //Where applicable.

    magnitude,  //Magnitude of the gradient vector.

    non_maximum_suppression //Edge-thinning technique to erode thick edges.

} VolumetricSpatialDerivativeMethod;

struct ComputeVolumetricSpatialDerivativeUserData {

    // The default should be symmetric.
    VolumetricSpatialDerivativeEstimator order = VolumetricSpatialDerivativeEstimator::first;
    VolumetricSpatialDerivativeMethod method = VolumetricSpatialDerivativeMethod::magnitude;

    // The channel to analyze. If negative, all channels are analyzed.
    long int channel = -1;

};

bool ComputeVolumetricSpatialDerivative(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::any ud );

