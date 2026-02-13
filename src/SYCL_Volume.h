//SYCL_Volume.h - A part of DICOMautomaton 2026. Written by hal clark.
//
// This header defines device-compatible structures for volumetric images used in SYCL kernels.
// The SyclVolume class represents a 3D rectilinear image array suitable for GPU computation.

#pragma once

#include <cstdint>
#include <cmath>
#include <vector>
#include <list>
#include <functional>
#include <stdexcept>
#include <limits>

// When external SYCL is available, include the real SYCL header; otherwise use mock.
#if defined(DCMA_USE_EXT_SYCL) && DCMA_USE_EXT_SYCL
    #include <sycl/sycl.hpp>
#else
    #include "Mock_SYCL.h"
#endif

#include "YgorMath.h"
#include "YgorImages.h"


// Device-compatible 3D vector structure for use within SYCL kernels.
// This is a simplified version of vec3<double> suitable for device code.
struct SyclVec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    SyclVec3() = default;
    SyclVec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    // Conversion from Ygor vec3
    explicit SyclVec3(const vec3<double> &v) : x(v.x), y(v.y), z(v.z) {}

    // Conversion back to Ygor vec3
    vec3<double> to_vec3() const { return vec3<double>(x, y, z); }

    SyclVec3 operator+(const SyclVec3 &o) const {
        return SyclVec3(x + o.x, y + o.y, z + o.z);
    }

    SyclVec3 operator-(const SyclVec3 &o) const {
        return SyclVec3(x - o.x, y - o.y, z - o.z);
    }

    SyclVec3 operator*(double s) const {
        return SyclVec3(x * s, y * s, z * s);
    }

    double dot(const SyclVec3 &o) const {
        return x * o.x + y * o.y + z * o.z;
    }

    double length() const {
        return std::sqrt(x * x + y * y + z * z);
    }
};


// Device-compatible volumetric image metadata.
// This structure holds all the spatial information needed to map between
// voxel indices and world coordinates, and to perform interpolation.
struct SyclVolumeMetadata {
    // Dimensions of the volume (number of voxels in each direction).
    int64_t dim_x = 0;  // columns (fastest changing index)
    int64_t dim_y = 0;  // rows
    int64_t dim_z = 0;  // slices (slowest changing index)
    int64_t channels = 1;

    // Voxel spacing in world units (typically mm).
    double spacing_x = 1.0;
    double spacing_y = 1.0;
    double spacing_z = 1.0;

    // Origin: the world position of the center of voxel (0, 0, 0).
    SyclVec3 origin;

    // Orientation vectors (unit vectors in world space).
    SyclVec3 row_unit;   // Direction along x-axis (columns)
    SyclVec3 col_unit;   // Direction along y-axis (rows)
    SyclVec3 slice_unit; // Direction along z-axis (slices)

    // Total number of voxels.
    int64_t total_voxels() const {
        return dim_x * dim_y * dim_z;
    }

    // Total number of data elements (voxels * channels).
    int64_t total_elements() const {
        return dim_x * dim_y * dim_z * channels;
    }

    // Convert voxel indices to a linear buffer index (row-major: z * (dim_y * dim_x) + y * dim_x + x).
    int64_t linear_index(int64_t x, int64_t y, int64_t z, int64_t c = 0) const {
        return ((z * dim_y + y) * dim_x + x) * channels + c;
    }

    // Convert voxel indices (ix, iy, iz) to world position.
    SyclVec3 voxel_to_world(int64_t ix, int64_t iy, int64_t iz) const {
        return SyclVec3(
            origin.x + static_cast<double>(ix) * spacing_x * row_unit.x
                     + static_cast<double>(iy) * spacing_y * col_unit.x
                     + static_cast<double>(iz) * spacing_z * slice_unit.x,
            origin.y + static_cast<double>(ix) * spacing_x * row_unit.y
                     + static_cast<double>(iy) * spacing_y * col_unit.y
                     + static_cast<double>(iz) * spacing_z * slice_unit.y,
            origin.z + static_cast<double>(ix) * spacing_x * row_unit.z
                     + static_cast<double>(iy) * spacing_y * col_unit.z
                     + static_cast<double>(iz) * spacing_z * slice_unit.z
        );
    }

