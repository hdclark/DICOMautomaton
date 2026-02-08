//Alignment_Demons.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>
#include <vector>
#include <functional>
#include <cmath>
#include <limits>

#include "doctest20251212/doctest.h"

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorStats.h"
#include "YgorString.h"

#include "Structs.h"
#include "Regex_Selectors.h"
#include "Thread_Pool.h"

#include "Alignment_Rigid.h"
#include "Alignment_Field.h"
#include "Alignment_Demons.h"


// Helper function to resample a moving image onto a reference image's grid.
// This is needed to handle images with different orientations or alignments.
static planar_image_collection<float, double>
resample_image_to_reference_grid(
    const planar_image_collection<float, double> & moving,
    const planar_image_collection<float, double> & reference ){
    
    if(moving.images.empty() || reference.images.empty()){
        throw std::invalid_argument("Cannot resample: image collection is empty");
    }
    
    // Create output image collection matching reference geometry
    planar_image_collection<float, double> resampled;
    
    for(const auto &ref_img : reference.images){
        // Create a new image with the same geometry as the reference
        planar_image<float, double> new_img = ref_img;
        
        // Clear the pixel data and initialize to zero (or NaN for out-of-bounds)
        for(auto &val : new_img.data){
            val = std::numeric_limits<float>::quiet_NaN();
        }
        
        // Sample from the moving image at each voxel position in the reference grid
        const int64_t N_rows = ref_img.rows;
        const int64_t N_cols = ref_img.columns;
        const int64_t N_chnls = ref_img.channels;
        
        for(int64_t row = 0; row < N_rows; ++row){
            for(int64_t col = 0; col < N_cols; ++col){
                // Get the 3D position of this voxel in the reference image
                const auto pos = ref_img.position(row, col);
                
                // Try to sample this position from the moving image using trilinear interpolation
                for(int64_t chnl = 0; chnl < N_chnls; ++chnl){
                    const auto val = moving.trilinearly_interpolate(pos, chnl, std::numeric_limits<float>::quiet_NaN());
                    if(std::isfinite(val)){
                        new_img.reference(row, col, chnl) = val;
                    }
                }
            }
        }
        
        resampled.images.push_back(new_img);
    }
    
    return resampled;
}


