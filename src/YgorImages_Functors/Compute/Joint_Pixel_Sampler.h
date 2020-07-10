//Joint_Pixel_Sampler.h.
#pragma once

#include <any>
#include <functional>
#include <limits>
#include <list>
#include <string>
#include <vector>

#include "YgorMath.h"

template <class T, class R> class planar_image_collection;
template <class T> class contour_collection;

struct ComputeJointPixelSamplerUserData {

    // -----------------------------
    // The channel to consider. 
    //
    // Note: Channel numbers in the images that will be edited and reference images must match.
    //       Negative values will use all channels.
    long int channel = -1;

    // -----------------------------
    // Parameters for pixel thresholds.

    // Pixel thresholds for the images that will be edited. Only pixels with values between these thresholds (inclusive)
    // will be compared.
    double inc_lower_threshold = -(std::numeric_limits<double>::infinity());
    double inc_upper_threshold = std::numeric_limits<double>::infinity();

    // -----------------------------
    // Reduction functor for joint voxels.
    //
    // The vector contains: (1) the intensity of the first voxel, and then (2-n) the intensities of all sampled voxels
    // from reference images. Note that the sampled voxel values may not correspond to an actual voxel; they can be
    // interpolated if the images do not align exactly. The position of the first image's voxel (which is also the point
    // in space the other intensities are sampled) is also provided.
    std::function<float(std::vector<float> &, vec3<double>)> 
    f_reduce = [](std::vector<float> &, const vec3<double>&) -> float {
        return std::numeric_limits<float>::quiet_NaN();
    };

    // -----------------------------
    // The method of voxel sampling to use.
    enum class
    SamplingMethod {
        NearestVoxel,            // Find the encompassing voxel. Involves no interpolation.
                                 // Sampled value may be shifted from the true value up to 0.5*max(pxl_dx,pxl_dy,pxl_dz).
                                 // Note that this is safe for unaligned images.
                                 // This method is best used when a discrete number of voxel values are required, e.g.,
                                 // clustering IDs or integer-valued voxels.

        LinearInterpolation,     // Perform trilinear interpolation to sample the corresponding value at the precise 
                                 // location of the image to edit's voxel centre.
    } sampling_method = SamplingMethod::LinearInterpolation;

    // -----------------------------
    // Outgoing image description to imbue.
    std::string description;

};

bool ComputeJointPixelSampler(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::any ud );

