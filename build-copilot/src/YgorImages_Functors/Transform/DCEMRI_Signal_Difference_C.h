//DCEMRI_Signal_Difference_C.h.
#pragma once


#include <list>
#include <functional>

#include <any>

#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorMath.h"
#include "YgorImages.h"


bool DCEMRISigDiffC( planar_image_collection<float,double>::images_list_it_t  local_img_it,
                     std::list<std::reference_wrapper<planar_image_collection<float,double>>> external_imgs,
                     std::list<std::reference_wrapper<contour_collection<double>>>, 
                     std::any );