    // Convert world position to fractional voxel indices.
    // Returns (fx, fy, fz) where the fractional indices can be used for interpolation.
    void world_to_voxel_frac(const SyclVec3 &pos, double &fx, double &fy, double &fz) const {
        // Compute displacement from origin.
        const SyclVec3 diff = pos - origin;
        // Project onto each axis.
        fx = diff.dot(row_unit) / spacing_x;
        fy = diff.dot(col_unit) / spacing_y;
        fz = diff.dot(slice_unit) / spacing_z;
    }

    // Check if fractional indices are within bounds.
    bool in_bounds(double fx, double fy, double fz) const {
        return fx >= 0.0 && fx < static_cast<double>(dim_x) &&
               fy >= 0.0 && fy < static_cast<double>(dim_y) &&
               fz >= 0.0 && fz < static_cast<double>(dim_z);
    }
};


// Helper class for managing volumetric data on the host side.
// This class handles marshaling between planar_image_collection and contiguous device buffers.
template <typename T>
class SyclVolume {
public:
    SyclVolumeMetadata meta;
    std::vector<T> data;

    SyclVolume() = default;

    // Construct from a planar_image_collection.
    // The collection must represent a rectilinear grid.
    explicit SyclVolume(const planar_image_collection<T, double> &pic, int64_t channel = 0) {
        if (pic.images.empty()) {
            throw std::invalid_argument("Cannot create SyclVolume from empty image collection");
        }

        // Validate grid geometry upfront.
        // Note: Images_Form_Regular_Grid takes non-const references but doesn't modify images,
        // so const_cast is safe here.
        std::list<std::reference_wrapper<planar_image<T, double>>> selected_imgs;
        for (const auto &img : pic.images) {
            selected_imgs.push_back(std::ref(const_cast<planar_image<T, double>&>(img)));
        }
        if (!Images_Form_Regular_Grid(selected_imgs)) {
            throw std::invalid_argument("Images do not form a rectilinear grid. Cannot create SyclVolume.");
        }

        // Verify all images have the same dimensions and spacing.
        const auto &first_img = pic.images.front();
        meta.dim_x = first_img.columns;
        meta.dim_y = first_img.rows;
        meta.dim_z = static_cast<int64_t>(pic.images.size());
        meta.channels = 1; // We extract a single channel.

        meta.spacing_x = first_img.pxl_dx;
        meta.spacing_y = first_img.pxl_dy;
        meta.spacing_z = first_img.pxl_dz;

        meta.row_unit = SyclVec3(first_img.row_unit);
        meta.col_unit = SyclVec3(first_img.col_unit);
        meta.slice_unit = SyclVec3(first_img.ortho_unit());

        // Origin is the position of the first voxel (row=0, col=0) of the first slice.
        const vec3<double> origin_pos = first_img.position(0, 0);
        meta.origin = SyclVec3(origin_pos);

        // Allocate and copy data in Z-Y-X order (slice-major).
        data.resize(meta.total_elements());

        int64_t slice = 0;
        for (const auto &img : pic.images) {
            if (img.columns != meta.dim_x || img.rows != meta.dim_y) {
                throw std::invalid_argument("Image dimensions are not consistent in the collection");
            }
            for (int64_t row = 0; row < meta.dim_y; ++row) {
                for (int64_t col = 0; col < meta.dim_x; ++col) {
                    const int64_t idx = meta.linear_index(col, row, slice, 0);
                    data[idx] = img.value(row, col, channel);
                }
            }
            ++slice;
        }
    }

