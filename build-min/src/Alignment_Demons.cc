//Alignment_Demons.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <algorithm>
#include <exception>
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

#if __has_include(<sycl/sycl.hpp>)
    #include <sycl/sycl.hpp>
    namespace dcma_sycl = sycl;
#elif __has_include(<CL/sycl.hpp>)
    #include <CL/sycl.hpp>
    namespace dcma_sycl = cl::sycl;
#elif __has_include("SYCL_Fallback.h")
    #include "SYCL_Fallback.h"
    namespace dcma_sycl = sycl;
#else
    #error "Unable to find suitable SYCL header file."
#endif

using namespace AlignViaDemonsHelpers;


class sycl_demons_engine {
public:
    sycl_demons_engine(const demons_volume<float> &fixed_in,
                       const demons_volume<float> &moving_in,
                       const AlignViaDemonsParams &params_in)
        : fixed(fixed_in)
        , moving(moving_in)
        , warped(moving_in)
        , params(params_in)
        , q(dcma_sycl::default_selector_v,
            [this](dcma_sycl::exception_list exceptions){
                if(!exceptions.empty()){
                    std::lock_guard<std::mutex> lock(this->async_exception_mutex);
                    this->async_exception = exceptions.front();
                }
            }){

        dcma_sycl::device dev = q.get_device();
        YLOGINFO("Running on device: " << dev.get_info<dcma_sycl::info::device::name>());

        this->initialize_geometry();
        this->allocate_device_memory();
        this->compute_gradient();
    }

    ~sycl_demons_engine(){
        this->release_device_memory();
    }

    double compute_single_iteration(){
        double mse = 0.0;
        int64_t n_voxels = 0;
        this->compute_update_and_mse(mse, n_voxels);

        if(params.use_diffeomorphic && params.update_field_smoothing_sigma > 0.0){
            this->smooth_on_device(this->update_dev, params.update_field_smoothing_sigma);
        }

        if(!params.use_diffeomorphic){
            // Standard demons: simply add the update to the deformation field.
            this->add_update();
        }else{
            // Diffeomorphic demons: compose the update with the current deformation field.
            // This implements d ← d ∘ u (composition of deformation with update).
            // For each voxel position p in the deformation field, we compute:
            //   new_d(p) = d(p) + u(p + d(p))
            // This ensures the transformation remains diffeomorphic (invertible).
            this->add_update_diffeomorphic();
        }

        if(params.deformation_field_smoothing_sigma > 0.0){
            this->smooth_on_device(this->deformation_dev, params.deformation_field_smoothing_sigma);
        }

        this->warp_moving();
        return mse;
    }

    demons_volume<double> export_deformation_volume(){
        this->copy_device_vector_to_host(this->deformation_dev, deformation);
        return deformation;
    }

private:
    demons_volume<float> fixed;
    demons_volume<float> moving;
    demons_volume<float> warped;
    demons_volume<double> gradient;
    demons_volume<double> update;
    demons_volume<double> deformation;
    AlignViaDemonsParams params;
    dcma_sycl::queue q;

    size_t scalar_voxel_count = 0;
    size_t scalar_volume_size = 0;
    size_t vector_volume_size = 0;

    float *fixed_dev = nullptr;
    float *moving_dev = nullptr;
    float *warped_dev = nullptr;
    double *gradient_dev = nullptr;
    double *update_dev = nullptr;
    double *deformation_dev = nullptr;
    double *mse_term_dev = nullptr;
    int64_t *valid_dev = nullptr;
    std::exception_ptr async_exception = nullptr;
    std::mutex async_exception_mutex;

    void wait_and_rethrow(){
        q.wait_and_throw();
        {
            std::lock_guard<std::mutex> lock(this->async_exception_mutex);
            if(this->async_exception){
                const auto ex = this->async_exception;
                this->async_exception = nullptr;
                std::rethrow_exception(ex);
            }
        }
    }

