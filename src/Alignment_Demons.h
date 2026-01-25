//Alignment_Demons.h - A part of DICOMautomaton 2026. Written by hal clark.

#pragma once

#include <optional>
#include <list>
#include <functional>
#include <iosfwd>
#include <cstdint>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorImages.h"

#include "Alignment_Field.h"


// Parameters for controlling the demons registration algorithm.
struct AlignViaDemonsParams {
    
    // The maximum number of iterations to perform.
    int64_t max_iterations = 100;
    
    // The convergence threshold. Registration stops when the mean squared error change is below this value.
    double convergence_threshold = 0.001;
    
    // The standard deviation (in DICOM units, mm) of the Gaussian kernel used to smooth the deformation field.
    // This controls regularization and ensures smooth deformations.
    double deformation_field_smoothing_sigma = 1.0;
    
    // The standard deviation (in DICOM units, mm) of the Gaussian kernel used to smooth the update field.
    // This is primarily used in diffeomorphic demons.
    double update_field_smoothing_sigma = 0.5;
    
    // Whether to use the diffeomorphic demons variant.
    // If true, uses an exponential update scheme that ensures diffeomorphic (invertible) transformations.
    bool use_diffeomorphic = false;
    
    // Whether to apply histogram matching to the moving image before registration.
    // This can help when images have different intensity distributions (e.g., different scanners or protocols).
    bool use_histogram_matching = false;
    
    // The number of histogram bins to use for histogram matching.
    int64_t histogram_bins = 256;
    
    // The fraction of intensity values to use when determining histogram bounds (to handle outliers).
    // E.g., 0.01 means use the range from 1st to 99th percentile.
    double histogram_outlier_fraction = 0.01;
    
    // Normalization factor for the demons force (gradient magnitude).
    // This controls the step size and affects convergence speed and stability.
    double normalization_factor = 1.0;
    
    // Maximum update magnitude per iteration (in DICOM units, mm).
    // This prevents large, unstable updates.
    double max_update_magnitude = 2.0;
    
    // Verbosity level for logging intermediate results.
    int64_t verbosity = 1;
};


// This routine performs deformable image registration using the demons algorithm.
//
// The demons algorithm is an intensity-based registration method that iteratively computes
// a deformation field to align a moving image to a fixed (stationary) image.
//
// The basic algorithm:
// 1. Compute the gradient of the fixed image
// 2. Compute the intensity difference between fixed and (warped) moving image
// 3. Compute update field: displacement = - (difference * gradient) / (gradient_magnitude^2 + difference^2)
// 4. Smooth the update field (optional, primarily for diffeomorphic variant)
// 5. Add/compose the update to the deformation field
// 6. Smooth the deformation field for regularization
// 7. Warp the moving image with the updated deformation field
// 8. Repeat until convergence or max iterations
//
// The diffeomorphic variant uses an exponential update scheme to ensure the transformation
// is invertible (diffeomorphic).
//
// Note: This routine handles images that are not aligned or have different orientations
// by first resampling the moving image onto the fixed image's grid.
//
std::optional<deformation_field>
AlignViaDemons(AlignViaDemonsParams & params,
               const planar_image_collection<float, double> & moving,
               const planar_image_collection<float, double> & stationary );

