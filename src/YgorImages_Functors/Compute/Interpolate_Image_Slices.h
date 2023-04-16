//Interpolate_Image_Slices.h.
#pragma once

#include <any>
#include <functional>
#include <list>
#include <string>
#include <cstdint>


template <class T, class R> class planar_image_collection;
template <class T> class contour_collection;

struct ComputeInterpolateImageSlicesUserData {

    // -----------------------------
    // The type of interpolation method to use.
    enum class
    InterpolationMethod {
        Linear,               // Linear interpolation without extrapolation at the extrema.
        LinearExtrapolation,  // Linear interpolation with extrapolation at the extrema.
    } interpolation_method = InterpolationMethod::LinearExtrapolation;


    // -----------------------------
    // The channel to consider. 
    //
    // Note: Channel numbers in the images that will be edited and reference images must match.
    int64_t channel = 0;


    // -----------------------------
    // The description to imbue images with.
    std::string description;

};

bool ComputeInterpolateImageSlices(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::any ud );