// Helper function to perform histogram matching.
// Maps the intensity distribution of the source to match the reference.
static planar_image_collection<float, double>
histogram_match(
    const planar_image_collection<float, double> & source,
    const planar_image_collection<float, double> & reference,
    int64_t num_bins,
    double outlier_fraction ){
    
    if(source.images.empty() || reference.images.empty()){
        throw std::invalid_argument("Cannot perform histogram matching: image collection is empty");
    }
    
    // Collect all finite pixel values from both images
    std::vector<double> source_values;
    std::vector<double> reference_values;
    
    for(const auto &img : source.images){
        for(const auto &val : img.data){
            if(std::isfinite(val)){
                source_values.push_back(val);
            }
        }
    }
    
    for(const auto &img : reference.images){
        for(const auto &val : img.data){
            if(std::isfinite(val)){
                reference_values.push_back(val);
            }
        }
    }
    
    if(source_values.empty() || reference_values.empty()){
        YLOGWARN("No valid pixel values found for histogram matching, returning source unchanged");
        return source;
    }
    
    // Sort to compute percentiles
    std::sort(source_values.begin(), source_values.end());
    std::sort(reference_values.begin(), reference_values.end());
    
    // Determine intensity bounds based on outlier fraction
    const auto get_percentile = [](const std::vector<double> &sorted_vals, double percentile) -> double {
        const size_t idx = static_cast<size_t>(percentile * (sorted_vals.size() - 1));
        return sorted_vals[idx];
    };
    
    const double src_min = get_percentile(source_values, outlier_fraction);
    const double src_max = get_percentile(source_values, 1.0 - outlier_fraction);
    const double ref_min = get_percentile(reference_values, outlier_fraction);
    const double ref_max = get_percentile(reference_values, 1.0 - outlier_fraction);
    
    // Check for degenerate intensity ranges (constant images)
    if(src_max <= src_min || ref_max <= ref_min){
        YLOGWARN("Image has constant or near-constant intensity, histogram matching not applicable");
        return source;
    }
    
    // Build cumulative histograms
    std::vector<double> src_cdf(num_bins, 0.0);
    std::vector<double> ref_cdf(num_bins, 0.0);
    
    for(const auto &val : source_values){
        if(val >= src_min && val <= src_max){
            const int64_t bin = std::min<int64_t>(num_bins - 1, 
                static_cast<int64_t>((val - src_min) / (src_max - src_min) * num_bins));
            src_cdf[bin] += 1.0;
        }
    }
    
    for(const auto &val : reference_values){
        if(val >= ref_min && val <= ref_max){
            const int64_t bin = std::min<int64_t>(num_bins - 1,
                static_cast<int64_t>((val - ref_min) / (ref_max - ref_min) * num_bins));
            ref_cdf[bin] += 1.0;
        }
    }
    
    // Normalize to create CDFs
    for(int64_t i = 1; i < num_bins; ++i){
        src_cdf[i] += src_cdf[i-1];
        ref_cdf[i] += ref_cdf[i-1];
    }
    
    const double src_total = src_cdf.back();
    const double ref_total = ref_cdf.back();
    
    if(src_total > 0.0){
        for(auto &val : src_cdf) val /= src_total;
    }
    if(ref_total > 0.0){
        for(auto &val : ref_cdf) val /= ref_total;
    }
    
    // Build lookup table for mapping
    std::vector<double> lookup(num_bins);
    for(int64_t src_bin = 0; src_bin < num_bins; ++src_bin){
        const double src_quantile = src_cdf[src_bin];
        
        // Find corresponding bin in reference
        int64_t ref_bin = 0;
        for(int64_t i = 0; i < num_bins; ++i){
            if(ref_cdf[i] >= src_quantile){
                ref_bin = i;
                break;
            }
        }
        
        // Map to reference intensity
        lookup[src_bin] = ref_min + (ref_max - ref_min) * ref_bin / num_bins;
    }
    
    // Apply the mapping
    planar_image_collection<float, double> matched = source;
    
    for(auto &img : matched.images){
        for(auto &val : img.data){
            if(std::isfinite(val)){
                if(val < src_min){
                    val = ref_min;
                }else if(val > src_max){
                    val = ref_max;
                }else{
                    const int64_t bin = std::min<int64_t>(num_bins - 1,
                        static_cast<int64_t>((val - src_min) / (src_max - src_min) * num_bins));
                    val = lookup[bin];
                }
            }
        }
    }
    
    return matched;
}

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
static void
smooth_vector_field(
    planar_image_collection<double, double> & field,
    double sigma_mm ){
    
    if(field.images.empty()){
        return;
    }
    
    // Early return if smoothing is disabled
    if(sigma_mm <= 0.0){
        return;
    }
    
    // Check that all images have 3 channels
    for(const auto &img : field.images){
        if(img.channels != 3){
            throw std::invalid_argument("Vector field smoothing requires 3-channel images");
        }
    }
    
    // Simple Gaussian smoothing using a separable 3D kernel
    // This is a basic implementation; more sophisticated methods could be used
    
    // Determine the kernel size based on sigma (3-sigma rule)
    const auto &ref_img = field.images.front();
    const double pxl_dx = ref_img.pxl_dx;
    const double pxl_dy = ref_img.pxl_dy;
    const double pxl_dz = ref_img.pxl_dz;
    
    const int64_t kernel_radius_x = std::max<int64_t>(1, static_cast<int64_t>(3.0 * sigma_mm / pxl_dx));
    const int64_t kernel_radius_y = std::max<int64_t>(1, static_cast<int64_t>(3.0 * sigma_mm / pxl_dy));
    const int64_t kernel_radius_z = std::max<int64_t>(1, static_cast<int64_t>(3.0 * sigma_mm / pxl_dz));
    
    // Create 1D Gaussian kernels for each direction
    auto create_1d_gaussian = [](int64_t radius, double sigma_pixels) -> std::vector<double> {
        std::vector<double> kernel(2 * radius + 1);
        double sum = 0.0;
        for(int64_t i = -radius; i <= radius; ++i){
            const double val = std::exp(-0.5 * (i * i) / (sigma_pixels * sigma_pixels));
            kernel[i + radius] = val;
            sum += val;
        }
        // Normalize
        for(auto &val : kernel) val /= sum;
        return kernel;
    };
    
    const double sigma_x = sigma_mm / pxl_dx;
    const double sigma_y = sigma_mm / pxl_dy;
    const double sigma_z = sigma_mm / pxl_dz;
    
    const auto kernel_x = create_1d_gaussian(kernel_radius_x, sigma_x);
    const auto kernel_y = create_1d_gaussian(kernel_radius_y, sigma_y);
    const auto kernel_z = create_1d_gaussian(kernel_radius_z, sigma_z);
    
    // Apply separable filtering for each channel
    for(int64_t chnl = 0; chnl < 3; ++chnl){
        
        // Apply along x-direction (columns)
        planar_image_collection<double, double> temp_x = field;
        for(size_t img_idx = 0; img_idx < temp_x.images.size(); ++img_idx){
            auto &img = get_image(temp_x.images,img_idx);
            const int64_t N_rows = img.rows;
            const int64_t N_cols = img.columns;
            
            for(int64_t row = 0; row < N_rows; ++row){
                for(int64_t col = 0; col < N_cols; ++col){
                    double sum = 0.0;
                    double weight_sum = 0.0;
                    
                    for(int64_t k = -kernel_radius_x; k <= kernel_radius_x; ++k){
                        const int64_t col_k = col + k;
                        if(col_k >= 0 && col_k < N_cols){
                            const double val = get_image(field.images,img_idx).value(row, col_k, chnl);
                            if(std::isfinite(val)){
                                const double w = kernel_x[k + kernel_radius_x];
                                sum += w * val;
                                weight_sum += w;
                            }
                        }
                    }
                    
                    if(weight_sum > 0.0){
                        img.reference(row, col, chnl) = sum / weight_sum;
                    }
                }
            }
        }
        
        // Apply along y-direction (rows)
        planar_image_collection<double, double> temp_y = temp_x;
        for(size_t img_idx = 0; img_idx < temp_y.images.size(); ++img_idx){
            auto &img = get_image(temp_y.images,img_idx);
            const int64_t N_rows = img.rows;
            const int64_t N_cols = img.columns;
            
            for(int64_t row = 0; row < N_rows; ++row){
                for(int64_t col = 0; col < N_cols; ++col){
                    double sum = 0.0;
                    double weight_sum = 0.0;
                    
                    for(int64_t k = -kernel_radius_y; k <= kernel_radius_y; ++k){
                        const int64_t row_k = row + k;
                        if(row_k >= 0 && row_k < N_rows){
                            const double val = get_image(temp_x.images,img_idx).value(row_k, col, chnl);
                            if(std::isfinite(val)){
                                const double w = kernel_y[k + kernel_radius_y];
                                sum += w * val;
                                weight_sum += w;
                            }
                        }
                    }
                    
                    if(weight_sum > 0.0){
                        img.reference(row, col, chnl) = sum / weight_sum;
                    }
                }
            }
        }
        
        // Apply along z-direction (between images) and write back to field
        const int64_t N_imgs = field.images.size();
        for(int64_t img_idx = 0; img_idx < N_imgs; ++img_idx){
            auto &img = get_image(field.images,img_idx);
            const int64_t N_rows = img.rows;
            const int64_t N_cols = img.columns;
            
            for(int64_t row = 0; row < N_rows; ++row){
                for(int64_t col = 0; col < N_cols; ++col){
                    double sum = 0.0;
                    double weight_sum = 0.0;
                    
                    for(int64_t k = -kernel_radius_z; k <= kernel_radius_z; ++k){
                        const int64_t img_k = img_idx + k;
                        if(img_k >= 0 && img_k < N_imgs){
                            const double val = get_image(temp_y.images,img_k).value(row, col, chnl);
                            if(std::isfinite(val)){
                                const double w = kernel_z[k + kernel_radius_z];
                                sum += w * val;
                                weight_sum += w;
                            }
                        }
                    }
                    
                    if(weight_sum > 0.0){
                        img.reference(row, col, chnl) = sum / weight_sum;
                    }
                }
            }
        }
    }
}


