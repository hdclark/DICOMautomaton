//ImagePartialDerivative.h.
#pragma once


#include <cmath>
#include <experimental/any>
#include <functional>
#include <limits>
#include <list>
#include <map>

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"

template <class T> class contour_collection;


typedef enum { // Controls which image derivatives are computed.

    // Centered first-order finite-difference derivatives.
    first, //Simple cartesian-aligned.

    Roberts_cross_3x3,

    Prewitt_3x3,

    Sobel_3x3,
    Sobel_5x5,

    Scharr_3x3,  //Approximately rotationally-symmetric.
    Scharr_5x5,  //Approximately rotationally-symmetric.

    // Centered second-order finite-difference derivatives.
    second

} PartialDerivativeEstimator;

typedef enum { // Controls how image derivatives are computed and combined.
    row_aligned,    //Where applicable.
    column_aligned, //Where applicable.

    prow_pcol_aligned, //Where applicable.
    nrow_pcol_aligned, //Where applicable.

    magnitude,  //Magnitude of the gradient vector.
    orientation, //Orientation of the gradient vector.

    cross //Applicable for higher-order (compound) derivatives.

} PartialDerivativeMethod;

struct ImagePartialDerivativeUserData {

    // The default should be symmetric.
    PartialDerivativeEstimator order = PartialDerivativeEstimator::Scharr_3x3;
    PartialDerivativeMethod method = PartialDerivativeMethod::magnitude;

};


bool ImagePartialDerivative(planar_image_collection<float,double>::images_list_it_t first_img_it,
                           std::list<planar_image_collection<float,double>::images_list_it_t> ,
                           std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                           std::list<std::reference_wrapper<contour_collection<double>>>, 
                           std::experimental::any );

