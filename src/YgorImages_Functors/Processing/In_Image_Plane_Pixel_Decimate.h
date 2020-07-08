//In_Image_Plane_Pixel_Decimate.h.
#pragma once


#include <any>
#include <functional>
#include <list>

#include "YgorImages.h"

template <class T> class contour_collection;


bool InImagePlanePixelDecimate(planar_image_collection<float,double>::images_list_it_t first_img_it,
                               std::list<planar_image_collection<float,double>::images_list_it_t> ,
                               std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                               std::list<std::reference_wrapper<contour_collection<double>>>, 
                               long int ScaleFactorRows, long int ScaleFactorColumns,
                               std::any );