// Helper function to compute the gradient of an image collection.
// Returns a 3-channel image where channels represent gradients in x, y, z directions.
static planar_image_collection<double, double>
compute_gradient(const planar_image_collection<float, double> & img_coll){
    
    if(img_coll.images.empty()){
        throw std::invalid_argument("Cannot compute gradient: image collection is empty");
    }
    
    // Create output image collection with 3 channels (for gradients in x, y, z)
    planar_image_collection<double, double> gradient;
    
    for(size_t img_idx = 0; img_idx < img_coll.images.size(); ++img_idx){
        const auto &img = get_image(img_coll.images,img_idx);
        
        planar_image<double, double> grad_img;
        grad_img.init_orientation(img.row_unit, img.col_unit);
        grad_img.init_buffer(img.rows, img.columns, 3); // 3 channels for dx, dy, dz
        grad_img.init_spatial(img.pxl_dx, img.pxl_dy, img.pxl_dz, img.anchor, img.offset);
        grad_img.metadata = img.metadata;
        
        const int64_t N_rows = img.rows;
        const int64_t N_cols = img.columns;
        
        // Compute gradients using central differences (where possible)
        for(int64_t row = 0; row < N_rows; ++row){
            for(int64_t col = 0; col < N_cols; ++col){
                
                // Gradient in x-direction (along columns)
                double grad_x = 0.0;
                if(col > 0 && col < N_cols - 1){
                    const double val_left = img.value(row, col - 1, 0);
                    const double val_right = img.value(row, col + 1, 0);
                    if(std::isfinite(val_left) && std::isfinite(val_right)){
                        grad_x = (val_right - val_left) / (2.0 * img.pxl_dx);
                    }
                }else if(col == 0 && N_cols > 1){
                    const double val_curr = img.value(row, col, 0);
                    const double val_right = img.value(row, col + 1, 0);
                    if(std::isfinite(val_curr) && std::isfinite(val_right)){
                        grad_x = (val_right - val_curr) / img.pxl_dx;
                    }
                }else if(col == N_cols - 1 && N_cols > 1){
                    const double val_left = img.value(row, col - 1, 0);
                    const double val_curr = img.value(row, col, 0);
                    if(std::isfinite(val_left) && std::isfinite(val_curr)){
                        grad_x = (val_curr - val_left) / img.pxl_dx;
                    }
                }
                
                // Gradient in y-direction (along rows)
                double grad_y = 0.0;
                if(row > 0 && row < N_rows - 1){
                    const double val_up = img.value(row - 1, col, 0);
                    const double val_down = img.value(row + 1, col, 0);
                    if(std::isfinite(val_up) && std::isfinite(val_down)){
                        grad_y = (val_down - val_up) / (2.0 * img.pxl_dy);
                    }
                }else if(row == 0 && N_rows > 1){
                    const double val_curr = img.value(row, col, 0);
                    const double val_down = img.value(row + 1, col, 0);
                    if(std::isfinite(val_curr) && std::isfinite(val_down)){
                        grad_y = (val_down - val_curr) / img.pxl_dy;
                    }
                }else if(row == N_rows - 1 && N_rows > 1){
                    const double val_up = img.value(row - 1, col, 0);
                    const double val_curr = img.value(row, col, 0);
                    if(std::isfinite(val_up) && std::isfinite(val_curr)){
                        grad_y = (val_curr - val_up) / img.pxl_dy;
                    }
                }
                
                // Gradient in z-direction (between slices)
                double grad_z = 0.0;
                const int64_t N_imgs = img_coll.images.size();
                if(N_imgs > 1){
                    if( (img_idx > 0UL) && (img_idx < (N_imgs - 1UL)) ){
                        const double val_prev = get_image(img_coll.images,img_idx - 1).value(row, col, 0);
                        const double val_next = get_image(img_coll.images,img_idx + 1).value(row, col, 0);
                        if(std::isfinite(val_prev) && std::isfinite(val_next)){
                            grad_z = (val_next - val_prev) / (2.0 * img.pxl_dz);
                        }
                    }else if(img_idx == 0UL){
                        const double val_curr = img.value(row, col, 0);
                        const double val_next = get_image(img_coll.images,img_idx + 1).value(row, col, 0);
                        if(std::isfinite(val_curr) && std::isfinite(val_next)){
                            grad_z = (val_next - val_curr) / img.pxl_dz;
                        }
                    }else if(img_idx == (N_imgs - 1UL)){
                        const double val_prev = get_image(img_coll.images,img_idx - 1).value(row, col, 0);
                        const double val_curr = img.value(row, col, 0);
                        if(std::isfinite(val_prev) && std::isfinite(val_curr)){
                            grad_z = (val_curr - val_prev) / img.pxl_dz;
                        }
                    }
                }
                
                grad_img.reference(row, col, 0) = grad_x;
                grad_img.reference(row, col, 1) = grad_y;
                grad_img.reference(row, col, 2) = grad_z;
            }
        }
        
        gradient.images.push_back(grad_img);
    }
    
    return gradient;
}


