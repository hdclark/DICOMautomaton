//Volumetric_Neighbourhood_Sampler.h.
#pragma once

#include <cmath>
#include <any>
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

struct ComputeVolumetricNeighbourhoodSamplerUserData {

    // The type of neighbourhood to use.
    enum class
    Neighbourhood {
        Spherical,         // Spherically-bound neighbourhood.
        Cubic,             // Cubically-bound neighbourhood.
        Selection,         // Specific-voxel sampling via list of integer triplets.
        SelectionPeriodic, // Periodic boundary conditions for integer triplets.
    } neighbourhood = Neighbourhood::Spherical;

    // -----------------------------
    // Generic parameters controlling the neighbourhood search (in DICOM units; mm).
    //
    // Stop searching the voxel neighbourhood when all voxels are at least this far away.
    //
    // Note: Applicable only for whole-neighbourhood sampling.
    double maximum_distance = 3.0;

    // -----------------------------
    // Voxel selection for specific voxel addressing relative to the current voxel (in integer voxel coordinates).
    //
    // Note: Applicable only for specific-voxel sampling.
    //
    // Note: The triplets are relative to the current voxel (so can be positive or negative) and ordered like: (row,
    //       column, image).
    //
    // Note: The shuttle of voxel values passed to the reduction functor will be ordered to correspond with the triplet
    //       order. If the specified voxel is not available (e.g., on borders), a NaN will be emitted in its place.
    std::vector<std::array<long int, 3>> voxel_triplets;

    // -----------------------------
    // The channel to consider. 
    //
    // Note: Channel numbers in the images that will be edited and reference images must match.
    //       Negative values will use all channels.
    long int channel = -1;

    // -----------------------------
    // Reduction functor for bounded voxels.
    //
    // The scalar parameter contains the existing voxel value and the vector contains the entire enighbourhood
    // (possibly including the existing voxel value). The vec3 contains the voxel's location.
    std::function<float(float, std::vector<float> &, vec3<double>)> 
    f_reduce = [](float v, std::vector<float> &, const vec3<double>&) -> float {
        return v; // Effectively does nothing.
    };

    // -----------------------------
    // Outgoing image description to imbue.
    std::string description;

};

bool ComputeVolumetricNeighbourhoodSampler(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::any ud );