    void initialize_geometry(){
        this->scalar_voxel_count = static_cast<size_t>(fixed.slices * fixed.rows * fixed.cols);
        this->scalar_volume_size = this->scalar_voxel_count * static_cast<size_t>(fixed.channels);
        this->vector_volume_size = this->scalar_voxel_count * 3UL;

        gradient = demons_volume<double>{};
        gradient.slices = fixed.slices;
        gradient.rows = fixed.rows;
        gradient.cols = fixed.cols;
        gradient.channels = 3;
        gradient.pxl_dx = fixed.pxl_dx;
        gradient.pxl_dy = fixed.pxl_dy;
        gradient.pxl_dz = fixed.pxl_dz;
        gradient.data.assign(this->vector_volume_size, 0.0);

        update = gradient;
        deformation = gradient;
    }

    void allocate_device_memory(){
        fixed_dev = dcma_sycl::malloc_shared<float>(this->scalar_volume_size, q);
        moving_dev = dcma_sycl::malloc_shared<float>(this->scalar_volume_size, q);
        warped_dev = dcma_sycl::malloc_shared<float>(this->scalar_volume_size, q);
        gradient_dev = dcma_sycl::malloc_shared<double>(this->vector_volume_size, q);
        update_dev = dcma_sycl::malloc_shared<double>(this->vector_volume_size, q);
        deformation_dev = dcma_sycl::malloc_shared<double>(this->vector_volume_size, q);
        mse_term_dev = dcma_sycl::malloc_shared<double>(this->scalar_voxel_count, q);
        valid_dev = dcma_sycl::malloc_shared<int64_t>(this->scalar_voxel_count, q);

        std::copy(fixed.data.begin(), fixed.data.end(), fixed_dev);
        std::copy(moving.data.begin(), moving.data.end(), moving_dev);
        std::copy(moving.data.begin(), moving.data.end(), warped_dev);
        std::fill(gradient_dev, gradient_dev + this->vector_volume_size, 0.0);
        std::fill(update_dev, update_dev + this->vector_volume_size, 0.0);
        std::fill(deformation_dev, deformation_dev + this->vector_volume_size, 0.0);
    }

    void release_device_memory(){
        if(fixed_dev) dcma_sycl::free(fixed_dev, q);
        if(moving_dev) dcma_sycl::free(moving_dev, q);
        if(warped_dev) dcma_sycl::free(warped_dev, q);
        if(gradient_dev) dcma_sycl::free(gradient_dev, q);
        if(update_dev) dcma_sycl::free(update_dev, q);
        if(deformation_dev) dcma_sycl::free(deformation_dev, q);
        if(mse_term_dev) dcma_sycl::free(mse_term_dev, q);
        if(valid_dev) dcma_sycl::free(valid_dev, q);
        fixed_dev = nullptr;
        moving_dev = nullptr;
        warped_dev = nullptr;
        gradient_dev = nullptr;
        update_dev = nullptr;
        deformation_dev = nullptr;
        mse_term_dev = nullptr;
        valid_dev = nullptr;
    }

