//AccumulatePixelDistributions.h.
#pragma once

#include <cmath>
#include <experimental/any>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"

template <class T, class R> class planar_image_collection;
template <class T> class contour_collection;


struct AccumulatePixelDistributionsUserData {
    std::map<std::string, std::vector<double>> accumulated_voxels; // key: RawROIName.
};

bool AccumulatePixelDistributions(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::experimental::any ud );