// Helper function to warp an image using a deformation field.
static planar_image_collection<float, double>
warp_image_with_field(
    const planar_image_collection<float, double> & img_coll,
    const deformation_field & def_field ){
    
    if(img_coll.images.empty()){
        throw std::invalid_argument("Cannot warp: image collection is empty");
    }
    
    planar_image_collection<float, double> warped = img_coll;
    
    for(auto &img : warped.images){
        const int64_t N_rows = img.rows;
        const int64_t N_cols = img.columns;
        const int64_t N_chnls = img.channels;
        
        for(int64_t row = 0; row < N_rows; ++row){
            for(int64_t col = 0; col < N_cols; ++col){
                // Get the current voxel position
                const auto pos = img.position(row, col);
                
                // Apply the deformation to get the source position
                const auto warped_pos = def_field.transform(pos);
                
                // Sample from the original image at the warped position
                for(int64_t chnl = 0; chnl < N_chnls; ++chnl){
                    const auto val = img_coll.trilinearly_interpolate(warped_pos, chnl, 
                                                                       std::numeric_limits<float>::quiet_NaN());
                    img.reference(row, col, chnl) = val;
                }
            }
        }
    }
    
    return warped;
}


// Main demons registration function
std::optional<deformation_field>
AlignViaDemons(AlignViaDemonsParams & params,
               const planar_image_collection<float, double> & moving_in,
               const planar_image_collection<float, double> & stationary ){
    
    // Small epsilon to prevent division by zero
    constexpr double epsilon = 1e-10;
    
    if(moving_in.images.empty() || stationary.images.empty()){
        YLOGWARN("Unable to perform demons alignment: an image array is empty");
        return std::nullopt;
    }
    
    try {
        // Step 1: Resample moving image to stationary image's grid
        // This handles different orientations and alignments
        if(params.verbosity >= 1){
            YLOGINFO("Resampling moving image to reference grid");
        }
        auto moving = resample_image_to_reference_grid(moving_in, stationary);
        
        // Step 2: Apply histogram matching if requested
        if(params.use_histogram_matching){
            if(params.verbosity >= 1){
                YLOGINFO("Applying histogram matching");
            }
            moving = histogram_match(moving, stationary, params.histogram_bins, params.histogram_outlier_fraction);
        }
        
        // Step 3: Initialize deformation field (zero displacement)
        planar_image_collection<double, double> deformation_field_images;
        
        for(const auto &img : stationary.images){
            planar_image<double, double> def_img;
            def_img.init_orientation(img.row_unit, img.col_unit);
            def_img.init_buffer(img.rows, img.columns, 3); // 3 channels for dx, dy, dz
            def_img.init_spatial(img.pxl_dx, img.pxl_dy, img.pxl_dz, img.anchor, img.offset);
            def_img.metadata = img.metadata;
            
            // Initialize to zero displacement
            for(auto &val : def_img.data){
                val = 0.0;
            }
            
            deformation_field_images.images.push_back(def_img);
        }
        
        // Step 4: Iterative demons algorithm
        auto warped_moving = moving;
        double prev_mse = std::numeric_limits<double>::infinity();
        
        // Precompute gradient of the stationary (fixed) image; it does not change across iterations.
        auto gradient = compute_gradient(stationary);
        
        for(int64_t iter = 0; iter < params.max_iterations; ++iter){
            
            // Compute intensity difference
            double mse = 0.0;
            int64_t n_voxels = 0;
            
            planar_image_collection<double, double> update_field_images;
            
            for(size_t img_idx = 0; img_idx < stationary.images.size(); ++img_idx){
                const auto &fixed_img = get_image(stationary.images,img_idx);
                const auto &warped_img = get_image(warped_moving.images,img_idx);
                const auto &grad_img = get_image(gradient.images,img_idx);
                
                planar_image<double, double> update_img;
                update_img.init_orientation(fixed_img.row_unit, fixed_img.col_unit);
                update_img.init_buffer(fixed_img.rows, fixed_img.columns, 3);
                update_img.init_spatial(fixed_img.pxl_dx, fixed_img.pxl_dy, fixed_img.pxl_dz, 
                                       fixed_img.anchor, fixed_img.offset);
                update_img.metadata = fixed_img.metadata;
                
                // Initialize update to zero
                for(auto &val : update_img.data){
                    val = 0.0;
                }
                
                const int64_t N_rows = fixed_img.rows;
                const int64_t N_cols = fixed_img.columns;
                
                for(int64_t row = 0; row < N_rows; ++row){
                    for(int64_t col = 0; col < N_cols; ++col){
                        const double fixed_val = fixed_img.value(row, col, 0);
                        const double moving_val = warped_img.value(row, col, 0);
                        
                        if(!std::isfinite(fixed_val) || !std::isfinite(moving_val)){
                            continue;
                        }
                        
                        const double diff = fixed_val - moving_val;
                        mse += diff * diff;
                        ++n_voxels;
                        
                        const double grad_x = grad_img.value(row, col, 0);
                        const double grad_y = grad_img.value(row, col, 1);
                        const double grad_z = grad_img.value(row, col, 2);
                        
                        // Compute gradient magnitude squared
                        const double grad_mag_sq = grad_x * grad_x + grad_y * grad_y + grad_z * grad_z;
                        
                        // Demons force: u = - (diff * gradient) / (grad_mag^2 + diff^2 / normalization)
                        // This is the classic demons formulation
                        const double denom = grad_mag_sq + (diff * diff) / (params.normalization_factor + epsilon);
                        
                        if(denom > epsilon){
                            double update_x = - diff * grad_x / denom;
                            double update_y = - diff * grad_y / denom;
                            double update_z = - diff * grad_z / denom;
                            
                            // Clamp update magnitude
                            const double update_mag = std::sqrt(update_x * update_x + 
                                                               update_y * update_y + 
                                                               update_z * update_z);
                            if(update_mag > params.max_update_magnitude){
                                const double scale = params.max_update_magnitude / update_mag;
                                update_x *= scale;
                                update_y *= scale;
                                update_z *= scale;
                            }
                            
                            update_img.reference(row, col, 0) = update_x;
                            update_img.reference(row, col, 1) = update_y;
                            update_img.reference(row, col, 2) = update_z;
                        }
                    }
                }
                
                update_field_images.images.push_back(update_img);
            }
            
            if(n_voxels > 0){
                mse /= n_voxels;
            }
            
            if(params.verbosity >= 1){
                YLOGINFO("Iteration " << iter << ": MSE = " << mse);
            }
            
            // Check for convergence
            const double mse_change = std::abs(prev_mse - mse);
            if(mse_change < params.convergence_threshold && iter > 0){
                if(params.verbosity >= 1){
                    YLOGINFO("Converged after " << iter << " iterations");
                }
                break;
            }
            prev_mse = mse;
            
            // Smooth update field (for diffeomorphic variant)
            if(params.use_diffeomorphic && params.update_field_smoothing_sigma > 0.0){
                smooth_vector_field(update_field_images, params.update_field_smoothing_sigma);
            }
            
            // Add/compose update to deformation field
            if(params.use_diffeomorphic){
                // Diffeomorphic demons: compose the update with the current deformation field
                // This implements d ← d ∘ u (composition of deformation with update)
                // For each voxel position p in the deformation field, we compute:
                //   new_d(p) = d(p) + u(p + d(p))
                // This ensures the transformation remains diffeomorphic (invertible).
                
                // First, create a temporary deformation field from the update
                planar_image_collection<double, double> update_field_copy = update_field_images;
                deformation_field update_def_field(std::move(update_field_copy));
                
                // Now compose: for each position, add the update sampled at the deformed position
                for(size_t img_idx = 0; img_idx < deformation_field_images.images.size(); ++img_idx){
                    auto &def_img = get_image(deformation_field_images.images,img_idx);
                    const int64_t N_rows = def_img.rows;
                    const int64_t N_cols = def_img.columns;
                    
                    for(int64_t row = 0; row < N_rows; ++row){
                        for(int64_t col = 0; col < N_cols; ++col){
                            // Get current position
                            const auto pos = def_img.position(row, col);
                            
                            // Get current deformation at this position
                            const double dx = def_img.value(row, col, 0);
                            const double dy = def_img.value(row, col, 1);
                            const double dz = def_img.value(row, col, 2);
                            
                            // Compute deformed position
                            const vec3<double> deformed_pos = pos + vec3<double>(dx, dy, dz);
                            
                            // Sample update field at the deformed position
                            const double upd_dx = update_def_field.get_imagecoll_crefw().get().trilinearly_interpolate(deformed_pos, 0, 0.0);
                            const double upd_dy = update_def_field.get_imagecoll_crefw().get().trilinearly_interpolate(deformed_pos, 1, 0.0);
                            const double upd_dz = update_def_field.get_imagecoll_crefw().get().trilinearly_interpolate(deformed_pos, 2, 0.0);
                            
                            // Compose: new deformation = current deformation + update at deformed position
                            def_img.reference(row, col, 0) = dx + upd_dx;
                            def_img.reference(row, col, 1) = dy + upd_dy;
                            def_img.reference(row, col, 2) = dz + upd_dz;
                        }
                    }
                }
            }else{
                // Standard demons: simple addition
                for(size_t img_idx = 0; img_idx < deformation_field_images.images.size(); ++img_idx){
                    auto &def_img = get_image(deformation_field_images.images,img_idx);
                    const auto &upd_img = get_image(update_field_images.images,img_idx);
                    
                    for(size_t i = 0; i < def_img.data.size(); ++i){
                        def_img.data[i] += upd_img.data[i];
                    }
                }
            }
            
            // Smooth deformation field for regularization
            if(params.deformation_field_smoothing_sigma > 0.0){
                smooth_vector_field(deformation_field_images, params.deformation_field_smoothing_sigma);
            }
            
            // Create a copy for warping (to avoid move semantics issues)
            planar_image_collection<double, double> def_field_copy = deformation_field_images;
            deformation_field temp_def_field(std::move(def_field_copy));
            
            // Warp moving image with updated deformation field
            warped_moving = warp_image_with_field(moving, temp_def_field);
        }
        
        // Return final deformation field
        return deformation_field(std::move(deformation_field_images));
        
    }catch(const std::exception &e){
        YLOGWARN("Demons registration failed: " << e.what());
        return std::nullopt;
    }
}