    void compute_gradient(){
        // Compute gradient in index space (intensity per pixel), not physical space.
        // This ensures dimensional consistency in the demons update formula.
        // The update is later scaled by pixel spacing when stored in the deformation field.
        const int64_t slices = fixed.slices;
        const int64_t rows = fixed.rows;
        const int64_t cols = fixed.cols;
        const int64_t channels = fixed.channels;

        q.parallel_for(dcma_sycl::range<3>(static_cast<size_t>(slices),
                                           static_cast<size_t>(rows),
                                           static_cast<size_t>(cols)),
                       [=](dcma_sycl::id<3> id){
            const int64_t z = static_cast<int64_t>(id[0]);
            const int64_t y = static_cast<int64_t>(id[1]);
            const int64_t x = static_cast<int64_t>(id[2]);

            const auto in_idx = [=](int64_t zz, int64_t yy, int64_t xx){
                return (((zz * rows + yy) * cols + xx) * channels);
            };
            const auto out_idx = [=](int64_t c){
                return (((z * rows + y) * cols + x) * 3 + c);
            };

            double gx = 0.0, gy = 0.0, gz = 0.0;
            if(cols > 1){
                const int64_t xl = (x > 0) ? (x - 1) : x;
                const int64_t xr = (x + 1 < cols) ? (x + 1) : x;
                const float vl = fixed_dev[in_idx(z, y, xl)];
                const float vr = fixed_dev[in_idx(z, y, xr)];
                if(dcma_sycl::isfinite(vl) && dcma_sycl::isfinite(vr) && xr != xl){
                    // Gradient in index space (intensity per pixel)
                    gx = (static_cast<double>(vr) - static_cast<double>(vl))
                       / static_cast<double>(xr - xl);
                }
            }
            if(rows > 1){
                const int64_t yu = (y > 0) ? (y - 1) : y;
                const int64_t yd = (y + 1 < rows) ? (y + 1) : y;
                const float vu = fixed_dev[in_idx(z, yu, x)];
                const float vd = fixed_dev[in_idx(z, yd, x)];
                if(dcma_sycl::isfinite(vu) && dcma_sycl::isfinite(vd) && yd != yu){
                    // Gradient in index space (intensity per pixel)
                    gy = (static_cast<double>(vd) - static_cast<double>(vu))
                       / static_cast<double>(yd - yu);
                }
            }
            if(slices > 1){
                const int64_t zp = (z > 0) ? (z - 1) : z;
                const int64_t zn = (z + 1 < slices) ? (z + 1) : z;
                const float vp = fixed_dev[in_idx(zp, y, x)];
                const float vn = fixed_dev[in_idx(zn, y, x)];
                if(dcma_sycl::isfinite(vp) && dcma_sycl::isfinite(vn) && zn != zp){
                    // Gradient in index space (intensity per pixel)
                    gz = (static_cast<double>(vn) - static_cast<double>(vp))
                       / static_cast<double>(zn - zp);
                }
            }

            gradient_dev[out_idx(0)] = gx;
            gradient_dev[out_idx(1)] = gy;
            gradient_dev[out_idx(2)] = gz;
        });
        this->wait_and_rethrow();
    }

    void compute_update_and_mse(double &mse, int64_t &n_voxels){
        constexpr double epsilon = 1.0e-10;
        const int64_t slices = fixed.slices;
        const int64_t rows = fixed.rows;
        const int64_t cols = fixed.cols;
        const int64_t channels = fixed.channels;
        const double normalization = params.normalization_factor;
        const double max_update = params.max_update_magnitude;

        q.parallel_for(dcma_sycl::range<3>(static_cast<size_t>(slices),
                                           static_cast<size_t>(rows),
                                           static_cast<size_t>(cols)),
                       [=](dcma_sycl::id<3> id){
            const int64_t z = static_cast<int64_t>(id[0]);
            const int64_t y = static_cast<int64_t>(id[1]);
            const int64_t x = static_cast<int64_t>(id[2]);

            const auto vox_idx = static_cast<size_t>((z * rows + y) * cols + x);
            const auto f_idx = static_cast<size_t>(((z * rows + y) * cols + x) * channels);
            const auto g_idx = static_cast<size_t>(((z * rows + y) * cols + x) * 3);
            const float fixed_val = fixed_dev[f_idx];
            const float moving_val = warped_dev[f_idx];

            if(!(dcma_sycl::isfinite(fixed_val) && dcma_sycl::isfinite(moving_val))){
                mse_term_dev[vox_idx] = 0.0;
                valid_dev[vox_idx] = 0;
                update_dev[g_idx + 0] = 0.0;
                update_dev[g_idx + 1] = 0.0;
                update_dev[g_idx + 2] = 0.0;
                return;
            }

            const double diff = static_cast<double>(fixed_val) - static_cast<double>(moving_val);
            mse_term_dev[vox_idx] = diff * diff;
            valid_dev[vox_idx] = 1;

            double ux = 0.0, uy = 0.0, uz = 0.0;
            const double gx = gradient_dev[g_idx + 0];
            const double gy = gradient_dev[g_idx + 1];
            const double gz = gradient_dev[g_idx + 2];
            const double gmag_sq = gx * gx + gy * gy + gz * gz;
            const double denom = gmag_sq + (diff * diff) / (normalization + epsilon);
            if(denom > epsilon){
                ux = diff * gx / denom;
                uy = diff * gy / denom;
                uz = diff * gz / denom;
                const double mag = dcma_sycl::sqrt(ux * ux + uy * uy + uz * uz);
                if(mag > max_update){
                    const double scale = max_update / mag;
                    ux *= scale;
                    uy *= scale;
                    uz *= scale;
                }
            }
            update_dev[g_idx + 0] = ux;
            update_dev[g_idx + 1] = uy;
            update_dev[g_idx + 2] = uz;
        });
        this->wait_and_rethrow();

        mse = 0.0;
        n_voxels = 0;
        for(size_t i = 0; i < this->scalar_voxel_count; ++i){
            mse += mse_term_dev[i];
            n_voxels += valid_dev[i];
        }
        if(n_voxels > 0){
            mse /= static_cast<double>(n_voxels);
        }
    }

