//Pixel_Value_Histogram.h.
#pragma once


#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>

#include <any>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"


bool PixelHistogramAnalysis(planar_image_collection<float,double>::images_list_it_t  local_img_it,
                            std::list<std::reference_wrapper<planar_image_collection<float,double>>> ,
                            std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                            std::any );

void DumpPixelHistogramResults();


