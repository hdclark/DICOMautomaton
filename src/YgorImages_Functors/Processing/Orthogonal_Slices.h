//Orthogonal_Slices.h.
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


bool OrthogonalSlices(planar_image_collection<float,double>::images_list_it_t first_img_it,
                      std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                      std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                      std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                      std::any );

