//Alignment_Buffer3.h - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file provides a templated buffer3 class that serves as a contiguous 3D array proxy
// for planar_image_collection. It is designed for fast random access, neighbour traversal,
// and GPU/SIMD-friendly memory layout.
//
// Key design points:
//   - Contiguous memory: data is stored in a flat std::vector in [slice][row][col][channel] order.
//   - O(1) neighbour access: moving to adjacent voxels in any cardinal direction is a pointer offset.
//   - Spatial awareness: voxel positions are computed via member functions (not stored per-voxel).
//   - Marshalling: easy conversion to/from planar_image_collection.
//   - Multithreading: slice-based parallelism via work_queue.

#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <limits>
#include <list>
#include <mutex>

#include "YgorMath.h"
#include "YgorImages.h"
#include "Thread_Pool.h"


template <typename T>
class buffer3 {
  public:

    // Dimensions.
    int64_t N_slices  = 0;
    int64_t N_rows    = 0;
    int64_t N_cols    = 0;
    int64_t N_channels = 0;

    // Spatial parameters (shared across all voxels, matching planar_image conventions).
    double pxl_dx = 1.0;
    double pxl_dy = 1.0;
    double pxl_dz = 1.0;
    vec3<double> anchor  = vec3<double>(0.0, 0.0, 0.0);
    vec3<double> offset  = vec3<double>(0.0, 0.0, 0.0);
    vec3<double> row_unit = vec3<double>(1.0, 0.0, 0.0);
    vec3<double> col_unit = vec3<double>(0.0, 1.0, 0.0);

    // Per-slice offsets (needed because planar_image stores offset per image).
    std::vector<vec3<double>> slice_offsets;

    // Contiguous data storage: [slice][row][col][channel].
    std::vector<T> data;

    // ----- Constructors -----

    buffer3() = default;

    buffer3(int64_t slices, int64_t rows, int64_t cols, int64_t channels)
        : N_slices(slices), N_rows(rows), N_cols(cols), N_channels(channels) {
        data.resize(static_cast<size_t>(N_slices) * N_rows * N_cols * N_channels, T(0));
        slice_offsets.resize(static_cast<size_t>(N_slices));
        for(int64_t s = 0; s < N_slices; ++s){
            slice_offsets[s] = offset + ortho_unit() * (static_cast<double>(s) * pxl_dz);
        }
    }

    // ----- Indexing -----

    // Linear index from (slice, row, col, channel).
    int64_t index(int64_t slice, int64_t row, int64_t col, int64_t chnl = 0) const {
        return ((slice * N_rows + row) * N_cols + col) * N_channels + chnl;
    }

    // ----- Element access -----

    T value(int64_t slice, int64_t row, int64_t col, int64_t chnl = 0) const {
        return data[static_cast<size_t>(index(slice, row, col, chnl))];
    }

    T& reference(int64_t slice, int64_t row, int64_t col, int64_t chnl = 0) {
        return data[static_cast<size_t>(index(slice, row, col, chnl))];
    }

    // ----- Spatial functions -----

    vec3<double> ortho_unit() const {
        return row_unit.Cross(col_unit).unit();
    }

    // Compute 3D position of voxel centre at (slice, row, col).
    vec3<double> position(int64_t slice, int64_t row, int64_t col) const {
        return anchor
             + slice_offsets[static_cast<size_t>(slice)]
             + row_unit * (pxl_dx * (static_cast<double>(col) + 0.5))
             + col_unit * (pxl_dy * (static_cast<double>(row) + 0.5));
    }

    // ----- Neighbour access (O(1)) -----
    // These return references if in-bounds, or a provided default. Using index arithmetic.

    bool in_bounds(int64_t slice, int64_t row, int64_t col) const {
        return slice >= 0 && slice < N_slices
            && row   >= 0 && row   < N_rows
            && col   >= 0 && col   < N_cols;
    }

    // ----- Visitor: visit all voxels -----

    void visit_all(const std::function<void(int64_t slice, int64_t row, int64_t col)> &f) {
        for(int64_t s = 0; s < N_slices; ++s){
            for(int64_t r = 0; r < N_rows; ++r){
                for(int64_t c = 0; c < N_cols; ++c){
                    f(s, r, c);
                }
            }
        }
    }

    void visit_all(const std::function<void(int64_t slice, int64_t row, int64_t col)> &f) const {
        for(int64_t s = 0; s < N_slices; ++s){
            for(int64_t r = 0; r < N_rows; ++r){
                for(int64_t c = 0; c < N_cols; ++c){
                    f(s, r, c);
                }
            }
        }
    }

