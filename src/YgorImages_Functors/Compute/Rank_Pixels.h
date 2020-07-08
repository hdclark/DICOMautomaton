//Rank_Pixels.h.
#pragma once

#include <cmath>
#include <any>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <string>
#include <vector>


template <class T, class R> class planar_image_collection;
template <class T> class contour_collection;


struct RankPixelsUserData {

    // Only pixels with values between these thresholds (inclusive) are considered.
    double inc_lower_threshold = -(std::numeric_limits<double>::infinity());
    double inc_upper_threshold = std::numeric_limits<double>::infinity();

    typedef enum { // Controls how pixel values are replaced.
        Rank,        // Zero-based pixel value rank.
        Percentile   // Replace ranks with the corresponding percentile.
    } ReplacementMethod;

    ReplacementMethod replacement_method = ReplacementMethod::Percentile;

};

bool ComputeRankPixels(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::any ud );

