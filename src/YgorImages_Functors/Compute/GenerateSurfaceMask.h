//GenerateSurfaceMask.h.
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


struct GenerateSurfaceMaskUserData {
    float background_val = 0.0;
    float surface_val    = 1.0;
    float interior_val   = 2.0;

//    bool assume_boundary_is_surface = false; //If the ROI overshoots an image boundary, assume the boundary is the 
//    long int voxel_neighbour_family = 1; 
};

bool ComputeGenerateSurfaceMask(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                          std::experimental::any ud );