    void add_update(){
        // Scale the update from index space (pixels) to physical space (mm) and add to deformation.
        const int64_t slices = fixed.slices;
        const int64_t rows = fixed.rows;
        const int64_t cols = fixed.cols;
        const double pxl_dx = fixed.pxl_dx;
        const double pxl_dy = fixed.pxl_dy;
        const double pxl_dz = fixed.pxl_dz;

        q.parallel_for(dcma_sycl::range<3>(static_cast<size_t>(slices),
                                           static_cast<size_t>(rows),
                                           static_cast<size_t>(cols)),
                       [=](dcma_sycl::id<3> id){
            const int64_t z = static_cast<int64_t>(id[0]);
            const int64_t y = static_cast<int64_t>(id[1]);
            const int64_t x = static_cast<int64_t>(id[2]);
            const auto idx = static_cast<size_t>(((z * rows + y) * cols + x) * 3);

            // Update is in index space; convert to physical units before adding.
            deformation_dev[idx + 0] += update_dev[idx + 0] * pxl_dx;
            deformation_dev[idx + 1] += update_dev[idx + 1] * pxl_dy;
            deformation_dev[idx + 2] += update_dev[idx + 2] * pxl_dz;
        });
        this->wait_and_rethrow();
    }

    void add_update_diffeomorphic(){
        // Diffeomorphic demons: compose the update with the current deformation field.
        // For each voxel position p, compute: new_d(p) = d(p) + u(p + d(p))
        // where u is sampled at the deformed position using trilinear interpolation.
        // The update field is in index space and must be scaled to physical units.
        const int64_t slices = fixed.slices;
        const int64_t rows = fixed.rows;
        const int64_t cols = fixed.cols;
        const double pxl_dx = fixed.pxl_dx;
        const double pxl_dy = fixed.pxl_dy;
        const double pxl_dz = fixed.pxl_dz;
        // For 2D images (single slice), skip z interpolation and use bilinear only.
        const bool is_2d = (slices == 1);

        q.parallel_for(dcma_sycl::range<3>(static_cast<size_t>(slices),
                                           static_cast<size_t>(rows),
                                           static_cast<size_t>(cols)),
                       [=](dcma_sycl::id<3> id){
            const int64_t z = static_cast<int64_t>(id[0]);
            const int64_t y = static_cast<int64_t>(id[1]);
            const int64_t x = static_cast<int64_t>(id[2]);
            const auto vidx = [=](int64_t zz, int64_t yy, int64_t xx, int64_t c) -> size_t {
                return static_cast<size_t>(((zz * rows + yy) * cols + xx) * 3 + c);
            };

            // Get the current deformation at this voxel (in physical units, mm).
            const double dx = deformation_dev[vidx(z, y, x, 0)];
            const double dy = deformation_dev[vidx(z, y, x, 1)];
            const double dz = deformation_dev[vidx(z, y, x, 2)];

            // Compute the deformed position in index space.
            // The deformation is in physical units; convert to index space for sampling.
            const double sx = static_cast<double>(x) + dx / pxl_dx;
            const double sy = static_cast<double>(y) + dy / pxl_dy;
            const double sz = is_2d ? static_cast<double>(z) : (static_cast<double>(z) + dz / pxl_dz);

            // Sample the update field at the deformed position using trilinear interpolation.
            // If the deformed position is out of bounds, use zero update.
            auto sample_update = [&](int64_t c) -> double {
                // Floor and ceiling indices for interpolation.
                const int64_t x0 = static_cast<int64_t>(dcma_sycl::floor(sx));
                const int64_t y0 = static_cast<int64_t>(dcma_sycl::floor(sy));
                const int64_t z0 = static_cast<int64_t>(dcma_sycl::floor(sz));
                const int64_t x1 = x0 + 1;
                const int64_t y1 = y0 + 1;
                const int64_t z1 = is_2d ? z0 : (z0 + 1);  // For 2D, z1 = z0 (no z interpolation)

                // Interpolation weights.
                const double tx = sx - static_cast<double>(x0);
                const double ty = sy - static_cast<double>(y0);
                const double tz = is_2d ? 0.0 : (sz - static_cast<double>(z0));

                // Fetch values at the 8 corners, with bounds checking.
                auto fetch = [&](int64_t zz, int64_t yy, int64_t xx) -> double {
                    if(xx < 0 || xx >= cols || yy < 0 || yy >= rows || zz < 0 || zz >= slices){
                        return 0.0;  // Out of bounds: zero update contribution.
                    }
                    return update_dev[vidx(zz, yy, xx, c)];
                };

                const double c000 = fetch(z0, y0, x0);
                const double c100 = fetch(z0, y0, x1);
                const double c010 = fetch(z0, y1, x0);
                const double c110 = fetch(z0, y1, x1);

                // For 2D images (single slice), reuse z0-plane values for z1-plane.
                // Combined with tz=0.0 above, the trilinear formula degrades to bilinear:
                // result = c0*(1-tz) + c1*tz = c0*1 + c1*0 = c0 (no z-blending).
                const float c001 = is_2d ? c000 : fetch(z1, y0, x0);
                const float c101 = is_2d ? c100 : fetch(z1, y0, x1);
                const float c011 = is_2d ? c010 : fetch(z1, y1, x0);
                const float c111 = is_2d ? c110 : fetch(z1, y1, x1);

                // Trilinear interpolation.
                const double c00 = c000 * (1.0 - tx) + c100 * tx;
                const double c10 = c010 * (1.0 - tx) + c110 * tx;
                const double c01 = c001 * (1.0 - tx) + c101 * tx;
                const double c11 = c011 * (1.0 - tx) + c111 * tx;
                const double c0 = c00 * (1.0 - ty) + c10 * ty;
                const double c1 = c01 * (1.0 - ty) + c11 * ty;
                return c0 * (1.0 - tz) + c1 * tz;
            };

            // Sample update at deformed position (in index space).
            const double ux = sample_update(0);
            const double uy = sample_update(1);
            const double uz = sample_update(2);

            // Compose: add the sampled update (scaled to physical units) to the deformation.
            deformation_dev[vidx(z, y, x, 0)] += ux * pxl_dx;
            deformation_dev[vidx(z, y, x, 1)] += uy * pxl_dy;
            deformation_dev[vidx(z, y, x, 2)] += uz * pxl_dz;
        });
        this->wait_and_rethrow();
    }

