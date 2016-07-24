//Liver_Kinetic_1Compartment2Input_5Param_LinearInterp_LevenbergMarquardt.h.
#pragma once

#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <tuple>
#include <regex>

#include <experimental/any>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"


bool 
KineticModel_Liver_1C2I_5Param_LinearInterp(planar_image_collection<float,double>::images_list_it_t first_img_it,
                         std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                         std::list<std::reference_wrapper<planar_image_collection<float,double>>> outgoing_maps,
                         std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                         std::experimental::any ud);

