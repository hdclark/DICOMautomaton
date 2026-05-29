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

// Object meant to serve as proxy for planar_image_collection with regular image array.
template <class T>
struct demons_volume {
    int64_t slices = 0;
    int64_t rows = 0;
    int64_t cols = 0;
    int64_t channels = 0;
    double pxl_dx = 1.0;
    double pxl_dy = 1.0;
    double pxl_dz = 1.0;
    std::vector<T> data;
};

template <class T>
inline size_t vol_idx(const demons_volume<T> &v, int64_t z, int64_t y, int64_t x, int64_t c){
    return static_cast<size_t>((((z * v.rows + y) * v.cols + x) * v.channels) + c);
}


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

// Helper function to warp an image using a deformation field.
planar_image_collection<float, double>
warp_image_with_field(
    const planar_image_collection<float, double> & img_coll,
    const deformation_field & def_field );


// Helper functions to marshal back-and-forth planar_image_collection (for Drover) <--> demons_volume (for SYCL).
template <class T>
demons_volume<T>
marshal_collection_to_volume(const planar_image_collection<T, double> &coll){
    if(coll.images.empty()){
        throw std::invalid_argument("Cannot marshal empty image collection");
    }
    auto &nonconst_coll = const_cast<planar_image_collection<T, double> &>(coll);
    std::list<std::reference_wrapper<planar_image<T, double>>> selected_imgs;
    for(auto &img : nonconst_coll.images){
        selected_imgs.push_back(std::ref(img));
    }
    if(!Images_Form_Regular_Grid(selected_imgs)){
        throw std::invalid_argument("Image collection is not a regular rectilinear grid and cannot be marshaled as a dense SYCL volume");
    }

    const auto &img0 = coll.images.front();
    demons_volume<T> out;
    out.slices = static_cast<int64_t>(coll.images.size());
    out.rows = img0.rows;
    out.cols = img0.columns;
    out.channels = img0.channels;
    out.pxl_dx = img0.pxl_dx;
    out.pxl_dy = img0.pxl_dy;
    out.pxl_dz = img0.pxl_dz;
    out.data.resize(static_cast<size_t>(out.slices * out.rows * out.cols * out.channels), T{});

    int64_t z = 0;
    for(const auto &img : coll.images){
        if(img.rows != out.rows || img.columns != out.cols || img.channels != out.channels){
            throw std::invalid_argument("Image collection has inconsistent rows/columns/channels and cannot be marshaled as a rectilinear volume");
        }
        for(int64_t y = 0; y < out.rows; ++y){
            for(int64_t x = 0; x < out.cols; ++x){
                for(int64_t c = 0; c < out.channels; ++c){
                    out.data[vol_idx(out, z, y, x, c)] = static_cast<T>(img.value(y, x, c));
                }
            }
        }
        ++z;
    }
    return out;
}

template <class T>
planar_image_collection<T, double>
marshal_volume_to_collection(const demons_volume<T> &vol,
                             const planar_image_collection<float, double> &reference_geometry){
    planar_image_collection<T, double> out;
    for(int64_t z = 0; z < vol.slices; ++z){
        const auto &ref_img = get_image(reference_geometry.images, static_cast<size_t>(z));
        planar_image<T, double> img;
        img.init_orientation(ref_img.row_unit, ref_img.col_unit);
        img.init_buffer(vol.rows, vol.cols, vol.channels);
        img.init_spatial(ref_img.pxl_dx, ref_img.pxl_dy, ref_img.pxl_dz, ref_img.anchor, ref_img.offset);
        img.metadata = ref_img.metadata;
        for(int64_t y = 0; y < vol.rows; ++y){
            for(int64_t x = 0; x < vol.cols; ++x){
                for(int64_t c = 0; c < vol.channels; ++c){
                    img.reference(y, x, c) = vol.data[vol_idx(vol, z, y, x, c)];
                }
            }
        }
        out.images.push_back(img);
    }
    return out;
}

} // namespace AlignViaDemonsHelpers


// This routine performs deformable image registration using the demons algorithm.
//
// The demons algorithm is an intensity-based registration method that iteratively computes
// a deformation field to align a moving image to a fixed (stationary) image.
//
// The basic algorithm:
// 1. Compute the gradient of the fixed image
// 2. Compute the intensity difference between fixed and (warped) moving image
// 3. Compute update field: displacement = (difference * gradient) / (gradient_magnitude^2 + difference^2)
//    where difference = fixed - moving. The resulting displacement points from
//    positions in the fixed image grid toward corresponding positions in the
//    moving image, suitable for pull-based warping where warped(x) = moving(x + d(x)).
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

