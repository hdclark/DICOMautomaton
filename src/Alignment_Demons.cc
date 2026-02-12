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
#include <thread>

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorStats.h"
#include "YgorString.h"

#include "Structs.h"
#include "Thread_Pool.h"

#include "Alignment_Rigid.h"
#include "Alignment_Field.h"
#include "Alignment_Demons.h"
#include "Alignment_Buffer3.h"

using namespace AlignViaDemonsHelpers;

// Helper function to resample a moving image onto a reference image's grid.
// This is needed to handle images with different orientations or alignments.
planar_image_collection<float, double>
AlignViaDemonsHelpers::resample_image_to_reference_grid(
    const planar_image_collection<float, double> & moving,
    const planar_image_collection<float, double> & reference ){
    
    if(moving.images.empty() || reference.images.empty()){
        throw std::invalid_argument("Cannot resample: image collection is empty");
    }
    
    // Create output image collection matching reference geometry
    planar_image_collection<float, double> resampled;
    
    const float oob = std::numeric_limits<float>::quiet_NaN();
    for(const auto &ref_img : reference.images){
        // Create a new image with the same geometry as the reference
        planar_image<float, double> new_img = ref_img;
        
        // Clear the pixel data and initialize to zero (or NaN for out-of-bounds)
        for(auto &val : new_img.data){
            val = oob;
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
                    const auto val = moving.trilinearly_interpolate(pos, chnl, oob);
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
planar_image_collection<float, double>
AlignViaDemonsHelpers::histogram_match(
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

// ---- buffer3-native internal helpers ----

// Apply 3D Gaussian smoothing in-place to a buffer3 vector field (3 channels).
void
AlignViaDemonsHelpers::smooth_vector_field(
    buffer3<double> & field_buf,
    double sigma_mm,
    work_queue<std::function<void()>> &wq ){
    
    if(field_buf.N_slices == 0) return;
    if(sigma_mm <= 0.0) return;
    if(field_buf.N_channels != 3){
        throw std::invalid_argument("Vector field smoothing requires 3-channel buffer");
    }
    field_buf.gaussian_smooth(sigma_mm, wq);
}

// planar_image_collection wrapper for smooth_vector_field.
void
AlignViaDemonsHelpers::smooth_vector_field(
    planar_image_collection<double, double> & field,
    double sigma_mm ){
    
    if(field.images.empty()) return;
    if(sigma_mm <= 0.0) return;
    
    for(const auto &img : field.images){
        if(img.channels != 3){
            throw std::invalid_argument("Vector field smoothing requires 3-channel images");
        }
    }
    
    auto buf = buffer3<double>::from_planar_image_collection(field);
    const auto n_threads = std::max(1u, std::thread::hardware_concurrency());
    work_queue<std::function<void()>> wq(n_threads);
    smooth_vector_field(buf, sigma_mm, wq);
    buf.write_to_planar_image_collection(field);
}


// buffer3-native compute_gradient.
buffer3<double>
AlignViaDemonsHelpers::compute_gradient(const buffer3<float> & buf){
    
    const int64_t N_slices = buf.N_slices;
    const int64_t N_rows = buf.N_rows;
    const int64_t N_cols = buf.N_cols;

    if(N_slices == 0 || N_rows == 0 || N_cols == 0){
        throw std::invalid_argument("Cannot compute gradient: buffer is empty");
    }

    buffer3<double> grad;
    grad.N_slices = N_slices;
    grad.N_rows = N_rows;
    grad.N_cols = N_cols;
    grad.N_channels = 3;
    grad.pxl_dx = buf.pxl_dx;
    grad.pxl_dy = buf.pxl_dy;
    grad.pxl_dz = buf.pxl_dz;
    grad.anchor = buf.anchor;
    grad.offset = buf.offset;
    grad.row_unit = buf.row_unit;
    grad.col_unit = buf.col_unit;
    grad.slice_offsets = buf.slice_offsets;
    grad.data.resize(static_cast<size_t>(N_slices) * N_rows * N_cols * 3, 0.0);

    for(int64_t s = 0; s < N_slices; ++s){
        for(int64_t row = 0; row < N_rows; ++row){
            for(int64_t col = 0; col < N_cols; ++col){
                
                // Gradient in x-direction (along columns)
                double grad_x = 0.0;
                if(col > 0 && col < N_cols - 1){
                    const double val_left = buf.value(s, row, col - 1, 0);
                    const double val_right = buf.value(s, row, col + 1, 0);
                    if(std::isfinite(val_left) && std::isfinite(val_right)){
                        grad_x = (val_right - val_left) / (2.0 * buf.pxl_dx);
                    }
                }else if(col == 0 && N_cols > 1){
                    const double val_curr = buf.value(s, row, col, 0);
                    const double val_right = buf.value(s, row, col + 1, 0);
                    if(std::isfinite(val_curr) && std::isfinite(val_right)){
                        grad_x = (val_right - val_curr) / buf.pxl_dx;
                    }
                }else if(col == N_cols - 1 && N_cols > 1){
                    const double val_left = buf.value(s, row, col - 1, 0);
                    const double val_curr = buf.value(s, row, col, 0);
                    if(std::isfinite(val_left) && std::isfinite(val_curr)){
                        grad_x = (val_curr - val_left) / buf.pxl_dx;
                    }
                }
                
                // Gradient in y-direction (along rows)
                double grad_y = 0.0;
                if(row > 0 && row < N_rows - 1){
                    const double val_up = buf.value(s, row - 1, col, 0);
                    const double val_down = buf.value(s, row + 1, col, 0);
                    if(std::isfinite(val_up) && std::isfinite(val_down)){
                        grad_y = (val_down - val_up) / (2.0 * buf.pxl_dy);
                    }
                }else if(row == 0 && N_rows > 1){
                    const double val_curr = buf.value(s, row, col, 0);
                    const double val_down = buf.value(s, row + 1, col, 0);
                    if(std::isfinite(val_curr) && std::isfinite(val_down)){
                        grad_y = (val_down - val_curr) / buf.pxl_dy;
                    }
                }else if(row == N_rows - 1 && N_rows > 1){
                    const double val_up = buf.value(s, row - 1, col, 0);
                    const double val_curr = buf.value(s, row, col, 0);
                    if(std::isfinite(val_up) && std::isfinite(val_curr)){
                        grad_y = (val_curr - val_up) / buf.pxl_dy;
                    }
                }
                
                // Gradient in z-direction (between slices)
                double grad_z = 0.0;
                if(N_slices > 1){
                    if(s > 0 && s < N_slices - 1){
                        const double val_prev = buf.value(s - 1, row, col, 0);
                        const double val_next = buf.value(s + 1, row, col, 0);
                        if(std::isfinite(val_prev) && std::isfinite(val_next)){
                            grad_z = (val_next - val_prev) / (2.0 * buf.pxl_dz);
                        }
                    }else if(s == 0){
                        const double val_curr = buf.value(s, row, col, 0);
                        const double val_next = buf.value(s + 1, row, col, 0);
                        if(std::isfinite(val_curr) && std::isfinite(val_next)){
                            grad_z = (val_next - val_curr) / buf.pxl_dz;
                        }
                    }else if(s == N_slices - 1){
                        const double val_prev = buf.value(s - 1, row, col, 0);
                        const double val_curr = buf.value(s, row, col, 0);
                        if(std::isfinite(val_prev) && std::isfinite(val_curr)){
                            grad_z = (val_curr - val_prev) / buf.pxl_dz;
                        }
                    }
                }
                
                grad.reference(s, row, col, 0) = grad_x;
                grad.reference(s, row, col, 1) = grad_y;
                grad.reference(s, row, col, 2) = grad_z;
            }
        }
    }
    
    return grad;
}

// planar_image_collection wrapper for compute_gradient.
// Converts to buffer3 internally, computes, then writes back matching original ordering.
planar_image_collection<double, double>
AlignViaDemonsHelpers::compute_gradient(const planar_image_collection<float, double> & img_coll){
    if(img_coll.images.empty()){
        throw std::invalid_argument("Cannot compute gradient: image collection is empty");
    }
    auto buf = buffer3<float>::from_planar_image_collection(img_coll);
    auto grad = compute_gradient(buf);

    // Build output collection matching the geometry of img_coll.
    // Allocate a collection with the right spatial metadata, then write buffer3 data back.
    planar_image_collection<double, double> gradient_coll;
    for(const auto &img : img_coll.images){
        planar_image<double, double> grad_img;
        grad_img.init_orientation(img.row_unit, img.col_unit);
        grad_img.init_buffer(img.rows, img.columns, 3);
        grad_img.init_spatial(img.pxl_dx, img.pxl_dy, img.pxl_dz, img.anchor, img.offset);
        grad_img.metadata = img.metadata;
        for(auto &val : grad_img.data) val = 0.0;
        gradient_coll.images.push_back(std::move(grad_img));
    }
    grad.write_to_planar_image_collection(gradient_coll);
    return gradient_coll;
}


// buffer3-native warp: warp an image buffer using a deformation field buffer.
buffer3<float>
AlignViaDemonsHelpers::warp_image_with_field(
    const buffer3<float> & img_buf,
    const buffer3<double> & def_field_buf ){
    
    if(img_buf.N_slices == 0 || img_buf.N_rows == 0 || img_buf.N_cols == 0){
        throw std::invalid_argument("Cannot warp: image buffer is empty");
    }
    
    buffer3<float> warped = img_buf;
    const float oob = std::numeric_limits<float>::quiet_NaN();
    
    for(int64_t s = 0; s < warped.N_slices; ++s){
        for(int64_t row = 0; row < warped.N_rows; ++row){
            for(int64_t col = 0; col < warped.N_cols; ++col){
                const auto pos = warped.position(s, row, col);
                
                // Look up displacement from the deformation field buffer.
                const double dx = def_field_buf.trilinear_interpolate(pos, 0, 0.0);
                const double dy = def_field_buf.trilinear_interpolate(pos, 1, 0.0);
                const double dz = def_field_buf.trilinear_interpolate(pos, 2, 0.0);
                const vec3<double> warped_pos = pos + vec3<double>(dx, dy, dz);
                
                for(int64_t chnl = 0; chnl < warped.N_channels; ++chnl){
                    warped.reference(s, row, col, chnl) = img_buf.trilinear_interpolate(warped_pos, chnl, oob);
                }
            }
        }
    }
    
    return warped;
}

// planar_image_collection wrapper for warp_image_with_field.
planar_image_collection<float, double>
AlignViaDemonsHelpers::warp_image_with_field(
    const planar_image_collection<float, double> & img_coll,
    const deformation_field & def_field ){
    
    if(img_coll.images.empty()){
        throw std::invalid_argument("Cannot warp: image collection is empty");
    }
    
    // Build a spatial index for the source images so that trilinear interpolation
    // (with proper bilinear in-plane sampling) is used instead of the
    // planar_image_collection::trilinearly_interpolate() fallback, which degrades
    // to nearest-neighbour when only one image plane is present.
    const auto img_unit = img_coll.images.front().ortho_unit();
    // img_coll is const, but planar_image_adjacency needs non-const refs internally.
    // We cast away const only for the index — no mutation occurs during interpolation.
    auto &img_coll_nc = const_cast<planar_image_collection<float, double> &>(img_coll);
    planar_image_adjacency<float, double> img_adj( {}, { { std::ref(img_coll_nc) } }, img_unit );

    planar_image_collection<float, double> warped = img_coll;
    const float oob = std::numeric_limits<float>::quiet_NaN();
    
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
                    const auto val = img_adj.trilinearly_interpolate(warped_pos, chnl, oob);
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
        const auto n_threads = std::max(1u, std::thread::hardware_concurrency());
        work_queue<std::function<void()>> wq(n_threads);

        // Step 1: Resample moving image to stationary image's grid
        // This handles different orientations and alignments
        if(params.verbosity >= 1){
            YLOGINFO("Resampling moving image to reference grid");
        }
        auto moving_coll = resample_image_to_reference_grid(moving_in, stationary);
        
        // Step 2: Apply histogram matching if requested
        if(params.use_histogram_matching){
            if(params.verbosity >= 1){
                YLOGINFO("Applying histogram matching");
            }
            moving_coll = histogram_match(moving_coll, stationary, params.histogram_bins, params.histogram_outlier_fraction);
        }
        
        // Convert to buffer3 for the entire iterative loop.
        auto stationary_buf = buffer3<float>::from_planar_image_collection(stationary);
        auto moving_buf = buffer3<float>::from_planar_image_collection(moving_coll);

        const int64_t N_slices = stationary_buf.N_slices;
        const int64_t N_rows = stationary_buf.N_rows;
        const int64_t N_cols = stationary_buf.N_cols;

        // Step 3: Initialize deformation field buffer (zero displacement, 3 channels)
        buffer3<double> def_buf;
        def_buf.N_slices = N_slices;
        def_buf.N_rows = N_rows;
        def_buf.N_cols = N_cols;
        def_buf.N_channels = 3;
        def_buf.pxl_dx = stationary_buf.pxl_dx;
        def_buf.pxl_dy = stationary_buf.pxl_dy;
        def_buf.pxl_dz = stationary_buf.pxl_dz;
        def_buf.anchor = stationary_buf.anchor;
        def_buf.offset = stationary_buf.offset;
        def_buf.row_unit = stationary_buf.row_unit;
        def_buf.col_unit = stationary_buf.col_unit;
        def_buf.slice_offsets = stationary_buf.slice_offsets;
        def_buf.data.resize(static_cast<size_t>(N_slices) * N_rows * N_cols * 3, 0.0);
        
        // Step 4: Iterative demons algorithm
        auto warped_buf = moving_buf;
        double prev_mse = std::numeric_limits<double>::infinity();
        
        // Precompute gradient of the stationary (fixed) image; it does not change across iterations.
        auto grad_buf = compute_gradient(stationary_buf);
        
        for(int64_t iter = 0; iter < params.max_iterations; ++iter){
            
            // Compute intensity difference and update field
            double mse = 0.0;
            int64_t n_voxels = 0;
            
            buffer3<double> update_buf;
            update_buf.N_slices = N_slices;
            update_buf.N_rows = N_rows;
            update_buf.N_cols = N_cols;
            update_buf.N_channels = 3;
            update_buf.pxl_dx = def_buf.pxl_dx;
            update_buf.pxl_dy = def_buf.pxl_dy;
            update_buf.pxl_dz = def_buf.pxl_dz;
            update_buf.anchor = def_buf.anchor;
            update_buf.offset = def_buf.offset;
            update_buf.row_unit = def_buf.row_unit;
            update_buf.col_unit = def_buf.col_unit;
            update_buf.slice_offsets = def_buf.slice_offsets;
            update_buf.data.resize(static_cast<size_t>(N_slices) * N_rows * N_cols * 3, 0.0);
            
            for(int64_t s = 0; s < N_slices; ++s){
                for(int64_t row = 0; row < N_rows; ++row){
                    for(int64_t col = 0; col < N_cols; ++col){
                        const double fixed_val = stationary_buf.value(s, row, col, 0);
                        const double moving_val = warped_buf.value(s, row, col, 0);
                        
                        if(!std::isfinite(fixed_val) || !std::isfinite(moving_val)){
                            continue;
                        }
                        
                        const double diff = fixed_val - moving_val;
                        mse += diff * diff;
                        ++n_voxels;
                        
                        const double gx = grad_buf.value(s, row, col, 0);
                        const double gy = grad_buf.value(s, row, col, 1);
                        const double gz = grad_buf.value(s, row, col, 2);
                        
                        const double grad_mag_sq = gx * gx + gy * gy + gz * gz;
                        const double denom = grad_mag_sq + (diff * diff) / (params.normalization_factor + epsilon);
                        
                        if(denom > epsilon){
                            double ux = diff * gx / denom;
                            double uy = diff * gy / denom;
                            double uz = diff * gz / denom;
                            
                            const double umag = std::sqrt(ux * ux + uy * uy + uz * uz);
                            if(umag > params.max_update_magnitude){
                                const double scale = params.max_update_magnitude / umag;
                                ux *= scale;
                                uy *= scale;
                                uz *= scale;
                            }
                            
                            update_buf.reference(s, row, col, 0) = ux;
                            update_buf.reference(s, row, col, 1) = uy;
                            update_buf.reference(s, row, col, 2) = uz;
                        }
                    }
                }
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
                smooth_vector_field(update_buf, params.update_field_smoothing_sigma, wq);
            }
            
            // Add/compose update to deformation field
            if(params.use_diffeomorphic){
                // Diffeomorphic demons: compose d <- d o u
                // new_d(p) = d(p) + u(p + d(p))
                buffer3<double> new_def_buf = def_buf;
                for(int64_t s = 0; s < N_slices; ++s){
                    for(int64_t row = 0; row < N_rows; ++row){
                        for(int64_t col = 0; col < N_cols; ++col){
                            const auto pos = def_buf.position(s, row, col);
                            
                            const double dx = def_buf.value(s, row, col, 0);
                            const double dy = def_buf.value(s, row, col, 1);
                            const double dz = def_buf.value(s, row, col, 2);
                            
                            const vec3<double> deformed_pos = pos + vec3<double>(dx, dy, dz);
                            
                            const double upd_dx = update_buf.trilinear_interpolate(deformed_pos, 0, 0.0);
                            const double upd_dy = update_buf.trilinear_interpolate(deformed_pos, 1, 0.0);
                            const double upd_dz = update_buf.trilinear_interpolate(deformed_pos, 2, 0.0);
                            
                            new_def_buf.reference(s, row, col, 0) = dx + upd_dx;
                            new_def_buf.reference(s, row, col, 1) = dy + upd_dy;
                            new_def_buf.reference(s, row, col, 2) = dz + upd_dz;
                        }
                    }
                }
                def_buf = std::move(new_def_buf);
            }else{
                // Standard demons: simple element-wise addition
                for(size_t i = 0; i < def_buf.data.size(); ++i){
                    def_buf.data[i] += update_buf.data[i];
                }
            }
            
            // Smooth deformation field for regularization
            if(params.deformation_field_smoothing_sigma > 0.0){
                smooth_vector_field(def_buf, params.deformation_field_smoothing_sigma, wq);
            }
            
            // Warp moving image with updated deformation field (all in buffer3)
            warped_buf = warp_image_with_field(moving_buf, def_buf);
        }
        
        // Convert final deformation field buffer back to planar_image_collection,
        // preserving the original image ordering and metadata from stationary.
        planar_image_collection<double, double> deformation_field_images;
        for(const auto &img : stationary.images){
            planar_image<double, double> def_img;
            def_img.init_orientation(img.row_unit, img.col_unit);
            def_img.init_buffer(img.rows, img.columns, 3);
            def_img.init_spatial(img.pxl_dx, img.pxl_dy, img.pxl_dz, img.anchor, img.offset);
            def_img.metadata = img.metadata;
            for(auto &val : def_img.data) val = 0.0;
            deformation_field_images.images.push_back(std::move(def_img));
        }
        def_buf.write_to_planar_image_collection(deformation_field_images);
        
        return deformation_field(std::move(deformation_field_images));
        
    }catch(const std::exception &e){
        YLOGWARN("Demons registration failed: " << e.what());
        return std::nullopt;
    }
}
