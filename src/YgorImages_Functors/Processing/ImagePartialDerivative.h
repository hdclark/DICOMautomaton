//ImagePartialDerivative.h.
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


typedef enum { // Controls which image derivatives are computed.

    // Centered first-order finite-difference derivatives.
    first, //Simple cartesian-aligned.

    Roberts_cross,
    Prewitt,
    Sobel,   
    Scharr,  //Approximately rotationally-symmetric.

    // Centered second-order finite-difference derivatives.
    second

} PartialDerivativeOrder;

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

    // The default is second-order 'cross' to be more symmetric.
    PartialDerivativeOrder order = PartialDerivativeOrder::Scharr;
    PartialDerivativeMethod method = PartialDerivativeMethod::magnitude;

};


bool ImagePartialDerivative(planar_image_collection<float,double>::images_list_it_t first_img_it,
                           std::list<planar_image_collection<float,double>::images_list_it_t> ,
                           std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                           std::list<std::reference_wrapper<contour_collection<double>>>, 
                           std::experimental::any );

