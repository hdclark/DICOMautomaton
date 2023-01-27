//DCEMRI_T1_Map_v2.h.
#pragma once


#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>

#include <any>

#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorMath.h"
#include "YgorImages.h"


bool DCEMRIT1MapV2(planar_image_collection<float,double>::images_list_it_t  local_img_it,
                   std::list<std::reference_wrapper<planar_image_collection<float,double>>> external_imgs,
                   std::list<std::reference_wrapper<contour_collection<double>>>, 
                   std::any );