// ============================================================================
// Unit tests
// ============================================================================

static planar_image_collection<float, double>
make_test_image_collection(
    int64_t slices,
    int64_t rows,
    int64_t cols,
    const std::function<float(int64_t, int64_t, int64_t)> &value_fn,
    const vec3<double> &offset = vec3<double>(0.0, 0.0, 0.0),
    double pxl_dx = 1.0,
    double pxl_dy = 1.0,
    double pxl_dz = 1.0){

    planar_image_collection<float, double> coll;
    const vec3<double> row_unit(1.0, 0.0, 0.0);
    const vec3<double> col_unit(0.0, 1.0, 0.0);
    const vec3<double> anchor(0.0, 0.0, 0.0);

    for(int64_t slice = 0; slice < slices; ++slice){
        planar_image<float, double> img;
        img.init_orientation(row_unit, col_unit);
        img.init_buffer(rows, cols, 1);
        const vec3<double> slice_offset = offset + vec3<double>(0.0, 0.0, static_cast<double>(slice) * pxl_dz);
        img.init_spatial(pxl_dx, pxl_dy, pxl_dz, anchor, slice_offset);

        for(int64_t row = 0; row < rows; ++row){
            for(int64_t col = 0; col < cols; ++col){
                img.reference(row, col, 0) = value_fn(slice, row, col);
            }
        }

        coll.images.push_back(img);
    }

    return coll;
}