    // ----- Visitor: visit a single XY slice -----

    void visit_slice_xy(int64_t slice, const std::function<void(int64_t row, int64_t col)> &f) {
        for(int64_t r = 0; r < N_rows; ++r){
            for(int64_t c = 0; c < N_cols; ++c){
                f(r, c);
            }
        }
    }

    // ----- Parallel visit using work_queue -----

    // Visit all slices in parallel, one task per slice.
    void parallel_visit_slices(work_queue<std::function<void()>> &wq,
                               const std::function<void(int64_t slice)> &f) {
        const int64_t n = N_slices;
        std::atomic<int64_t> done{0};
        std::mutex mtx;
        std::condition_variable cv;

        for(int64_t s = 0; s < n; ++s){
            wq.submit_task([&f, s, &done, &cv](){
                f(s);
                ++done;
                cv.notify_all();
            });
        }
        // Wait for all slices to complete.
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&](){ return done.load() >= n; });
    }

    // Even/odd two-step parallel processing: first process even-numbered slices,
    // then odd-numbered slices. This ensures no two adjacent slices are processed
    // simultaneously, avoiding data races when a kernel reads from neighbour slices.
    void parallel_even_odd_slices(work_queue<std::function<void()>> &wq,
                                  const std::function<void(int64_t slice)> &f) {
        // Process even slices.
        {
            int64_t count = 0;
            for(int64_t s = 0; s < N_slices; s += 2) ++count;
            std::atomic<int64_t> done{0};
            std::mutex mtx;
            std::condition_variable cv;
            for(int64_t s = 0; s < N_slices; s += 2){
                wq.submit_task([&f, s, &done, &cv](){
                    f(s);
                    ++done;
                    cv.notify_all();
                });
            }
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&](){ return done.load() >= count; });
        }
        // Process odd slices.
        {
            int64_t count = 0;
            for(int64_t s = 1; s < N_slices; s += 2) ++count;
            if(count > 0){
                std::atomic<int64_t> done{0};
                std::mutex mtx;
                std::condition_variable cv;
                for(int64_t s = 1; s < N_slices; s += 2){
                    wq.submit_task([&f, s, &done, &cv](){
                        f(s);
                        ++done;
                        cv.notify_all();
                    });
                }
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [&](){ return done.load() >= count; });
            }
        }
    }

    // ----- Separable Gaussian smoothing -----
    //
    // Performs in-place 3D Gaussian smoothing using separable 1D kernels along X, Y, Z axes.
    // sigma_x, sigma_y, sigma_z are in physical units (mm).
    // Uses a temporary buffer and processes slices in parallel.

    void gaussian_smooth(double sigma_x_mm, double sigma_y_mm, double sigma_z_mm,
                         work_queue<std::function<void()>> &wq) {

        if(sigma_x_mm <= 0.0 && sigma_y_mm <= 0.0 && sigma_z_mm <= 0.0) return;

        auto make_kernel = [](int64_t radius, double sigma_pixels) -> std::vector<double> {
            if(sigma_pixels <= 0.0 || radius <= 0) return {1.0};
            std::vector<double> k(2 * radius + 1);
            double sum = 0.0;
            for(int64_t i = -radius; i <= radius; ++i){
                const double v = std::exp(-0.5 * (i * i) / (sigma_pixels * sigma_pixels));
                k[static_cast<size_t>(i + radius)] = v;
                sum += v;
            }
            for(auto &v : k) v /= sum;
            return k;
        };

        const double sx = (sigma_x_mm > 0.0) ? sigma_x_mm / pxl_dx : 0.0;
        const double sy = (sigma_y_mm > 0.0) ? sigma_y_mm / pxl_dy : 0.0;
        const double sz = (sigma_z_mm > 0.0) ? sigma_z_mm / pxl_dz : 0.0;

        const int64_t rx = (sx > 0.0) ? std::max<int64_t>(1, static_cast<int64_t>(3.0 * sx)) : 0;
        const int64_t ry = (sy > 0.0) ? std::max<int64_t>(1, static_cast<int64_t>(3.0 * sy)) : 0;
        const int64_t rz = (sz > 0.0) ? std::max<int64_t>(1, static_cast<int64_t>(3.0 * sz)) : 0;

        const auto kx = make_kernel(rx, sx);
        const auto ky = make_kernel(ry, sy);
        const auto kz = make_kernel(rz, sz);

        // Temporary buffer (same size).
        std::vector<T> temp(data.size());

        // ----- Pass 1: smooth along X (columns) -----
        // Each slice is independent, so we parallelize over slices.
        if(rx > 0){
            parallel_visit_slices(wq, [&](int64_t s){
                for(int64_t r = 0; r < N_rows; ++r){
                    for(int64_t c = 0; c < N_cols; ++c){
                        for(int64_t ch = 0; ch < N_channels; ++ch){
                            double sum = 0.0;
                            double wsum = 0.0;
                            for(int64_t k = -rx; k <= rx; ++k){
                                const int64_t cc = c + k;
                                if(cc >= 0 && cc < N_cols){
                                    const T val = data[static_cast<size_t>(index(s, r, cc, ch))];
                                    if(std::isfinite(static_cast<double>(val))){
                                        const double w = kx[static_cast<size_t>(k + rx)];
                                        sum += w * static_cast<double>(val);
                                        wsum += w;
                                    }
                                }
                            }
                            const auto idx = static_cast<size_t>(index(s, r, c, ch));
                            temp[idx] = (wsum > 0.0) ? static_cast<T>(sum / wsum) : data[idx];
                        }
                    }
                }
            });
        }else{
            temp = data;
        }

        // ----- Pass 2: smooth along Y (rows) -----
        if(ry > 0){
            // Read from temp, write to data.
            parallel_visit_slices(wq, [&](int64_t s){
                for(int64_t r = 0; r < N_rows; ++r){
                    for(int64_t c = 0; c < N_cols; ++c){
                        for(int64_t ch = 0; ch < N_channels; ++ch){
                            double sum = 0.0;
                            double wsum = 0.0;
                            for(int64_t k = -ry; k <= ry; ++k){
                                const int64_t rr = r + k;
                                if(rr >= 0 && rr < N_rows){
                                    const T val = temp[static_cast<size_t>(index(s, rr, c, ch))];
                                    if(std::isfinite(static_cast<double>(val))){
                                        const double w = ky[static_cast<size_t>(k + ry)];
                                        sum += w * static_cast<double>(val);
                                        wsum += w;
                                    }
                                }
                            }
                            const auto idx = static_cast<size_t>(index(s, r, c, ch));
                            data[idx] = (wsum > 0.0) ? static_cast<T>(sum / wsum) : temp[idx];
                        }
                    }
                }
            });
        }else{
            data = temp;
        }

        // ----- Pass 3: smooth along Z (slices) -----
        // Reads from data (result of Y pass), writes to temp.
        // Then swap back.
        if(rz > 0){
            // Use even/odd pattern because each slice reads from its Z neighbours.
            // However, we read from 'data' and write to 'temp', so there is no conflict.
            parallel_visit_slices(wq, [&](int64_t s){
                for(int64_t r = 0; r < N_rows; ++r){
                    for(int64_t c = 0; c < N_cols; ++c){
                        for(int64_t ch = 0; ch < N_channels; ++ch){
                            double sum = 0.0;
                            double wsum = 0.0;
                            for(int64_t k = -rz; k <= rz; ++k){
                                const int64_t ss = s + k;
                                if(ss >= 0 && ss < N_slices){
                                    const T val = data[static_cast<size_t>(index(ss, r, c, ch))];
                                    if(std::isfinite(static_cast<double>(val))){
                                        const double w = kz[static_cast<size_t>(k + rz)];
                                        sum += w * static_cast<double>(val);
                                        wsum += w;
                                    }
                                }
                            }
                            const auto idx = static_cast<size_t>(index(s, r, c, ch));
                            temp[idx] = (wsum > 0.0) ? static_cast<T>(sum / wsum) : data[idx];
                        }
                    }
                }
            });
            data.swap(temp);
        }
    }

    // Single-threaded Gaussian smoothing (for small buffers or when no work_queue is available).
    void gaussian_smooth(double sigma_x_mm, double sigma_y_mm, double sigma_z_mm) {
        work_queue<std::function<void()>> wq(1);
        gaussian_smooth(sigma_x_mm, sigma_y_mm, sigma_z_mm, wq);
    }

    // Isotropic smoothing convenience.
    void gaussian_smooth(double sigma_mm) {
        gaussian_smooth(sigma_mm, sigma_mm, sigma_mm);
    }

    void gaussian_smooth(double sigma_mm, work_queue<std::function<void()>> &wq) {
        gaussian_smooth(sigma_mm, sigma_mm, sigma_mm, wq);
    }

    // ----- Convolution with user-provided kernel -----
    //
    // Applies a 3D separable convolution with separate 1D kernels for X, Y, Z.
    // Kernels are provided as vectors (assumed to be centred at the middle element).

    void convolve_separable(const std::vector<double> &kernel_x,
                            const std::vector<double> &kernel_y,
                            const std::vector<double> &kernel_z,
                            work_queue<std::function<void()>> &wq) {
        std::vector<T> temp(data.size());

        const int64_t rx = static_cast<int64_t>(kernel_x.size()) / 2;
        const int64_t ry = static_cast<int64_t>(kernel_y.size()) / 2;
        const int64_t rz = static_cast<int64_t>(kernel_z.size()) / 2;

        // Pass 1: X.
        if(!kernel_x.empty()){
            parallel_visit_slices(wq, [&](int64_t s){
                for(int64_t r = 0; r < N_rows; ++r){
                    for(int64_t c = 0; c < N_cols; ++c){
                        for(int64_t ch = 0; ch < N_channels; ++ch){
                            double sum = 0.0;
                            double wsum = 0.0;
                            for(int64_t k = -rx; k <= rx; ++k){
                                const int64_t cc = c + k;
                                if(cc >= 0 && cc < N_cols){
                                    const T val = data[static_cast<size_t>(index(s, r, cc, ch))];
                                    if(std::isfinite(static_cast<double>(val))){
                                        const double w = kernel_x[static_cast<size_t>(k + rx)];
                                        sum += w * static_cast<double>(val);
                                        wsum += w;
                                    }
                                }
                            }
                            const auto idx = static_cast<size_t>(index(s, r, c, ch));
                            temp[idx] = (wsum > 0.0) ? static_cast<T>(sum / wsum) : data[idx];
                        }
                    }
                }
            });
        }else{
            temp = data;
        }

        // Pass 2: Y.
        if(!kernel_y.empty()){
            parallel_visit_slices(wq, [&](int64_t s){
                for(int64_t r = 0; r < N_rows; ++r){
                    for(int64_t c = 0; c < N_cols; ++c){
                        for(int64_t ch = 0; ch < N_channels; ++ch){
                            double sum = 0.0;
                            double wsum = 0.0;
                            for(int64_t k = -ry; k <= ry; ++k){
                                const int64_t rr = r + k;
                                if(rr >= 0 && rr < N_rows){
                                    const T val = temp[static_cast<size_t>(index(s, rr, c, ch))];
                                    if(std::isfinite(static_cast<double>(val))){
                                        const double w = kernel_y[static_cast<size_t>(k + ry)];
                                        sum += w * static_cast<double>(val);
                                        wsum += w;
                                    }
                                }
                            }
                            const auto idx = static_cast<size_t>(index(s, r, c, ch));
                            data[idx] = (wsum > 0.0) ? static_cast<T>(sum / wsum) : temp[idx];
                        }
                    }
                }
            });
        }else{
            data = temp;
        }

        // Pass 3: Z.
        if(!kernel_z.empty()){
            parallel_visit_slices(wq, [&](int64_t s){
                for(int64_t r = 0; r < N_rows; ++r){
                    for(int64_t c = 0; c < N_cols; ++c){
                        for(int64_t ch = 0; ch < N_channels; ++ch){
                            double sum = 0.0;
                            double wsum = 0.0;
                            for(int64_t k = -rz; k <= rz; ++k){
                                const int64_t ss = s + k;
                                if(ss >= 0 && ss < N_slices){
                                    const T val = data[static_cast<size_t>(index(ss, r, c, ch))];
                                    if(std::isfinite(static_cast<double>(val))){
                                        const double w = kernel_z[static_cast<size_t>(k + rz)];
                                        sum += w * static_cast<double>(val);
                                        wsum += w;
                                    }
                                }
                            }
                            const auto idx = static_cast<size_t>(index(s, r, c, ch));
                            temp[idx] = (wsum > 0.0) ? static_cast<T>(sum / wsum) : data[idx];
                        }
                    }
                }
            });
            data.swap(temp);
        }
    }

    // ----- Marshalling: from planar_image_collection -----
    //
    // Loads data from a planar_image_collection. Images are sorted spatially along
    // the ortho direction to ensure correct slice ordering (images in a
    // planar_image_collection are NOT guaranteed to be spatially ordered).

    static buffer3 from_planar_image_collection(const planar_image_collection<T, double> &coll) {
        if(coll.images.empty()){
            throw std::invalid_argument("Cannot create buffer3 from empty image collection");
        }

        // Build a vector of pointers with spatial ordering.
        const auto &first = coll.images.front();
        const auto ort = first.row_unit.Cross(first.col_unit).unit();

        struct slice_ref {
            const planar_image<T, double> *img;
            double ortho_pos;
        };
        std::vector<slice_ref> sorted_refs;
        sorted_refs.reserve(coll.images.size());
        for(const auto &img : coll.images){
            const double pos = img.center().Dot(ort);
            sorted_refs.push_back({&img, pos});
        }
        std::sort(sorted_refs.begin(), sorted_refs.end(),
                  [](const slice_ref &a, const slice_ref &b){ return a.ortho_pos < b.ortho_pos; });

        const int64_t N_slices  = static_cast<int64_t>(sorted_refs.size());
        const int64_t N_rows    = first.rows;
        const int64_t N_cols    = first.columns;
        const int64_t N_channels = first.channels;

        buffer3 buf;
        buf.N_slices   = N_slices;
        buf.N_rows     = N_rows;
        buf.N_cols     = N_cols;
        buf.N_channels = N_channels;
        buf.pxl_dx     = first.pxl_dx;
        buf.pxl_dy     = first.pxl_dy;
        buf.pxl_dz     = first.pxl_dz;
        buf.anchor     = first.anchor;
        buf.offset     = sorted_refs[0].img->offset;
        buf.row_unit   = first.row_unit;
        buf.col_unit   = first.col_unit;

        buf.slice_offsets.resize(static_cast<size_t>(N_slices));
        buf.data.resize(static_cast<size_t>(N_slices) * N_rows * N_cols * N_channels);

        for(int64_t s = 0; s < N_slices; ++s){
            const auto *img = sorted_refs[static_cast<size_t>(s)].img;
            buf.slice_offsets[static_cast<size_t>(s)] = img->offset;

            // Copy data row-by-row.
            for(int64_t r = 0; r < N_rows; ++r){
                for(int64_t c = 0; c < N_cols; ++c){
                    for(int64_t ch = 0; ch < N_channels; ++ch){
                        buf.reference(s, r, c, ch) = img->value(r, c, ch);
                    }
                }
            }
        }

        return buf;
    }

    // ----- Marshalling: to planar_image_collection -----
    //
    // Writes data back to a planar_image_collection. The output images are in
    // spatially sorted order (increasing ortho position). Metadata is not preserved
    // (caller should copy it if needed).

    planar_image_collection<T, double> to_planar_image_collection() const {
        planar_image_collection<T, double> coll;

        for(int64_t s = 0; s < N_slices; ++s){
            planar_image<T, double> img;
            img.init_orientation(row_unit, col_unit);
            img.init_buffer(N_rows, N_cols, N_channels);
            img.init_spatial(pxl_dx, pxl_dy, pxl_dz, anchor, slice_offsets[static_cast<size_t>(s)]);

            for(int64_t r = 0; r < N_rows; ++r){
                for(int64_t c = 0; c < N_cols; ++c){
                    for(int64_t ch = 0; ch < N_channels; ++ch){
                        img.reference(r, c, ch) = value(s, r, c, ch);
                    }
                }
            }

            coll.images.push_back(std::move(img));
        }

        return coll;
    }

    // ----- Marshalling: write back into existing planar_image_collection -----
    //
    // Writes data back into an existing collection, matching images by spatial position
    // to handle potentially unordered collections. This preserves metadata.

    void write_to_planar_image_collection(planar_image_collection<T, double> &coll) const {
        if(static_cast<int64_t>(coll.images.size()) != N_slices){
            throw std::invalid_argument("Image count mismatch when writing buffer3 back to collection");
        }

        const auto ort = row_unit.Cross(col_unit).unit();

        // Build a mapping from the collection's images to our sorted slice indices.
        struct slice_map {
            planar_image<T, double> *img;
            double ortho_pos;
        };
        std::vector<slice_map> coll_sorted;
        coll_sorted.reserve(coll.images.size());
        for(auto &img : coll.images){
            const double pos = img.center().Dot(ort);
            coll_sorted.push_back({&img, pos});
        }
        std::sort(coll_sorted.begin(), coll_sorted.end(),
                  [](const slice_map &a, const slice_map &b){ return a.ortho_pos < b.ortho_pos; });

        for(int64_t s = 0; s < N_slices; ++s){
            auto *img = coll_sorted[static_cast<size_t>(s)].img;
            for(int64_t r = 0; r < N_rows; ++r){
                for(int64_t c = 0; c < N_cols; ++c){
                    for(int64_t ch = 0; ch < N_channels; ++ch){
                        img->reference(r, c, ch) = value(s, r, c, ch);
                    }
                }
            }
        }
    }

    // ----- Trilinear interpolation -----
    //
    // Interpolate at an arbitrary 3D position. Returns out_of_bounds if the position
    // is outside the buffer.

    T trilinear_interpolate(const vec3<double> &pos, int64_t chnl, T out_of_bounds) const {
        if(N_slices == 0 || N_rows == 0 || N_cols == 0) return out_of_bounds;

        const auto ort = ortho_unit();
        const auto diff = pos - anchor - slice_offsets[0];

        // Compute fractional coordinates.
        const double fc = diff.Dot(row_unit) / pxl_dx - 0.5;
        const double fr = diff.Dot(col_unit) / pxl_dy - 0.5;
        const double fs = diff.Dot(ort) / pxl_dz;

        // But we need to account for non-zero slice_offsets[0] in z direction.
        // Actually, slice_offsets already encode the offset, so we should compute
        // the z-position relative to the first slice.
        // For a regular grid, slices are at z = slice_offsets[s].Dot(ort) + anchor.Dot(ort).
        // The fractional slice index is:
        //   fs = (pos.Dot(ort) - first_slice_centre.Dot(ort)) / pxl_dz
        const double pos_z = pos.Dot(ort);
        const vec3<double> first_centre = anchor + slice_offsets[0]
                                        + row_unit * (pxl_dx * 0.5)
                                        + col_unit * (pxl_dy * 0.5);
        const double first_z = first_centre.Dot(ort);
        const double frac_s = (pos_z - first_z) / pxl_dz;

        // For X and Y, compute relative to the first voxel centre.
        const double frac_c = diff.Dot(row_unit) / pxl_dx - 0.5;
        const double frac_r = diff.Dot(col_unit) / pxl_dy - 0.5;

        // Clamp to valid range for interpolation.
        if(frac_c < -0.5 || frac_c > N_cols - 0.5) return out_of_bounds;
        if(frac_r < -0.5 || frac_r > N_rows - 0.5) return out_of_bounds;
        if(N_slices > 1 && (frac_s < -0.5 || frac_s > N_slices - 0.5)) return out_of_bounds;

        // For single-slice case, just do bilinear interpolation.
        const int64_t c0 = std::max<int64_t>(0, static_cast<int64_t>(std::floor(frac_c)));
        const int64_t r0 = std::max<int64_t>(0, static_cast<int64_t>(std::floor(frac_r)));
        const int64_t c1 = std::min<int64_t>(N_cols - 1, c0 + 1);
        const int64_t r1 = std::min<int64_t>(N_rows - 1, r0 + 1);
        const double tc = frac_c - std::floor(frac_c);
        const double tr = frac_r - std::floor(frac_r);

        if(N_slices == 1){
            const double v00 = static_cast<double>(value(0, r0, c0, chnl));
            const double v01 = static_cast<double>(value(0, r0, c1, chnl));
            const double v10 = static_cast<double>(value(0, r1, c0, chnl));
            const double v11 = static_cast<double>(value(0, r1, c1, chnl));
            const double v0 = v00 * (1.0 - tc) + v01 * tc;
            const double v1 = v10 * (1.0 - tc) + v11 * tc;
            return static_cast<T>(v0 * (1.0 - tr) + v1 * tr);
        }

        const int64_t s0 = std::max<int64_t>(0, static_cast<int64_t>(std::floor(frac_s)));
        const int64_t s1 = std::min<int64_t>(N_slices - 1, s0 + 1);
        const double ts = frac_s - std::floor(frac_s);

        // Trilinear: interpolate two bilinear results along z.
        auto bilinear = [&](int64_t s) -> double {
            const double v00 = static_cast<double>(value(s, r0, c0, chnl));
            const double v01 = static_cast<double>(value(s, r0, c1, chnl));
            const double v10 = static_cast<double>(value(s, r1, c0, chnl));
            const double v11 = static_cast<double>(value(s, r1, c1, chnl));
            const double v0 = v00 * (1.0 - tc) + v01 * tc;
            const double v1 = v10 * (1.0 - tc) + v11 * tc;
            return v0 * (1.0 - tr) + v1 * tr;
        };

        const double val0 = bilinear(s0);
        const double val1 = bilinear(s1);
        return static_cast<T>(val0 * (1.0 - ts) + val1 * ts);
    }
};
