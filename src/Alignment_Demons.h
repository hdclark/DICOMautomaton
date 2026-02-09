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

namespace AlignViaDemonsHelpers {

// Helper function to resample a moving image onto a reference image's grid.
// This is needed to handle images with different orientations or alignments.
planar_image_collection<float, double>
resample_image_to_reference_grid(
    const planar_image_collection<float, double> & moving,
    const planar_image_collection<float, double> & reference );


// Helper function to perform histogram matching.
// Maps the intensity distribution of the source to match the reference.
planar_image_collection<float, double>
histogram_match(
    const planar_image_collection<float, double> & source,
    const planar_image_collection<float, double> & reference,
    int64_t num_bins,
    double outlier_fraction );

// Helper functions to emulate random-access to a std::list.
//
// BEWARE that references can become stale due to scope, deletion, etc. No images are allocated in this helper.
//
// TODO: replace callsites with cached image index, e.g., using planar_image_adjacency, which will be much more
// efficient. Alternatively, for visiting all images in order, just using image iterators directly and iterating once
// per loop will be best.
template <class T,class R>
planar_image<T, R> &
get_image( std::list<planar_image<T, R>> &imgs, size_t i){
    const size_t N_imgs = imgs.size();
    if(!isininc(0,i,N_imgs)){
        throw std::runtime_error("Requested image index not present, unable to continue");
    }
    auto it = std::next( std::begin(imgs), i );
    return *it;
}

template <class T,class R>
const planar_image<T, R> &
get_image( const std::list<planar_image<T, R>> &imgs, size_t i){
    const size_t N_imgs = imgs.size();
    if(!isininc(0,i,N_imgs)){
        throw std::runtime_error("Requested image index not present, unable to continue");
    }
    auto it = std::next( std::begin(imgs), i );
    return *it;
}

// Helper function to apply 3D Gaussian smoothing to a vector field.
// The field should have 3 channels representing dx, dy, dz displacements.
void
smooth_vector_field(
    planar_image_collection<double, double> & field,
    double sigma_mm );


// Helper function to compute the gradient of an image collection.
// Returns a 3-channel image where channels represent gradients in x, y, z directions.
planar_image_collection<double, double>
compute_gradient(const planar_image_collection<float, double> & img_coll);

// Helper function to warp an image using a deformation field.
planar_image_collection<float, double>
warp_image_with_field(
    const planar_image_collection<float, double> & img_coll,
    const deformation_field & def_field );

} // namespace AlignViaDemonsHelpers


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