    void warp_moving(){
        const float out_of_bounds_value = std::numeric_limits<float>::quiet_NaN();
        const int64_t slices = fixed.slices;
        const int64_t rows = fixed.rows;
        const int64_t cols = fixed.cols;
        const int64_t channels = fixed.channels;
        const double pxl_dx = fixed.pxl_dx;
        const double pxl_dy = fixed.pxl_dy;
        const double pxl_dz = fixed.pxl_dz;
        // For 2D images (single slice), skip z interpolation and use bilinear only.
        const bool is_2d = (slices == 1);

        q.parallel_for(dcma_sycl::range<3>(static_cast<size_t>(slices),
                                           static_cast<size_t>(rows),
                                           static_cast<size_t>(cols)),
                       [=](dcma_sycl::id<3> id){
            const int64_t z = static_cast<int64_t>(id[0]);
            const int64_t y = static_cast<int64_t>(id[1]);
            const int64_t x = static_cast<int64_t>(id[2]);

            const auto vidx = [=](int64_t zz, int64_t yy, int64_t xx, int64_t c, int64_t chnl_count){
                return (((zz * rows + yy) * cols + xx) * chnl_count) + c;
            };

            const double dx = deformation_dev[vidx(z, y, x, 0, 3)];
            const double dy = deformation_dev[vidx(z, y, x, 1, 3)];
            const double dz = deformation_dev[vidx(z, y, x, 2, 3)];
            if(!(dcma_sycl::isfinite(dx) && dcma_sycl::isfinite(dy) && dcma_sycl::isfinite(dz))){
                // If any component is invalid, the full displacement vector is unusable for interpolation.
                for(int64_t c = 0; c < channels; ++c){
                    warped_dev[vidx(z, y, x, c, channels)] = out_of_bounds_value;
                }
                return;
            }
            const double sx = static_cast<double>(x) + dx / pxl_dx;
            const double sy = static_cast<double>(y) + dy / pxl_dy;
            const double sz = is_2d ? static_cast<double>(z) : (static_cast<double>(z) + dz / pxl_dz);

            const int64_t x0 = static_cast<int64_t>(dcma_sycl::floor(sx));
            const int64_t y0 = static_cast<int64_t>(dcma_sycl::floor(sy));
            const int64_t z0 = static_cast<int64_t>(dcma_sycl::floor(sz));
            const int64_t x1 = x0 + 1;
            const int64_t y1 = y0 + 1;
            const int64_t z1 = is_2d ? z0 : (z0 + 1);  // For 2D, z1 = z0 (no z interpolation)
            const double tx = sx - static_cast<double>(x0);
            const double ty = sy - static_cast<double>(y0);
            const double tz = is_2d ? 0.0 : (sz - static_cast<double>(z0));

            for(int64_t c = 0; c < channels; ++c){
                auto sample = [&](int64_t zz, int64_t yy, int64_t xx) -> float {
                    if(xx < 0 || xx >= cols || yy < 0 || yy >= rows || zz < 0 || zz >= slices){
                        return out_of_bounds_value;
                    }
                    return moving_dev[vidx(zz, yy, xx, c, channels)];
                };
                const float c000 = sample(z0, y0, x0);
                const float c100 = sample(z0, y0, x1);
                const float c010 = sample(z0, y1, x0);
                const float c110 = sample(z0, y1, x1);
                
                // For 2D images (single slice), reuse z0-plane values for z1-plane.
                // Combined with tz=0.0 above, the trilinear formula degrades to bilinear:
                // result = c0*(1-tz) + c1*tz = c0*1 + c1*0 = c0 (no z-blending).
                const float c001 = is_2d ? c000 : sample(z1, y0, x0);
                const float c101 = is_2d ? c100 : sample(z1, y0, x1);
                const float c011 = is_2d ? c010 : sample(z1, y1, x0);
                const float c111 = is_2d ? c110 : sample(z1, y1, x1);
                
                if(!(dcma_sycl::isfinite(c000) && dcma_sycl::isfinite(c100) && dcma_sycl::isfinite(c010) && dcma_sycl::isfinite(c110)
                  && dcma_sycl::isfinite(c001) && dcma_sycl::isfinite(c101) && dcma_sycl::isfinite(c011) && dcma_sycl::isfinite(c111))){
                    warped_dev[vidx(z, y, x, c, channels)] = out_of_bounds_value;
                    continue;
                }
                const double c00 = static_cast<double>(c000) * (1.0 - tx) + static_cast<double>(c100) * tx;
                const double c10 = static_cast<double>(c010) * (1.0 - tx) + static_cast<double>(c110) * tx;
                const double c01 = static_cast<double>(c001) * (1.0 - tx) + static_cast<double>(c101) * tx;
                const double c11 = static_cast<double>(c011) * (1.0 - tx) + static_cast<double>(c111) * tx;
                const double c0 = c00 * (1.0 - ty) + c10 * ty;
                const double c1 = c01 * (1.0 - ty) + c11 * ty;
                warped_dev[vidx(z, y, x, c, channels)] = static_cast<float>(c0 * (1.0 - tz) + c1 * tz);
            }
        });
        this->wait_and_rethrow();
    }