    // Create SyclVolume with 3 channels (for vector fields like deformation or gradient).
    static SyclVolume from_vector_field(const planar_image_collection<T, double> &pic) {
        if (pic.images.empty()) {
            throw std::invalid_argument("Cannot create SyclVolume from empty image collection");
        }

        // Validate grid geometry upfront.
        // Note: Images_Form_Regular_Grid takes non-const references but doesn't modify images,
        // so const_cast is safe here.
        std::list<std::reference_wrapper<planar_image<T, double>>> selected_imgs;
        for (const auto &img : pic.images) {
            selected_imgs.push_back(std::ref(const_cast<planar_image<T, double>&>(img)));
        }
        if (!Images_Form_Regular_Grid(selected_imgs)) {
            throw std::invalid_argument("Images do not form a rectilinear grid. Cannot create SyclVolume.");
        }

        SyclVolume vol;
        const auto &first_img = pic.images.front();

        if (first_img.channels != 3) {
            throw std::invalid_argument("Vector field must have 3 channels");
        }

        vol.meta.dim_x = first_img.columns;
        vol.meta.dim_y = first_img.rows;
        vol.meta.dim_z = static_cast<int64_t>(pic.images.size());
        vol.meta.channels = 3;

        vol.meta.spacing_x = first_img.pxl_dx;
        vol.meta.spacing_y = first_img.pxl_dy;
        vol.meta.spacing_z = first_img.pxl_dz;

        vol.meta.row_unit = SyclVec3(first_img.row_unit);
        vol.meta.col_unit = SyclVec3(first_img.col_unit);
        vol.meta.slice_unit = SyclVec3(first_img.ortho_unit());

        const vec3<double> origin_pos = first_img.position(0, 0);
        vol.meta.origin = SyclVec3(origin_pos);

        vol.data.resize(vol.meta.total_elements());

        int64_t slice = 0;
        for (const auto &img : pic.images) {
            for (int64_t row = 0; row < vol.meta.dim_y; ++row) {
                for (int64_t col = 0; col < vol.meta.dim_x; ++col) {
                    for (int64_t c = 0; c < 3; ++c) {
                        const int64_t idx = vol.meta.linear_index(col, row, slice, c);
                        vol.data[idx] = img.value(row, col, c);
                    }
                }
            }
            ++slice;
        }

        return vol;
    }

    // Marshal back to a planar_image_collection (single channel).
    planar_image_collection<T, double> to_planar_image_collection() const {
        planar_image_collection<T, double> pic;

        for (int64_t z = 0; z < meta.dim_z; ++z) {
            planar_image<T, double> img;
            img.init_orientation(meta.row_unit.to_vec3(), meta.col_unit.to_vec3());
            img.init_buffer(meta.dim_y, meta.dim_x, meta.channels);

            // Compute the offset for this slice.
            const vec3<double> slice_offset = meta.slice_unit.to_vec3() * (static_cast<double>(z) * meta.spacing_z);
            img.init_spatial(meta.spacing_x, meta.spacing_y, meta.spacing_z,
                             meta.origin.to_vec3(), slice_offset);

            for (int64_t row = 0; row < meta.dim_y; ++row) {
                for (int64_t col = 0; col < meta.dim_x; ++col) {
                    for (int64_t c = 0; c < meta.channels; ++c) {
                        const int64_t idx = meta.linear_index(col, row, z, c);
                        img.reference(row, col, c) = data[idx];
                    }
                }
            }

            pic.images.push_back(std::move(img));
        }

        return pic;
    }

    // Trilinear interpolation at a world position.
    // Returns oob_val if the position is outside the volume.
    T trilinear_interp(const SyclVec3 &pos, int64_t channel, T oob_val) const {
        double fx, fy, fz;
        meta.world_to_voxel_frac(pos, fx, fy, fz);

        // Check bounds with a small margin for numerical stability.
        if (fx < -0.5 || fx >= static_cast<double>(meta.dim_x) - 0.5 ||
            fy < -0.5 || fy >= static_cast<double>(meta.dim_y) - 0.5 ||
            fz < -0.5 || fz >= static_cast<double>(meta.dim_z) - 0.5) {
            return oob_val;
        }

        // Clamp to valid range for interpolation.
        fx = std::max(0.0, std::min(fx, static_cast<double>(meta.dim_x - 1)));
        fy = std::max(0.0, std::min(fy, static_cast<double>(meta.dim_y - 1)));
        fz = std::max(0.0, std::min(fz, static_cast<double>(meta.dim_z - 1)));

        const int64_t x0 = static_cast<int64_t>(fx);
        const int64_t y0 = static_cast<int64_t>(fy);
        const int64_t z0 = static_cast<int64_t>(fz);

        const int64_t x1 = std::min(x0 + 1, meta.dim_x - 1);
        const int64_t y1 = std::min(y0 + 1, meta.dim_y - 1);
        const int64_t z1 = std::min(z0 + 1, meta.dim_z - 1);

        const double xd = fx - static_cast<double>(x0);
        const double yd = fy - static_cast<double>(y0);
        const double zd = fz - static_cast<double>(z0);

        // Get the 8 corner values.
        const T c000 = data[meta.linear_index(x0, y0, z0, channel)];
        const T c100 = data[meta.linear_index(x1, y0, z0, channel)];
        const T c010 = data[meta.linear_index(x0, y1, z0, channel)];
        const T c110 = data[meta.linear_index(x1, y1, z0, channel)];
        const T c001 = data[meta.linear_index(x0, y0, z1, channel)];
        const T c101 = data[meta.linear_index(x1, y0, z1, channel)];
        const T c011 = data[meta.linear_index(x0, y1, z1, channel)];
        const T c111 = data[meta.linear_index(x1, y1, z1, channel)];

        // Trilinear interpolation.
        const T c00 = c000 * (1.0 - xd) + c100 * xd;
        const T c01 = c001 * (1.0 - xd) + c101 * xd;
        const T c10 = c010 * (1.0 - xd) + c110 * xd;
        const T c11 = c011 * (1.0 - xd) + c111 * xd;

        const T c0 = c00 * (1.0 - yd) + c10 * yd;
        const T c1 = c01 * (1.0 - yd) + c11 * yd;

        return c0 * (1.0 - zd) + c1 * zd;
    }
};