static planar_image_collection<double, double>
make_test_vector_field(
    int64_t slices,
    int64_t rows,
    int64_t cols,
    const std::function<vec3<double>(int64_t, int64_t, int64_t)> &value_fn,
    const vec3<double> &offset = vec3<double>(0.0, 0.0, 0.0),
    double pxl_dx = 1.0,
    double pxl_dy = 1.0,
    double pxl_dz = 1.0){

    planar_image_collection<double, double> coll;
    const vec3<double> row_unit(1.0, 0.0, 0.0);
    const vec3<double> col_unit(0.0, 1.0, 0.0);
    const vec3<double> anchor(0.0, 0.0, 0.0);

    for(int64_t slice = 0; slice < slices; ++slice){
        planar_image<double, double> img;
        img.init_orientation(row_unit, col_unit);
        img.init_buffer(rows, cols, 3);
        const vec3<double> slice_offset = offset + vec3<double>(0.0, 0.0, static_cast<double>(slice) * pxl_dz);
        img.init_spatial(pxl_dx, pxl_dy, pxl_dz, anchor, slice_offset);

        for(int64_t row = 0; row < rows; ++row){
            for(int64_t col = 0; col < cols; ++col){
                const auto disp = value_fn(slice, row, col);
                img.reference(row, col, 0) = disp.x;
                img.reference(row, col, 1) = disp.y;
                img.reference(row, col, 2) = disp.z;
            }
        }

        coll.images.push_back(img);
    }

    return coll;
}

static std::pair<double, int64_t>
compute_mse_and_count(
    const planar_image_collection<float, double> &a,
    const planar_image_collection<float, double> &b){

    double mse = 0.0;
    int64_t count = 0;

    for(size_t img_idx = 0; img_idx < a.images.size(); ++img_idx){
        const auto &img_a = get_image(a.images, img_idx);
        const auto &img_b = get_image(b.images, img_idx);
        const int64_t rows = img_a.rows;
        const int64_t cols = img_a.columns;

        for(int64_t row = 0; row < rows; ++row){
            for(int64_t col = 0; col < cols; ++col){
                const double val_a = img_a.value(row, col, 0);
                const double val_b = img_b.value(row, col, 0);
                if(!std::isfinite(val_a) || !std::isfinite(val_b)){
                    continue;
                }
                const double diff = val_a - val_b;
                mse += diff * diff;
                ++count;
            }
        }
    }

    if(count > 0){
        mse /= static_cast<double>(count);
    }else{
        mse = std::numeric_limits<double>::quiet_NaN();
    }

    return {mse, count};
}

static double
max_abs_displacement(const deformation_field &field){
    double max_abs = 0.0;
    const auto &images = field.get_imagecoll_crefw().get().images;
    for(const auto &img : images){
        for(const auto &val : img.data){
            max_abs = std::max(max_abs, std::abs(val));
        }
    }
    return max_abs;
}

