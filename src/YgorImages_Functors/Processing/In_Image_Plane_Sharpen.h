//In_Image_Plane_Sharpen.h.
#pragma once


#include <cmath>
#include <any>
#include <functional>
#include <limits>
#include <list>
#include <map>

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"

template <class T> class contour_collection;


typedef enum { // Controls which estimator is used to approximate the sharpen operator.

    //Fixed-size estimators.
    sharpen_3x3,
    unsharp_mask_5x5

    //Non-fixed (adaptive) estimators.
    // ... currently none ...

} SharpenEstimator;

struct InPlaneImageSharpenUserData {

    SharpenEstimator estimator = SharpenEstimator::unsharp_mask_5x5;

};


bool InPlaneImageSharpen(planar_image_collection<float,double>::images_list_it_t first_img_it,
                      std::list<planar_image_collection<float,double>::images_list_it_t> ,
                      std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                      std::list<std::reference_wrapper<contour_collection<double>>>, 
                      std::any );

