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


typedef enum { // Controls how image derivatives are computed.
    first,
    second
} PartialDerivativeOrder;

typedef enum { // Controls how image derivatives are computed.
    row_aligned,
    column_aligned,
    cross

} PartialDerivativeMethod;

struct ImagePartialDerivativeUserData {

    // The default is second-order 'cross' to be more symmetric.
    PartialDerivativeOrder order = PartialDerivativeOrder::second;
    PartialDerivativeMethod method = PartialDerivativeMethod::cross;

};


bool ImagePartialDerivative(planar_image_collection<float,double>::images_list_it_t first_img_it,
                           std::list<planar_image_collection<float,double>::images_list_it_t> ,
                           std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                           std::list<std::reference_wrapper<contour_collection<double>>>, 
                           std::experimental::any );

