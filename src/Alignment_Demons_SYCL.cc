//Alignment_Demons_SYCL.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file implements SYCL-accelerated functions for the Demons registration algorithm.

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <stdexcept>
#include <cstdint>

// Include SYCL header (real or mock).
#if defined(DCMA_USE_EXT_SYCL) && DCMA_USE_EXT_SYCL
    #include <sycl/sycl.hpp>
#else
    #include "Mock_SYCL.h"
#endif

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"

#include "Alignment_Demons.h"
#include "Alignment_Demons_SYCL.h"
#include "SYCL_Volume.h"

// Alignment_Field.h is only needed for the full AlignViaDemons_SYCL function.
// When building standalone tests, this may not be available.
#ifdef DCMA_FULL_BUILD
#include "Alignment_Field.h"
#endif

namespace AlignViaDemonsSYCL {


// SYCL-accelerated gradient computation.
SyclVolume<double> compute_gradient_sycl(const SyclVolume<float> &vol) {
    SyclVolume<double> gradient;
    gradient.meta = vol.meta;
    gradient.meta.channels = 3;
    gradient.data.resize(gradient.meta.total_elements(), 0.0);

    const int64_t dim_x = vol.meta.dim_x;
    const int64_t dim_y = vol.meta.dim_y;
    const int64_t dim_z = vol.meta.dim_z;
    const double spacing_x = vol.meta.spacing_x;
    const double spacing_y = vol.meta.spacing_y;
    const double spacing_z = vol.meta.spacing_z;

    sycl::queue q;

    // Create buffers. Cast away const for read-only buffer (safe since we only read).
    sycl::buffer<float, 1> src_buf(const_cast<float*>(vol.data.data()), sycl::range<1>(vol.data.size()));
    sycl::buffer<double, 1> grad_buf(gradient.data.data(), sycl::range<1>(gradient.data.size()));

    q.submit([&](sycl::handler &h) {
        sycl::accessor src_acc(src_buf, h);
        sycl::accessor grad_acc(grad_buf, h);

        h.parallel_for(sycl::range<3>(dim_z, dim_y, dim_x), [=](sycl::id<3> idx) {
            const int64_t z = idx[0];
            const int64_t y = idx[1];
            const int64_t x = idx[2];

            // Helper lambda to compute linear index for single-channel source.
            auto src_idx = [=](int64_t ix, int64_t iy, int64_t iz) {
                return iz * (dim_y * dim_x) + iy * dim_x + ix;
            };

            // Helper lambda to compute linear index for 3-channel gradient.
            auto grad_idx = [=](int64_t ix, int64_t iy, int64_t iz, int64_t c) {
                return ((iz * dim_y + iy) * dim_x + ix) * 3 + c;
            };

            // Compute gradient in x direction (central differences).
            double grad_x = 0.0;
            if (x > 0 && x < dim_x - 1) {
                const float left = src_acc[src_idx(x - 1, y, z)];
                const float right = src_acc[src_idx(x + 1, y, z)];
                grad_x = static_cast<double>(right - left) / (2.0 * spacing_x);
            } else if (x == 0 && dim_x > 1) {
                const float curr = src_acc[src_idx(x, y, z)];
                const float right = src_acc[src_idx(x + 1, y, z)];
                grad_x = static_cast<double>(right - curr) / spacing_x;
            } else if (x == dim_x - 1 && dim_x > 1) {
                const float left = src_acc[src_idx(x - 1, y, z)];
                const float curr = src_acc[src_idx(x, y, z)];
                grad_x = static_cast<double>(curr - left) / spacing_x;
            }

            // Compute gradient in y direction.
            double grad_y = 0.0;
            if (y > 0 && y < dim_y - 1) {
                const float up = src_acc[src_idx(x, y - 1, z)];
                const float down = src_acc[src_idx(x, y + 1, z)];
                grad_y = static_cast<double>(down - up) / (2.0 * spacing_y);
            } else if (y == 0 && dim_y > 1) {
                const float curr = src_acc[src_idx(x, y, z)];
                const float down = src_acc[src_idx(x, y + 1, z)];
                grad_y = static_cast<double>(down - curr) / spacing_y;
            } else if (y == dim_y - 1 && dim_y > 1) {
                const float up = src_acc[src_idx(x, y - 1, z)];
                const float curr = src_acc[src_idx(x, y, z)];
                grad_y = static_cast<double>(curr - up) / spacing_y;
            }

            // Compute gradient in z direction.
            double grad_z = 0.0;
            if (z > 0 && z < dim_z - 1) {
                const float prev = src_acc[src_idx(x, y, z - 1)];
                const float next = src_acc[src_idx(x, y, z + 1)];
                grad_z = static_cast<double>(next - prev) / (2.0 * spacing_z);
            } else if (z == 0 && dim_z > 1) {
                const float curr = src_acc[src_idx(x, y, z)];
                const float next = src_acc[src_idx(x, y, z + 1)];
                grad_z = static_cast<double>(next - curr) / spacing_z;
            } else if (z == dim_z - 1 && dim_z > 1) {
                const float prev = src_acc[src_idx(x, y, z - 1)];
                const float curr = src_acc[src_idx(x, y, z)];
                grad_z = static_cast<double>(curr - prev) / spacing_z;
            }

            grad_acc[grad_idx(x, y, z, 0)] = grad_x;
            grad_acc[grad_idx(x, y, z, 1)] = grad_y;
            grad_acc[grad_idx(x, y, z, 2)] = grad_z;
        });
    });

    q.wait();
    return gradient;
}


// SYCL-accelerated Gaussian smoothing of a vector field.
void smooth_vector_field_sycl(SyclVolume<double> &field, double sigma_mm) {
    if (sigma_mm <= 0.0) {
        return;
    }

    if (field.meta.channels != 3) {
        throw std::invalid_argument("Vector field smoothing requires 3-channel data");
    }

    const int64_t dim_x = field.meta.dim_x;
    const int64_t dim_y = field.meta.dim_y;
    const int64_t dim_z = field.meta.dim_z;

    // Calculate kernel radii based on sigma and spacing.
    const double sigma_x = sigma_mm / field.meta.spacing_x;
    const double sigma_y = sigma_mm / field.meta.spacing_y;
    const double sigma_z = sigma_mm / field.meta.spacing_z;

    const int64_t radius_x = std::max<int64_t>(1, static_cast<int64_t>(3.0 * sigma_x));
    const int64_t radius_y = std::max<int64_t>(1, static_cast<int64_t>(3.0 * sigma_y));
    const int64_t radius_z = std::max<int64_t>(1, static_cast<int64_t>(3.0 * sigma_z));

    // Precompute 1D Gaussian kernels.
    auto make_kernel = [](int64_t radius, double sigma) -> std::vector<double> {
        std::vector<double> kernel(2 * radius + 1);
        double sum = 0.0;
        for (int64_t i = -radius; i <= radius; ++i) {
            const double val = std::exp(-0.5 * static_cast<double>(i * i) / (sigma * sigma));
            kernel[i + radius] = val;
            sum += val;
        }
        for (auto &v : kernel) v /= sum;
        return kernel;
    };

    const std::vector<double> kernel_x = make_kernel(radius_x, sigma_x);
    const std::vector<double> kernel_y = make_kernel(radius_y, sigma_y);
    const std::vector<double> kernel_z = make_kernel(radius_z, sigma_z);

    // Allocate temporary buffer for intermediate results.
    std::vector<double> temp(field.data.size());

    sycl::queue q;

    // Apply separable filtering: X -> Y -> Z.
    // Each pass is done independently for all three channels.

    // Pass 1: Smooth along X.
    {
        sycl::buffer<double, 1> src_buf(field.data.data(), sycl::range<1>(field.data.size()));
        sycl::buffer<double, 1> dst_buf(temp.data(), sycl::range<1>(temp.size()));
        sycl::buffer<double, 1> kern_buf(const_cast<double*>(kernel_x.data()), sycl::range<1>(kernel_x.size()));

        q.submit([&](sycl::handler &h) {
            sycl::accessor src(src_buf, h);
            sycl::accessor dst(dst_buf, h);
            sycl::accessor kern(kern_buf, h);

            const int64_t rad = radius_x;

            h.parallel_for(sycl::range<3>(dim_z, dim_y, dim_x), [=](sycl::id<3> idx) {
                const int64_t z = idx[0];
                const int64_t y = idx[1];
                const int64_t x = idx[2];

                auto linear = [=](int64_t ix, int64_t iy, int64_t iz, int64_t c) {
                    return ((iz * dim_y + iy) * dim_x + ix) * 3 + c;
                };

                for (int64_t c = 0; c < 3; ++c) {
                    double sum = 0.0;
                    double weight_sum = 0.0;

                    for (int64_t k = -rad; k <= rad; ++k) {
                        const int64_t nx = x + k;
                        if (nx >= 0 && nx < dim_x) {
                            const double w = kern[k + rad];
                            const double val = src[linear(nx, y, z, c)];
                            sum += w * val;
                            weight_sum += w;
                        }
                    }

                    dst[linear(x, y, z, c)] = (weight_sum > 0.0) ? sum / weight_sum : 0.0;
                }
            });
        });
        q.wait();
    }

    // Pass 2: Smooth along Y.
    {
        sycl::buffer<double, 1> src_buf(temp.data(), sycl::range<1>(temp.size()));
        sycl::buffer<double, 1> dst_buf(field.data.data(), sycl::range<1>(field.data.size()));
        sycl::buffer<double, 1> kern_buf(const_cast<double*>(kernel_y.data()), sycl::range<1>(kernel_y.size()));

        q.submit([&](sycl::handler &h) {
            sycl::accessor src(src_buf, h);
            sycl::accessor dst(dst_buf, h);
            sycl::accessor kern(kern_buf, h);

            const int64_t rad = radius_y;

            h.parallel_for(sycl::range<3>(dim_z, dim_y, dim_x), [=](sycl::id<3> idx) {
                const int64_t z = idx[0];
                const int64_t y = idx[1];
                const int64_t x = idx[2];

                auto linear = [=](int64_t ix, int64_t iy, int64_t iz, int64_t c) {
                    return ((iz * dim_y + iy) * dim_x + ix) * 3 + c;
                };

                for (int64_t c = 0; c < 3; ++c) {
                    double sum = 0.0;
                    double weight_sum = 0.0;

                    for (int64_t k = -rad; k <= rad; ++k) {
                        const int64_t ny = y + k;
                        if (ny >= 0 && ny < dim_y) {
                            const double w = kern[k + rad];
                            const double val = src[linear(x, ny, z, c)];
                            sum += w * val;
                            weight_sum += w;
                        }
                    }

                    dst[linear(x, y, z, c)] = (weight_sum > 0.0) ? sum / weight_sum : 0.0;
                }
            });
        });
        q.wait();
    }

    // Pass 3: Smooth along Z.
    {
        sycl::buffer<double, 1> src_buf(field.data.data(), sycl::range<1>(field.data.size()));
        sycl::buffer<double, 1> dst_buf(temp.data(), sycl::range<1>(temp.size()));
        sycl::buffer<double, 1> kern_buf(const_cast<double*>(kernel_z.data()), sycl::range<1>(kernel_z.size()));

        q.submit([&](sycl::handler &h) {
            sycl::accessor src(src_buf, h);
            sycl::accessor dst(dst_buf, h);
            sycl::accessor kern(kern_buf, h);

            const int64_t rad = radius_z;

            h.parallel_for(sycl::range<3>(dim_z, dim_y, dim_x), [=](sycl::id<3> idx) {
                const int64_t z = idx[0];
                const int64_t y = idx[1];
                const int64_t x = idx[2];

                auto linear = [=](int64_t ix, int64_t iy, int64_t iz, int64_t c) {
                    return ((iz * dim_y + iy) * dim_x + ix) * 3 + c;
                };

                for (int64_t c = 0; c < 3; ++c) {
                    double sum = 0.0;
                    double weight_sum = 0.0;

                    for (int64_t k = -rad; k <= rad; ++k) {
                        const int64_t nz = z + k;
                        if (nz >= 0 && nz < dim_z) {
                            const double w = kern[k + rad];
                            const double val = src[linear(x, y, nz, c)];
                            sum += w * val;
                            weight_sum += w;
                        }
                    }

                    dst[linear(x, y, z, c)] = (weight_sum > 0.0) ? sum / weight_sum : 0.0;
                }
            });
        });
        q.wait();
    }

    // Copy result back.
    field.data = std::move(temp);
}


// SYCL-accelerated image warping using a deformation field.
SyclVolume<float> warp_image_sycl(
    const SyclVolume<float> &source_vol,
    const SyclVolume<double> &deformation_vol) {

    SyclVolume<float> warped;
    warped.meta = source_vol.meta;
    warped.data.resize(warped.meta.total_elements());

    const int64_t dim_x = source_vol.meta.dim_x;
    const int64_t dim_y = source_vol.meta.dim_y;
    const int64_t dim_z = source_vol.meta.dim_z;

    const SyclVolumeMetadata src_meta = source_vol.meta;
    const SyclVolumeMetadata def_meta = deformation_vol.meta;

    const float oob = std::numeric_limits<float>::quiet_NaN();

    sycl::queue q;

    // Cast away const for read-only buffers (safe since we only read from them).
    sycl::buffer<float, 1> src_buf(const_cast<float*>(source_vol.data.data()), sycl::range<1>(source_vol.data.size()));
    sycl::buffer<double, 1> def_buf(const_cast<double*>(deformation_vol.data.data()), sycl::range<1>(deformation_vol.data.size()));
    sycl::buffer<float, 1> out_buf(warped.data.data(), sycl::range<1>(warped.data.size()));

    q.submit([&](sycl::handler &h) {
        sycl::accessor src(src_buf, h);
        sycl::accessor def(def_buf, h);
        sycl::accessor out(out_buf, h);

        h.parallel_for(sycl::range<3>(dim_z, dim_y, dim_x), [=](sycl::id<3> idx) {
            const int64_t z = idx[0];
            const int64_t y = idx[1];
            const int64_t x = idx[2];

            const int64_t out_idx = z * (dim_y * dim_x) + y * dim_x + x;

            // Get the deformation at this voxel.
            auto def_linear = [=](int64_t ix, int64_t iy, int64_t iz, int64_t c) {
                return ((iz * def_meta.dim_y + iy) * def_meta.dim_x + ix) * 3 + c;
            };

            const double dx = def[def_linear(x, y, z, 0)];
            const double dy = def[def_linear(x, y, z, 1)];
            const double dz = def[def_linear(x, y, z, 2)];

            // Compute world position of this voxel.
            const double wx = src_meta.origin.x 
                + static_cast<double>(x) * src_meta.spacing_x * src_meta.row_unit.x
                + static_cast<double>(y) * src_meta.spacing_y * src_meta.col_unit.x
                + static_cast<double>(z) * src_meta.spacing_z * src_meta.slice_unit.x;
            const double wy = src_meta.origin.y 
                + static_cast<double>(x) * src_meta.spacing_x * src_meta.row_unit.y
                + static_cast<double>(y) * src_meta.spacing_y * src_meta.col_unit.y
                + static_cast<double>(z) * src_meta.spacing_z * src_meta.slice_unit.y;
            const double wz = src_meta.origin.z 
                + static_cast<double>(x) * src_meta.spacing_x * src_meta.row_unit.z
                + static_cast<double>(y) * src_meta.spacing_y * src_meta.col_unit.z
                + static_cast<double>(z) * src_meta.spacing_z * src_meta.slice_unit.z;

            // Apply deformation to get source position.
            const double swx = wx + dx;
            const double swy = wy + dy;
            const double swz = wz + dz;

            // Convert back to fractional voxel coordinates.
            const double diff_x = swx - src_meta.origin.x;
            const double diff_y = swy - src_meta.origin.y;
            const double diff_z = swz - src_meta.origin.z;

            const double fx = (diff_x * src_meta.row_unit.x + diff_y * src_meta.row_unit.y + diff_z * src_meta.row_unit.z) / src_meta.spacing_x;
            const double fy = (diff_x * src_meta.col_unit.x + diff_y * src_meta.col_unit.y + diff_z * src_meta.col_unit.z) / src_meta.spacing_y;
            const double fz = (diff_x * src_meta.slice_unit.x + diff_y * src_meta.slice_unit.y + diff_z * src_meta.slice_unit.z) / src_meta.spacing_z;

            // Trilinear interpolation.
            if (fx < -0.5 || fx >= static_cast<double>(dim_x) - 0.5 ||
                fy < -0.5 || fy >= static_cast<double>(dim_y) - 0.5 ||
                fz < -0.5 || fz >= static_cast<double>(dim_z) - 0.5) {
                out[out_idx] = oob;
                return;
            }

            // Clamp to valid range for interpolation.
            auto clamp_val = [](double v, double lo, double hi) {
                return (v < lo) ? lo : ((v > hi) ? hi : v);
            };
            const double max_x = static_cast<double>(dim_x - 1);
            const double max_y = static_cast<double>(dim_y - 1);
            const double max_z = static_cast<double>(dim_z - 1);
            const double cfx = clamp_val(fx, 0.0, max_x);
            const double cfy = clamp_val(fy, 0.0, max_y);
            const double cfz = clamp_val(fz, 0.0, max_z);

            const int64_t x0 = static_cast<int64_t>(cfx);
            const int64_t y0 = static_cast<int64_t>(cfy);
            const int64_t z0 = static_cast<int64_t>(cfz);

            const int64_t x1 = (x0 + 1 < dim_x) ? x0 + 1 : x0;
            const int64_t y1 = (y0 + 1 < dim_y) ? y0 + 1 : y0;
            const int64_t z1 = (z0 + 1 < dim_z) ? z0 + 1 : z0;

            const double xd = cfx - static_cast<double>(x0);
            const double yd = cfy - static_cast<double>(y0);
            const double zd = cfz - static_cast<double>(z0);

            auto src_linear = [=](int64_t ix, int64_t iy, int64_t iz) {
                return iz * (dim_y * dim_x) + iy * dim_x + ix;
            };

            const float c000 = src[src_linear(x0, y0, z0)];
            const float c100 = src[src_linear(x1, y0, z0)];
            const float c010 = src[src_linear(x0, y1, z0)];
            const float c110 = src[src_linear(x1, y1, z0)];
            const float c001 = src[src_linear(x0, y0, z1)];
            const float c101 = src[src_linear(x1, y0, z1)];
            const float c011 = src[src_linear(x0, y1, z1)];
            const float c111 = src[src_linear(x1, y1, z1)];

            const float c00 = c000 * static_cast<float>(1.0 - xd) + c100 * static_cast<float>(xd);
            const float c01 = c001 * static_cast<float>(1.0 - xd) + c101 * static_cast<float>(xd);
            const float c10 = c010 * static_cast<float>(1.0 - xd) + c110 * static_cast<float>(xd);
            const float c11 = c011 * static_cast<float>(1.0 - xd) + c111 * static_cast<float>(xd);

            const float c0 = c00 * static_cast<float>(1.0 - yd) + c10 * static_cast<float>(yd);
            const float c1 = c01 * static_cast<float>(1.0 - yd) + c11 * static_cast<float>(yd);

            out[out_idx] = c0 * static_cast<float>(1.0 - zd) + c1 * static_cast<float>(zd);
        });
    });

    q.wait();
    return warped;
}


// SYCL-accelerated demons iteration.
double compute_demons_iteration_sycl(
    const SyclVolume<float> &stationary_vol,
    SyclVolume<float> &warped_moving_vol,
    const SyclVolume<double> &gradient_vol,
    SyclVolume<double> &deformation_vol,
    const AlignViaDemonsParams &params) {

    constexpr double epsilon = 1e-10;

    const int64_t dim_x = stationary_vol.meta.dim_x;
    const int64_t dim_y = stationary_vol.meta.dim_y;
    const int64_t dim_z = stationary_vol.meta.dim_z;
    const int64_t total_voxels = dim_x * dim_y * dim_z;

    // Create update field.
    SyclVolume<double> update_field;
    update_field.meta = deformation_vol.meta;
    update_field.data.resize(update_field.meta.total_elements(), 0.0);

    // Accumulator for MSE computation (reduction).
    std::vector<double> mse_accum(total_voxels, 0.0);
    std::vector<int64_t> count_accum(total_voxels, 0);

    const double normalization_factor = params.normalization_factor;
    const double max_update_magnitude = params.max_update_magnitude;

    sycl::queue q;

    // Compute update field and MSE.
    {
        // Cast away const for read-only buffers (safe since we only read from them).
        sycl::buffer<float, 1> stat_buf(const_cast<float*>(stationary_vol.data.data()), sycl::range<1>(stationary_vol.data.size()));
        sycl::buffer<float, 1> mov_buf(warped_moving_vol.data.data(), sycl::range<1>(warped_moving_vol.data.size()));
        sycl::buffer<double, 1> grad_buf(const_cast<double*>(gradient_vol.data.data()), sycl::range<1>(gradient_vol.data.size()));
        sycl::buffer<double, 1> upd_buf(update_field.data.data(), sycl::range<1>(update_field.data.size()));
        sycl::buffer<double, 1> mse_buf(mse_accum.data(), sycl::range<1>(mse_accum.size()));
        sycl::buffer<int64_t, 1> cnt_buf(count_accum.data(), sycl::range<1>(count_accum.size()));

        q.submit([&](sycl::handler &h) {
            sycl::accessor stat(stat_buf, h);
            sycl::accessor mov(mov_buf, h);
            sycl::accessor grad(grad_buf, h);
            sycl::accessor upd(upd_buf, h);
            sycl::accessor mse_acc(mse_buf, h);
            sycl::accessor cnt_acc(cnt_buf, h);

            h.parallel_for(sycl::range<3>(dim_z, dim_y, dim_x), [=](sycl::id<3> idx) {
                const int64_t z = idx[0];
                const int64_t y = idx[1];
                const int64_t x = idx[2];

                const int64_t lin_idx = z * (dim_y * dim_x) + y * dim_x + x;

                auto grad_linear = [=](int64_t ix, int64_t iy, int64_t iz, int64_t c) {
                    return ((iz * dim_y + iy) * dim_x + ix) * 3 + c;
                };

                const float fixed_val = stat[lin_idx];
                const float moving_val = mov[lin_idx];

                // Check for valid values.
                if (!std::isfinite(fixed_val) || !std::isfinite(moving_val)) {
                    upd[grad_linear(x, y, z, 0)] = 0.0;
                    upd[grad_linear(x, y, z, 1)] = 0.0;
                    upd[grad_linear(x, y, z, 2)] = 0.0;
                    mse_acc[lin_idx] = 0.0;
                    cnt_acc[lin_idx] = 0;
                    return;
                }

                const double diff = static_cast<double>(fixed_val - moving_val);
                mse_acc[lin_idx] = diff * diff;
                cnt_acc[lin_idx] = 1;

                const double grad_x = grad[grad_linear(x, y, z, 0)];
                const double grad_y = grad[grad_linear(x, y, z, 1)];
                const double grad_z = grad[grad_linear(x, y, z, 2)];

                const double grad_mag_sq = grad_x * grad_x + grad_y * grad_y + grad_z * grad_z;
                const double denom = grad_mag_sq + (diff * diff) / (normalization_factor + epsilon);

                double update_x = 0.0;
                double update_y = 0.0;
                double update_z = 0.0;

                if (denom > epsilon) {
                    update_x = diff * grad_x / denom;
                    update_y = diff * grad_y / denom;
                    update_z = diff * grad_z / denom;

                    // Clamp update magnitude.
                    const double update_mag = std::sqrt(update_x * update_x + update_y * update_y + update_z * update_z);
                    if (update_mag > max_update_magnitude) {
                        const double scale = max_update_magnitude / update_mag;
                        update_x *= scale;
                        update_y *= scale;
                        update_z *= scale;
                    }
                }

                upd[grad_linear(x, y, z, 0)] = update_x;
                upd[grad_linear(x, y, z, 1)] = update_y;
                upd[grad_linear(x, y, z, 2)] = update_z;
            });
        });
        q.wait();
    }

    // Compute MSE on host.
    double total_mse = 0.0;
    int64_t total_count = 0;
    for (int64_t i = 0; i < total_voxels; ++i) {
        total_mse += mse_accum[i];
        total_count += count_accum[i];
    }
    const double mse = (total_count > 0) ? total_mse / static_cast<double>(total_count) : 0.0;

    // Smooth update field if using diffeomorphic demons.
    if (params.use_diffeomorphic && params.update_field_smoothing_sigma > 0.0) {
        smooth_vector_field_sycl(update_field, params.update_field_smoothing_sigma);
    }

    // Add update to deformation field.
    // For simplicity, using standard demons (simple addition) for now.
    // Diffeomorphic composition would require additional warping.
    {
        sycl::buffer<double, 1> def_buf(deformation_vol.data.data(), sycl::range<1>(deformation_vol.data.size()));
        sycl::buffer<double, 1> upd_buf(update_field.data.data(), sycl::range<1>(update_field.data.size()));

        q.submit([&](sycl::handler &h) {
            sycl::accessor def(def_buf, h);
            sycl::accessor upd(upd_buf, h);

            const int64_t total_elements = deformation_vol.data.size();

            h.parallel_for(sycl::range<1>(total_elements), [=](sycl::id<1> i) {
                def[i[0]] += upd[i[0]];
            });
        });
        q.wait();
    }

    // Smooth deformation field for regularization.
    if (params.deformation_field_smoothing_sigma > 0.0) {
        smooth_vector_field_sycl(deformation_vol, params.deformation_field_smoothing_sigma);
    }

    // Note: The caller is responsible for warping the moving image using the updated
    // deformation field before the next iteration. This should warp from the original
    // moving image, not the already-warped image, to avoid accumulating interpolation errors.

    return mse;
}


// Complete SYCL-accelerated demons registration.
// This function requires the full DICOMautomaton build environment.
#ifdef DCMA_FULL_BUILD
std::optional<deformation_field> AlignViaDemons_SYCL(
    AlignViaDemonsParams &params,
    const planar_image_collection<float, double> &moving_in,
    const planar_image_collection<float, double> &stationary) {

    if (moving_in.images.empty() || stationary.images.empty()) {
        YLOGWARN("Unable to perform SYCL demons alignment: an image array is empty");
        return std::nullopt;
    }

    try {
        // Resample moving image to stationary grid using CPU implementation.
        // (This is done once at the start and doesn't need SYCL acceleration.)
        if (params.verbosity >= 1) {
            YLOGINFO("SYCL Demons: Resampling moving image to reference grid");
        }
        auto moving = AlignViaDemonsHelpers::resample_image_to_reference_grid(moving_in, stationary);

        // Apply histogram matching if requested.
        if (params.use_histogram_matching) {
            if (params.verbosity >= 1) {
                YLOGINFO("SYCL Demons: Applying histogram matching");
            }
            moving = AlignViaDemonsHelpers::histogram_match(moving, stationary, 
                params.histogram_bins, params.histogram_outlier_fraction);
        }

        // Marshal to SyclVolume.
        if (params.verbosity >= 1) {
            YLOGINFO("SYCL Demons: Marshaling data to device format");
        }
        SyclVolume<float> stationary_vol(stationary, 0);
        SyclVolume<float> moving_vol(moving, 0);

        // Compute gradient of stationary image.
        if (params.verbosity >= 1) {
            YLOGINFO("SYCL Demons: Computing gradient");
        }
        SyclVolume<double> gradient_vol = compute_gradient_sycl(stationary_vol);

        // Initialize deformation field to zero.
        SyclVolume<double> deformation_vol;
        deformation_vol.meta = stationary_vol.meta;
        deformation_vol.meta.channels = 3;
        deformation_vol.data.resize(deformation_vol.meta.total_elements(), 0.0);

        // Initialize warped moving.
        SyclVolume<float> warped_moving = moving_vol;

        // Iterative demons algorithm.
        double prev_mse = std::numeric_limits<double>::infinity();

        for (int64_t iter = 0; iter < params.max_iterations; ++iter) {
            const double mse = compute_demons_iteration_sycl(
                stationary_vol, warped_moving, gradient_vol, deformation_vol, params);

            if (params.verbosity >= 1) {
                YLOGINFO("SYCL Demons iteration " << iter << ": MSE = " << mse);
            }

            // Check for convergence.
            const double mse_change = std::abs(prev_mse - mse);
            if (mse_change < params.convergence_threshold && iter > 0) {
                if (params.verbosity >= 1) {
                    YLOGINFO("SYCL Demons: Converged after " << iter << " iterations");
                }
                break;
            }
            prev_mse = mse;

            // Warp the original moving image using the updated deformation field.
            // This is done from the original moving_vol to avoid accumulating
            // interpolation errors from chaining warps.
            warped_moving = warp_image_sycl(moving_vol, deformation_vol);
        }

        // Marshal deformation field back to planar_image_collection.
        if (params.verbosity >= 1) {
            YLOGINFO("SYCL Demons: Marshaling result back to host format");
        }
        auto def_images = deformation_vol.to_planar_image_collection();

        return deformation_field(std::move(def_images));

    } catch (const std::exception &e) {
        YLOGWARN("SYCL Demons registration failed: " << e.what());
        return std::nullopt;
    }
}
#endif // DCMA_FULL_BUILD

} // namespace AlignViaDemonsSYCL