TEST_CASE( "resample_image_to_reference_grid identity and bounds" ){
    auto moving = make_test_image_collection(1, 2, 2,
        [](int64_t, int64_t row, int64_t col){
            return static_cast<float>(row * 10 + col);
        });

    SUBCASE("identity grid preserves values"){
        auto reference = make_test_image_collection(1, 2, 2,
            [](int64_t, int64_t, int64_t){ return 0.0f; });
        auto resampled = resample_image_to_reference_grid(moving, reference);
        const auto &resampled_img = get_image(resampled.images, 0);
        const auto &moving_img = get_image(moving.images, 0);
        CHECK(resampled_img.value(0, 0, 0) == doctest::Approx(moving_img.value(0, 0, 0)));
        CHECK(resampled_img.value(1, 1, 0) == doctest::Approx(moving_img.value(1, 1, 0)));
    }

    SUBCASE("larger reference leaves out-of-bounds as NaN"){
        auto reference = make_test_image_collection(1, 4, 4,
            [](int64_t, int64_t, int64_t){ return 0.0f; });
        auto resampled = resample_image_to_reference_grid(moving, reference);
        const auto &resampled_img = get_image(resampled.images, 0);
        CHECK(resampled_img.value(0, 0, 0) == doctest::Approx(0.0f));
        CHECK(std::isnan(resampled_img.value(3, 3, 0)));
    }

    SUBCASE("empty moving collection throws"){
        planar_image_collection<float, double> empty;
        auto reference = make_test_image_collection(1, 2, 2,
            [](int64_t, int64_t, int64_t){ return 0.0f; });
        REQUIRE_THROWS_AS(resample_image_to_reference_grid(empty, reference), std::invalid_argument);
    }
}

TEST_CASE( "histogram_match maps quantiles and handles constants" ){
    auto source = make_test_image_collection(1, 2, 2,
        [](int64_t, int64_t row, int64_t col){
            return static_cast<float>(row * 2 + col);
        });
    auto reference = make_test_image_collection(1, 2, 2,
        [](int64_t, int64_t row, int64_t col){
            return static_cast<float>(10.0 + 10.0 * (row * 2 + col));
        });

    auto matched = histogram_match(source, reference, 4, 0.0);
    const auto &matched_img = get_image(matched.images, 0);

    CHECK(matched_img.value(0, 0, 0) == doctest::Approx(10.0));
    CHECK(matched_img.value(0, 1, 0) == doctest::Approx(17.5));
    CHECK(matched_img.value(1, 0, 0) == doctest::Approx(25.0));
    CHECK(matched_img.value(1, 1, 0) == doctest::Approx(32.5));

    auto constant_source = make_test_image_collection(1, 2, 2,
        [](int64_t, int64_t, int64_t){ return 5.0f; });
    auto constant_reference = make_test_image_collection(1, 2, 2,
        [](int64_t, int64_t, int64_t){ return 10.0f; });
    auto constant_matched = histogram_match(constant_source, constant_reference, 8, 0.0);
    const auto &constant_img = get_image(constant_matched.images, 0);
    CHECK(constant_img.value(0, 0, 0) == doctest::Approx(5.0));
    CHECK(constant_img.value(1, 1, 0) == doctest::Approx(5.0));
}

TEST_CASE( "histogram_match rejects empty collections" ){
    planar_image_collection<float, double> empty;
    auto reference = make_test_image_collection(1, 1, 1,
        [](int64_t, int64_t, int64_t){ return 1.0f; });
    REQUIRE_THROWS_AS(histogram_match(empty, reference, 4, 0.0), std::invalid_argument);
    REQUIRE_THROWS_AS(histogram_match(reference, empty, 4, 0.0), std::invalid_argument);
}

TEST_CASE( "smooth_vector_field respects sigma and channel count" ){
    auto base_field = make_test_vector_field(1, 3, 3,
        [](int64_t, int64_t row, int64_t col){
            if(row == 1 && col == 1){
                return vec3<double>(3.0, 0.0, 0.0);
            }
            return vec3<double>(0.0, 0.0, 0.0);
        });

    auto no_smooth = base_field;
    smooth_vector_field(no_smooth, 0.0);
    const auto &orig_img = get_image(base_field.images, 0);
    const auto &no_img = get_image(no_smooth.images, 0);
    for(size_t i = 0; i < orig_img.data.size(); ++i){
        CHECK(orig_img.data[i] == doctest::Approx(no_img.data[i]));
    }

    auto smoothed = base_field;
    smooth_vector_field(smoothed, 1.0);
    const auto &smooth_img = get_image(smoothed.images, 0);
    CHECK(smooth_img.value(1, 1, 0) < 3.0);
    CHECK(smooth_img.value(1, 1, 0) > 0.0);
    CHECK(smooth_img.value(1, 0, 0) > 0.0);
    CHECK(smooth_img.value(1, 1, 1) == doctest::Approx(0.0));
    CHECK(smooth_img.value(1, 1, 2) == doctest::Approx(0.0));

    planar_image_collection<double, double> invalid_field;
    planar_image<double, double> invalid_img;
    invalid_img.init_orientation(vec3<double>(1.0, 0.0, 0.0), vec3<double>(0.0, 1.0, 0.0));
    invalid_img.init_buffer(2, 2, 1);
    invalid_img.init_spatial(1.0, 1.0, 1.0, vec3<double>(0.0, 0.0, 0.0), vec3<double>(0.0, 0.0, 0.0));
    invalid_field.images.push_back(invalid_img);
    REQUIRE_THROWS_AS(smooth_vector_field(invalid_field, 1.0), std::invalid_argument);
}

