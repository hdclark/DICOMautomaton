//Volumetric_Correlation_Detector.h.
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

/*
typedef enum { // Controls which image derivatives are computed.

    // Centered first-order finite-difference derivatives.
    first, //Simple cartesian-aligned.

    Sobel_3x3x3

} VolumetricCorrelationDetectorEstimator;

typedef enum { // Controls how image derivatives are computed and combined.
    row_aligned,    //Where applicable.
    column_aligned, //Where applicable.
    image_aligned,  //Where applicable.

    magnitude,  //Magnitude of the gradient vector.

    non_maximum_suppression //Edge-thinning technique to erode thick edges.

} VolumetricCorrelationDetectorMethod;
*/

struct ComputeVolumetricCorrelationDetectorUserData {

/*
    // The default should be symmetric.
    VolumetricCorrelationDetectorEstimator order = VolumetricCorrelationDetectorEstimator::first;
    VolumetricCorrelationDetectorMethod method = VolumetricCorrelationDetectorMethod::magnitude;
*/

    float low  = 0.05; // 5th percentile.
    float high = 0.95; // 95th percentile.

    // The channel to analyze. If negative, all channels are analyzed.
    long int channel = -1;

};

bool ComputeVolumetricCorrelationDetector(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::any ud );