// Device-side helper functions for SYCL kernels.
// These use raw pointers and metadata structures that can be passed to kernels.

// Trilinear interpolation for device code.
// The data pointer and metadata must be accessible from the device.
template <typename T>
inline T sycl_trilinear_interp(
    const T* data,
    const SyclVolumeMetadata &meta,
    double fx, double fy, double fz,
    int64_t channel,
    T oob_val) {

    // Check bounds.
    if (fx < -0.5 || fx >= static_cast<double>(meta.dim_x) - 0.5 ||
        fy < -0.5 || fy >= static_cast<double>(meta.dim_y) - 0.5 ||
        fz < -0.5 || fz >= static_cast<double>(meta.dim_z) - 0.5) {
        return oob_val;
    }

    // Clamp to valid range for interpolation.
    const double max_x = static_cast<double>(meta.dim_x - 1);
    const double max_y = static_cast<double>(meta.dim_y - 1);
    const double max_z = static_cast<double>(meta.dim_z - 1);
    fx = std::max(0.0, std::min(fx, max_x));
    fy = std::max(0.0, std::min(fy, max_y));
    fz = std::max(0.0, std::min(fz, max_z));

    const int64_t x0 = static_cast<int64_t>(fx);
    const int64_t y0 = static_cast<int64_t>(fy);
    const int64_t z0 = static_cast<int64_t>(fz);

    const int64_t x1 = (x0 + 1 < meta.dim_x) ? x0 + 1 : x0;
    const int64_t y1 = (y0 + 1 < meta.dim_y) ? y0 + 1 : y0;
    const int64_t z1 = (z0 + 1 < meta.dim_z) ? z0 + 1 : z0;

    const double xd = fx - static_cast<double>(x0);
    const double yd = fy - static_cast<double>(y0);
    const double zd = fz - static_cast<double>(z0);

    auto idx = [&](int64_t x, int64_t y, int64_t z) {
        return ((z * meta.dim_y + y) * meta.dim_x + x) * meta.channels + channel;
    };

    const T c000 = data[idx(x0, y0, z0)];
    const T c100 = data[idx(x1, y0, z0)];
    const T c010 = data[idx(x0, y1, z0)];
    const T c110 = data[idx(x1, y1, z0)];
    const T c001 = data[idx(x0, y0, z1)];
    const T c101 = data[idx(x1, y0, z1)];
    const T c011 = data[idx(x0, y1, z1)];
    const T c111 = data[idx(x1, y1, z1)];

    const T c00 = c000 * (1.0 - xd) + c100 * xd;
    const T c01 = c001 * (1.0 - xd) + c101 * xd;
    const T c10 = c010 * (1.0 - xd) + c110 * xd;
    const T c11 = c011 * (1.0 - xd) + c111 * xd;

    const T c0 = c00 * (1.0 - yd) + c10 * yd;
    const T c1 = c01 * (1.0 - yd) + c11 * yd;

    return c0 * (1.0 - zd) + c1 * zd;
}

