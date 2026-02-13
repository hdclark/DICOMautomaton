//Alignment_Demons_SYCL.h - A part of DICOMautomaton 2026. Written by hal clark.
//
// This header declares SYCL-accelerated helper functions for the Demons registration algorithm.
// These functions use the SyclVolume representation for efficient device computation.

#pragma once

#include <optional>
#include <cstdint>

#include "YgorImages.h"

#include "Alignment_Demons.h"
#include "SYCL_Volume.h"

// Forward declaration. Only needed for full build.
class deformation_field;

namespace AlignViaDemonsSYCL {

// SYCL-accelerated version of the demons update computation.
// This function performs a single iteration of the demons algorithm on the GPU.
//
// Inputs:
//   - stationary_vol: The fixed/target image volume.
//   - warped_moving_vol: The current warped moving image.
//   - gradient_vol: Precomputed gradient of the stationary image (3 channels: dx, dy, dz).
//   - deformation_vol: Current deformation field (3 channels: dx, dy, dz).
//   - params: Algorithm parameters.
//
// Outputs:
//   - deformation_vol is updated in-place with the new deformation field.
//   - Returns the mean squared error between stationary and warped_moving after this iteration.
//
double compute_demons_iteration_sycl(
    const SyclVolume<float> &stationary_vol,
    SyclVolume<float> &warped_moving_vol,
    const SyclVolume<double> &gradient_vol,
    SyclVolume<double> &deformation_vol,
    const AlignViaDemonsParams &params);


// SYCL-accelerated gradient computation.
// Computes the gradient of a scalar volume, returning a 3-channel volume.
SyclVolume<double> compute_gradient_sycl(const SyclVolume<float> &vol);


// SYCL-accelerated Gaussian smoothing of a vector field.
// The field must have 3 channels (dx, dy, dz).
void smooth_vector_field_sycl(SyclVolume<double> &field, double sigma_mm);


// SYCL-accelerated image warping using a deformation field.
// Returns a new warped volume.
SyclVolume<float> warp_image_sycl(
    const SyclVolume<float> &source_vol,
    const SyclVolume<double> &deformation_vol);


// Complete SYCL-accelerated demons registration.
// This function marshals planar_image_collection data to SyclVolume, performs
// the registration on device, and marshals the result back.
// Note: This function is only available when DCMA_FULL_BUILD is defined.
//
// Returns std::nullopt if registration fails.
#ifdef DCMA_FULL_BUILD
std::optional<deformation_field> AlignViaDemons_SYCL(
    AlignViaDemonsParams &params,
    const planar_image_collection<float, double> &moving,
    const planar_image_collection<float, double> &stationary);
#endif

} // namespace AlignViaDemonsSYCL
