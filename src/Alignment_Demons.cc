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

using namespace AlignViaDemonsHelpers;

#ifdef DCMA_WHICH_SYCL
#if defined(DCMA_USE_SYCL_FALLBACK)
#include "SYCL_Fallback.h"
namespace dcma_sycl = sycl;
#elif __has_include(<sycl/sycl.hpp>)
#include <sycl/sycl.hpp>
namespace dcma_sycl = sycl;
#elif __has_include(<CL/sycl.hpp>)
#include <CL/sycl.hpp>
namespace dcma_sycl = cl::sycl;
#else
#error "External SYCL backend was selected, but no SYCL header was found."
#endif

namespace {

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
        this->initialize_geometry();
        this->allocate_device_memory();
        this->compute_gradient();
    }

    ~sycl_demons_engine(){
        this->release_device_memory();
    }

    double compute_single_iteration(const planar_image_collection<float, double> & /*stationary*/){
        double mse = 0.0;
        int64_t n_voxels = 0;
        this->compute_update_and_mse(mse, n_voxels);

        if(params.use_diffeomorphic && params.update_field_smoothing_sigma > 0.0){
            this->smooth_on_device(this->update_dev, params.update_field_smoothing_sigma);
        }

        this->add_update();

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
        const int64_t slices = fixed.slices;
        const int64_t rows = fixed.rows;
        const int64_t cols = fixed.cols;
        const int64_t channels = fixed.channels;
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
                    gx = (static_cast<double>(vr) - static_cast<double>(vl))
                       / (static_cast<double>(xr - xl) * pxl_dx);
                }
            }
            if(rows > 1){
                const int64_t yu = (y > 0) ? (y - 1) : y;
                const int64_t yd = (y + 1 < rows) ? (y + 1) : y;
                const float vu = fixed_dev[in_idx(z, yu, x)];
                const float vd = fixed_dev[in_idx(z, yd, x)];
                if(dcma_sycl::isfinite(vu) && dcma_sycl::isfinite(vd) && yd != yu){
                    gy = (static_cast<double>(vd) - static_cast<double>(vu))
                       / (static_cast<double>(yd - yu) * pxl_dy);
                }
            }
            if(slices > 1){
                const int64_t zp = (z > 0) ? (z - 1) : z;
                const int64_t zn = (z + 1 < slices) ? (z + 1) : z;
                const float vp = fixed_dev[in_idx(zp, y, x)];
                const float vn = fixed_dev[in_idx(zn, y, x)];
                if(dcma_sycl::isfinite(vp) && dcma_sycl::isfinite(vn) && zn != zp){
                    gz = (static_cast<double>(vn) - static_cast<double>(vp))
                       / (static_cast<double>(zn - zp) * pxl_dz);
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
        const size_t N = this->vector_volume_size;
        q.parallel_for(dcma_sycl::range<1>(N), [=](dcma_sycl::id<1> i){
            deformation_dev[i[0]] += update_dev[i[0]];
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
            const double sz = static_cast<double>(z) + dz / pxl_dz;

            const int64_t x0 = static_cast<int64_t>(dcma_sycl::floor(sx));
            const int64_t y0 = static_cast<int64_t>(dcma_sycl::floor(sy));
            const int64_t z0 = static_cast<int64_t>(dcma_sycl::floor(sz));
            const int64_t x1 = x0 + 1;
            const int64_t y1 = y0 + 1;
            const int64_t z1 = z0 + 1;
            const double tx = sx - static_cast<double>(x0);
            const double ty = sy - static_cast<double>(y0);
            const double tz = sz - static_cast<double>(z0);

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
                const float c001 = sample(z1, y0, x0);
                const float c101 = sample(z1, y0, x1);
                const float c011 = sample(z1, y1, x0);
                const float c111 = sample(z1, y1, x1);
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

} // namespace
#endif

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

// Helper function to apply 3D Gaussian smoothing to a vector field.
// The field should have 3 channels representing dx, dy, dz displacements.
void
AlignViaDemonsHelpers::smooth_vector_field(
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
planar_image_collection<double, double>
AlignViaDemonsHelpers::compute_gradient(const planar_image_collection<float, double> & img_coll){
    
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
    
#ifdef DCMA_WHICH_SYCL
    try {
        if(params.verbosity >= 1){
            YLOGINFO("Using SYCL Demons implementation");
        }

        auto moving = resample_image_to_reference_grid(moving_in, stationary);
        if(params.use_histogram_matching){
            moving = histogram_match(moving, stationary, params.histogram_bins, params.histogram_outlier_fraction);
        }

        const auto fixed_vol = marshal_collection_to_volume(stationary);
        const auto moving_vol = marshal_collection_to_volume(moving);
        sycl_demons_engine engine(fixed_vol, moving_vol, params);

        double prev_mse = std::numeric_limits<double>::infinity();
        for(int64_t iter = 0; iter < params.max_iterations; ++iter){
            const double mse = engine.compute_single_iteration(stationary);
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
        YLOGWARN("SYCL Demons path failed (" << e.what() << "). Falling back to CPU implementation. "
                 "Selected SYCL backend: " << DCMA_WHICH_SYCL << ".");
    }
#endif

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
                        
                        // Demons force: u = (diff * gradient) / (grad_mag^2 + diff^2 / normalization)
                        // where diff = fixed - moving. The displacement u points from
                        // positions in the fixed image grid toward corresponding positions
                        // in the moving image, suitable for pull-based warping where
                        // warped(x) = moving(x + u(x)).
                        const double denom = grad_mag_sq + (diff * diff) / (params.normalization_factor + epsilon);
                        
                        if(denom > epsilon){
                            double update_x = diff * grad_x / denom;
                            double update_y = diff * grad_y / denom;
                            double update_z = diff * grad_z / denom;
                            
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
                const double oob = 0.0;
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
                            
                            // Sample update field at the deformed position using
                            // adjacency-based interpolation for proper bilinear sampling.
                            const auto &upd_adj = update_def_field.get_adjacency_crefw().get();
                            const double upd_dx = upd_adj.trilinearly_interpolate(deformed_pos, 0, oob);
                            const double upd_dy = upd_adj.trilinearly_interpolate(deformed_pos, 1, oob);
                            const double upd_dz = upd_adj.trilinearly_interpolate(deformed_pos, 2, oob);
                            
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