    void copy_device_vector_to_host(const double *dev, demons_volume<double> &host){
        host.data.assign(dev, dev + this->vector_volume_size);
    }

    void copy_host_vector_to_device(const demons_volume<double> &host, double *dev){
        std::copy(host.data.begin(), host.data.end(), dev);
    }

    // Smooth a vector field on device using separable 3D Gaussian filtering.
    // Allocates a temporary buffer, applies smoothing along each axis in sequence,
    // and writes the result back to the input buffer.
    void smooth_on_device(double *field_dev, double sigma_mm){
        if(sigma_mm <= 0.0){
            return;
        }
        const int64_t slices = fixed.slices;
        const int64_t rows = fixed.rows;
        const int64_t cols = fixed.cols;
        const double pxl_dx = fixed.pxl_dx;
        const double pxl_dy = fixed.pxl_dy;
        const double pxl_dz = fixed.pxl_dz;
        // Compute kernel radii based on 3-sigma rule.
        const int64_t rad_x = std::max<int64_t>(1, static_cast<int64_t>(3.0 * sigma_mm / pxl_dx));
        const int64_t rad_y = std::max<int64_t>(1, static_cast<int64_t>(3.0 * sigma_mm / pxl_dy));
        const int64_t rad_z = std::max<int64_t>(1, static_cast<int64_t>(3.0 * sigma_mm / pxl_dz));
        // Sigma in pixel units for each axis.
        const double sigma_x = sigma_mm / pxl_dx;
        const double sigma_y = sigma_mm / pxl_dy;
        const double sigma_z = sigma_mm / pxl_dz;
        // Allocate temporary buffer on device for intermediate results.
        double *temp_dev = dcma_sycl::malloc_shared<double>(this->vector_volume_size, q);
        if(temp_dev == nullptr){
            throw std::runtime_error("Failed to allocate temporary device buffer for smoothing");
        }
        // Helper lambda to create 1D Gaussian weights (precomputed on host, copied to device).
        auto make_kernel = [](int64_t radius, double sigma_pix) -> std::vector<double> {
            std::vector<double> k(2 * radius + 1);
            double sum = 0.0;
            for(int64_t i = -radius; i <= radius; ++i){
                double v = dcma_sycl::exp(-0.5 * (static_cast<double>(i * i)) / (sigma_pix * sigma_pix));
                k[static_cast<size_t>(i + radius)] = v;
                sum += v;
            }
            for(auto &v : k) v /= sum;
            return k;
        };
        const auto kernel_x = make_kernel(rad_x, sigma_x);
        const auto kernel_y = make_kernel(rad_y, sigma_y);
        const auto kernel_z = make_kernel(rad_z, sigma_z);
        // Copy kernels to device (using shared memory for simplicity).
        double *kx_dev = dcma_sycl::malloc_shared<double>(kernel_x.size(), q);
        double *ky_dev = dcma_sycl::malloc_shared<double>(kernel_y.size(), q);
        double *kz_dev = dcma_sycl::malloc_shared<double>(kernel_z.size(), q);
        std::copy(kernel_x.begin(), kernel_x.end(), kx_dev);
        std::copy(kernel_y.begin(), kernel_y.end(), ky_dev);
        std::copy(kernel_z.begin(), kernel_z.end(), kz_dev);
        // Pass 1: smooth along x (columns).
        q.parallel_for(dcma_sycl::range<3>(static_cast<size_t>(slices),
                                           static_cast<size_t>(rows),
                                           static_cast<size_t>(cols)),
                       [=](dcma_sycl::id<3> id){
            const int64_t z = static_cast<int64_t>(id[0]);
            const int64_t y = static_cast<int64_t>(id[1]);
            const int64_t x = static_cast<int64_t>(id[2]);
            const auto vidx = [=](int64_t zz, int64_t yy, int64_t xx, int64_t c) -> size_t {
                return static_cast<size_t>(((zz * rows + yy) * cols + xx) * 3 + c);
            };
            for(int64_t c = 0; c < 3; ++c){
                double sum = 0.0;
                double wsum = 0.0;
                for(int64_t k = -rad_x; k <= rad_x; ++k){
                    int64_t xx = x + k;
                    if(xx >= 0 && xx < cols){
                        double v = field_dev[vidx(z, y, xx, c)];
                        if(dcma_sycl::isfinite(v)){
                            double w = kx_dev[static_cast<size_t>(k + rad_x)];
                            sum += w * v;
                            wsum += w;
                        }
                    }
                }
                temp_dev[vidx(z, y, x, c)] = (wsum > 0.0) ? (sum / wsum) : field_dev[vidx(z, y, x, c)];
            }
        });
        this->wait_and_rethrow();
        // Pass 2: smooth along y (rows), reading from temp_dev, writing to field_dev.
        q.parallel_for(dcma_sycl::range<3>(static_cast<size_t>(slices),
                                           static_cast<size_t>(rows),
                                           static_cast<size_t>(cols)),
                       [=](dcma_sycl::id<3> id){
            const int64_t z = static_cast<int64_t>(id[0]);
            const int64_t y = static_cast<int64_t>(id[1]);
            const int64_t x = static_cast<int64_t>(id[2]);
            const auto vidx = [=](int64_t zz, int64_t yy, int64_t xx, int64_t c) -> size_t {
                return static_cast<size_t>(((zz * rows + yy) * cols + xx) * 3 + c);
            };
            for(int64_t c = 0; c < 3; ++c){
                double sum = 0.0;
                double wsum = 0.0;
                for(int64_t k = -rad_y; k <= rad_y; ++k){
                    int64_t yy = y + k;
                    if(yy >= 0 && yy < rows){
                        double v = temp_dev[vidx(z, yy, x, c)];
                        if(dcma_sycl::isfinite(v)){
                            double w = ky_dev[static_cast<size_t>(k + rad_y)];
                            sum += w * v;
                            wsum += w;
                        }
                    }
                }
                field_dev[vidx(z, y, x, c)] = (wsum > 0.0) ? (sum / wsum) : temp_dev[vidx(z, y, x, c)];
            }
        });
        this->wait_and_rethrow();
        // Pass 3: smooth along z (slices), reading from field_dev, writing to temp_dev.
        q.parallel_for(dcma_sycl::range<3>(static_cast<size_t>(slices),
                                           static_cast<size_t>(rows),
                                           static_cast<size_t>(cols)),
                       [=](dcma_sycl::id<3> id){
            const int64_t z = static_cast<int64_t>(id[0]);
            const int64_t y = static_cast<int64_t>(id[1]);
            const int64_t x = static_cast<int64_t>(id[2]);
            const auto vidx = [=](int64_t zz, int64_t yy, int64_t xx, int64_t c) -> size_t {
                return static_cast<size_t>(((zz * rows + yy) * cols + xx) * 3 + c);
            };
            for(int64_t c = 0; c < 3; ++c){
                double sum = 0.0;
                double wsum = 0.0;
                for(int64_t k = -rad_z; k <= rad_z; ++k){
                    int64_t zz = z + k;
                    if(zz >= 0 && zz < slices){
                        double v = field_dev[vidx(zz, y, x, c)];
                        if(dcma_sycl::isfinite(v)){
                            double w = kz_dev[static_cast<size_t>(k + rad_z)];
                            sum += w * v;
                            wsum += w;
                        }
                    }
                }
                temp_dev[vidx(z, y, x, c)] = (wsum > 0.0) ? (sum / wsum) : field_dev[vidx(z, y, x, c)];
            }
        });
        this->wait_and_rethrow();
        // Copy result back to field_dev.
        std::copy(temp_dev, temp_dev + this->vector_volume_size, field_dev);
        // Free temporary buffers.
        dcma_sycl::free(temp_dev, q);
        dcma_sycl::free(kx_dev, q);
        dcma_sycl::free(ky_dev, q);
        dcma_sycl::free(kz_dev, q);
    }

};


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



// Helper function to warp an image using a deformation field.
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

        // Step 3: Initialize demons algorithm
        const auto fixed_vol = marshal_collection_to_volume(stationary);
        const auto moving_vol = marshal_collection_to_volume(moving);
        sycl_demons_engine engine(fixed_vol, moving_vol, params);

        // Step 4: Iterative demons algorithm
        double prev_mse = std::numeric_limits<double>::infinity();
        for(int64_t iter = 0; iter < params.max_iterations; ++iter){
            const double mse = engine.compute_single_iteration();
            if(params.verbosity >= 1){
                YLOGINFO("Iteration " << iter << ": MSE = " << mse);
            }

            const double mse_change = std::abs(prev_mse - mse);
            if(mse_change < params.convergence_threshold && iter > 0){
                if(params.verbosity >= 1){
                    YLOGINFO("Converged after " << iter << " iterations");
                }
                break;
            }
            prev_mse = mse;
        }

        auto deformation_vol = engine.export_deformation_volume();
        auto def_coll = marshal_volume_to_collection(deformation_vol, stationary);
        return deformation_field(std::move(def_coll));

    }catch(const std::exception &e){
        YLOGWARN("Demons registration failed: " << e.what());
    }
    return std::nullopt;
}
