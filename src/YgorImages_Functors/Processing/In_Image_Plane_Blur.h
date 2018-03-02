//In_Image_Plane_Blur.h.
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


typedef enum { // Controls which estimator is used to approximate the blur operator.

    //Fixed-size estimators.
    box_3x3,
    box_5x5,

    gaussian_3x3,
    gaussian_5x5,

    //Non-fixed (adaptive) estimators.
    gaussian_open

} BlurEstimator;

struct InPlaneImageBlurUserData {

    BlurEstimator estimator = BlurEstimator::gaussian_open;

    //Parameters for non-fixed estimators.
    double gaussian_sigma = 1.5; // sigma in pixel coordinates.

};


bool InPlaneImageBlur(planar_image_collection<float,double>::images_list_it_t first_img_it,
                      std::list<planar_image_collection<float,double>::images_list_it_t> ,
                      std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                      std::list<std::reference_wrapper<contour_collection<double>>>, 
                      std::experimental::any );

