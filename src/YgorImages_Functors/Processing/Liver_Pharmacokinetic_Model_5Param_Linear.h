//Liver_Pharmacokinetic_Model_5Param_Linear.h.
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


bool 
LiverPharmacoModel5ParamLinear(planar_image_collection<float,double>::images_list_it_t first_img_it,
                         std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                         std::list<std::reference_wrapper<planar_image_collection<float,double>>> outgoing_maps,
                         std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                         std::experimental::any ud);

