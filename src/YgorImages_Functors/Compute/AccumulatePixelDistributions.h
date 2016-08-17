//AccumulatePixelDistributions.h.
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


struct AccumulatePixelDistributionsUserData {
    std::map<std::string, std::vector<double>> accumulated_voxels; // key: RawROIName.
};

bool AccumulatePixelDistributions(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::experimental::any ud );

