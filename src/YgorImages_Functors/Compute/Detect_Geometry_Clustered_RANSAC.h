//Detect_Geometry_Clustered_RANSAC.h.
#pragma once

#include <any>
#include <functional>
#include <limits>
#include <list>

#include "YgorMath.h"

template <class T, class R> class planar_image_collection;
template <class T> class contour_collection;


struct DetectGeometryClusteredRANSACUserData {

    // Only pixels with values between these thresholds (inclusive) are considered.
    //
    // Note: Typically edge detection preceeds this algorithm.
    double inc_lower_threshold = -(std::numeric_limits<double>::infinity());
    double inc_upper_threshold = std::numeric_limits<double>::infinity();

    // Sphere radius to consider.
    double radius = 1.0;

    // Number of spheres to locate.
    long int count = 1;

    // Spheres detected.
    std::list< sphere<double> > spheres;

};

bool ComputeDetectGeometryClusteredRANSAC(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::any ud );