TEST_CASE( "compute_gradient captures linear ramps" ){
    auto img = make_test_image_collection(1, 3, 3,
        [](int64_t, int64_t row, int64_t col){
            return static_cast<float>(2.0 * row + col);
        });

    auto gradient = compute_gradient(img);
    const auto &grad_img = get_image(gradient.images, 0);
    CHECK(grad_img.value(1, 1, 0) == doctest::Approx(1.0));
    CHECK(grad_img.value(1, 1, 1) == doctest::Approx(2.0));
    CHECK(grad_img.value(1, 1, 2) == doctest::Approx(0.0));
}

TEST_CASE( "compute_gradient captures z differences" ){
    auto img = make_test_image_collection(3, 1, 1,
        [](int64_t slice, int64_t, int64_t){
            return static_cast<float>(slice);
        });

    auto gradient = compute_gradient(img);
    const auto &grad_img = get_image(gradient.images, 1);
    CHECK(grad_img.value(0, 0, 2) == doctest::Approx(1.0));
}

TEST_CASE( "compute_gradient rejects empty collections" ){
    planar_image_collection<float, double> empty;
    REQUIRE_THROWS_AS(compute_gradient(empty), std::invalid_argument);
}

TEST_CASE( "warp_image_with_field identity warping" ){
    auto img = make_test_image_collection(1, 3, 3,
        [](int64_t, int64_t row, int64_t col){
            return static_cast<float>(row * 10 + col);
        });

    auto field_images = make_test_vector_field(1, 3, 3,
        [](int64_t, int64_t, int64_t){
            return vec3<double>(0.0, 0.0, 0.0);
        });
    deformation_field field(std::move(field_images));

    auto warped = warp_image_with_field(img, field);
    const auto &warped_img = get_image(warped.images, 0);
    const auto &orig_img = get_image(img.images, 0);
    for(int64_t row = 0; row < orig_img.rows; ++row){
        for(int64_t col = 0; col < orig_img.columns; ++col){
            CHECK(warped_img.value(row, col, 0) == doctest::Approx(orig_img.value(row, col, 0)));
        }
    }

    planar_image_collection<float, double> empty;
    REQUIRE_THROWS_AS(warp_image_with_field(empty, field), std::invalid_argument);
}

TEST_CASE( "AlignViaDemons handles empty inputs" ){
    AlignViaDemonsParams params;
    params.verbosity = 0;
    planar_image_collection<float, double> empty;
    auto stationary = make_test_image_collection(1, 2, 2,
        [](int64_t, int64_t, int64_t){ return 1.0f; });
    auto result = AlignViaDemons(params, empty, stationary);
    CHECK_FALSE(result.has_value());
}

TEST_CASE( "AlignViaDemons returns zero field for identical images" ){
    auto img = make_test_image_collection(1, 3, 3,
        [](int64_t, int64_t row, int64_t col){
            return static_cast<float>(row + col);
        });

    AlignViaDemonsParams params;
    params.max_iterations = 3;
    params.convergence_threshold = 0.0;
    params.deformation_field_smoothing_sigma = 0.0;
    params.update_field_smoothing_sigma = 0.0;
    params.verbosity = 0;

    auto result = AlignViaDemons(params, img, img);
    REQUIRE(result.has_value());
    CHECK(max_abs_displacement(*result) < 1e-6);
}

TEST_CASE( "AlignViaDemons improves MSE for shifted image" ){
    auto stationary = make_test_image_collection(1, 5, 5,
        [](int64_t, int64_t, int64_t col){
            return static_cast<float>(col);
        });

    auto moving = make_test_image_collection(1, 5, 5,
        [](int64_t, int64_t, int64_t col){
            const int64_t shifted = std::min<int64_t>(4, col + 1);
            return static_cast<float>(shifted);
        });

    const auto [mse_before, count_before] = compute_mse_and_count(stationary, moving);

    AlignViaDemonsParams base_params;
    base_params.max_iterations = 15;
    base_params.convergence_threshold = 0.0;
    base_params.deformation_field_smoothing_sigma = 0.0;
    base_params.update_field_smoothing_sigma = 0.0;
    base_params.max_update_magnitude = 1.0;
    base_params.verbosity = 0;

    SUBCASE("standard demons"){
        auto params = base_params;
        params.use_diffeomorphic = false;
        auto result = AlignViaDemons(params, moving, stationary);
        REQUIRE(result.has_value());
        auto warped = warp_image_with_field(moving, *result);
        const auto [mse_after, count_after] = compute_mse_and_count(stationary, warped);
        CHECK(count_after >= count_before - 5);
        CHECK(mse_after < mse_before);
    }

    SUBCASE("diffeomorphic demons"){
        auto params = base_params;
        params.use_diffeomorphic = true;
        auto result = AlignViaDemons(params, moving, stationary);
        REQUIRE(result.has_value());
        auto warped = warp_image_with_field(moving, *result);
        const auto [mse_after, count_after] = compute_mse_and_count(stationary, warped);
        CHECK(count_after >= count_before - 5);
        CHECK(mse_after < mse_before);
    }
}
