//Alignment_Demons_SYCL_Tests.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// Unit tests for the SYCL-accelerated demons registration implementation.
// 
// Note: Some tests require the full DICOMautomaton build environment (Alignment_Demons.cc, Structs.cc, etc.)
// and will be conditionally compiled based on whether DCMA_FULL_BUILD is defined.

#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
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

#include "SYCL_Volume.h"
#include "Alignment_Demons.h"
#include "Alignment_Demons_SYCL.h"

#ifdef DCMA_FULL_BUILD
#include "Structs.h"
#include "Alignment_Field.h"
#endif


// Helper function to create a test image collection.
static planar_image_collection<float, double>
make_sycl_test_image_collection(
    int64_t slices,
    int64_t rows,
    int64_t cols,
    const std::function<float(int64_t, int64_t, int64_t)> &value_fn,
    const vec3<double> &anchor = vec3<double>(0.0, 0.0, 0.0),
    const vec3<double> &offset = vec3<double>(0.0, 0.0, 0.0),
    double pxl_dx = 1.0,
    double pxl_dy = 1.0,
    double pxl_dz = 1.0) {

    planar_image_collection<float, double> coll;
    const vec3<double> row_unit(1.0, 0.0, 0.0);
    const vec3<double> col_unit(0.0, 1.0, 0.0);
    const vec3<double> z_unit(0.0, 0.0, 1.0);

    for (int64_t slice = 0; slice < slices; ++slice) {
        planar_image<float, double> img;
        img.init_orientation(row_unit, col_unit);
        img.init_buffer(rows, cols, 1);
        const vec3<double> slice_offset = offset + z_unit * (static_cast<double>(slice) * pxl_dz);
        img.init_spatial(pxl_dx, pxl_dy, pxl_dz, anchor, slice_offset);

        for (int64_t row = 0; row < rows; ++row) {
            for (int64_t col = 0; col < cols; ++col) {
                img.reference(row, col, 0) = value_fn(slice, row, col);
            }
        }

        coll.images.push_back(img);
    }

    return coll;
}


TEST_CASE("SyclVolume construction and marshaling") {
    SUBCASE("Single channel image") {
        auto img_coll = make_sycl_test_image_collection(3, 4, 5,
            [](int64_t z, int64_t y, int64_t x) {
                return static_cast<float>(z * 100 + y * 10 + x);
            });

        SyclVolume<float> vol(img_coll, 0);

        CHECK(vol.meta.dim_x == 5);
        CHECK(vol.meta.dim_y == 4);
        CHECK(vol.meta.dim_z == 3);
        CHECK(vol.meta.channels == 1);
        CHECK(vol.data.size() == 60);

        // Check a few values.
        CHECK(vol.data[vol.meta.linear_index(0, 0, 0, 0)] == doctest::Approx(0.0f));
        CHECK(vol.data[vol.meta.linear_index(4, 3, 2, 0)] == doctest::Approx(234.0f));
    }

    SUBCASE("Round-trip marshaling") {
        auto original = make_sycl_test_image_collection(2, 3, 4,
            [](int64_t z, int64_t y, int64_t x) {
                return static_cast<float>(z * 12 + y * 4 + x);
            });

        SyclVolume<float> vol(original, 0);
        auto restored = vol.to_planar_image_collection();

        CHECK(restored.images.size() == original.images.size());

        auto orig_it = original.images.begin();
        auto rest_it = restored.images.begin();
        for (; orig_it != original.images.end(); ++orig_it, ++rest_it) {
            for (int64_t row = 0; row < orig_it->rows; ++row) {
                for (int64_t col = 0; col < orig_it->columns; ++col) {
                    CHECK(rest_it->value(row, col, 0) == doctest::Approx(orig_it->value(row, col, 0)));
                }
            }
        }
    }
}


TEST_CASE("SyclVolume trilinear interpolation") {
    auto img_coll = make_sycl_test_image_collection(2, 2, 2,
        [](int64_t z, int64_t y, int64_t x) {
            return static_cast<float>(z * 4 + y * 2 + x);
        });

    SyclVolume<float> vol(img_coll, 0);

    SUBCASE("Exact voxel positions") {
        // Position at center of voxel (0,0,0).
        auto pos = vol.meta.voxel_to_world(0, 0, 0);
        SyclVec3 spos(pos.x, pos.y, pos.z);
        CHECK(vol.trilinear_interp(spos, 0, -1.0f) == doctest::Approx(0.0f));
    }

    SUBCASE("Interpolated positions") {
        // Midpoint between voxels.
        SyclVec3 mid(0.5, 0.0, 0.0);
        float val = vol.trilinear_interp(mid, 0, -1.0f);
        CHECK(val == doctest::Approx(0.5f));
    }

    SUBCASE("Out of bounds returns oob_val") {
        SyclVec3 oob(-10.0, 0.0, 0.0);
        CHECK(vol.trilinear_interp(oob, 0, -999.0f) == doctest::Approx(-999.0f));
    }
}


TEST_CASE("SYCL compute_gradient_sycl") {
    auto img = make_sycl_test_image_collection(1, 3, 3,
        [](int64_t, int64_t row, int64_t col) {
            return static_cast<float>(2.0 * row + col);
        });

    SyclVolume<float> vol(img, 0);
    auto gradient = AlignViaDemonsSYCL::compute_gradient_sycl(vol);

    CHECK(gradient.meta.channels == 3);
    CHECK(gradient.meta.dim_x == 3);
    CHECK(gradient.meta.dim_y == 3);
    CHECK(gradient.meta.dim_z == 1);

    // Check gradient at center voxel (1,1,0).
    const int64_t cx = 1, cy = 1, cz = 0;
    const double grad_x = gradient.data[gradient.meta.linear_index(cx, cy, cz, 0)];
    const double grad_y = gradient.data[gradient.meta.linear_index(cx, cy, cz, 1)];
    const double grad_z = gradient.data[gradient.meta.linear_index(cx, cy, cz, 2)];

    CHECK(grad_x == doctest::Approx(1.0));
    CHECK(grad_y == doctest::Approx(2.0));
    CHECK(grad_z == doctest::Approx(0.0));
}


TEST_CASE("SYCL smooth_vector_field_sycl") {
    // Create a vector field with a single spike.
    planar_image_collection<double, double> field_coll;
    for (int64_t z = 0; z < 1; ++z) {
        planar_image<double, double> img;
        img.init_orientation(vec3<double>(1, 0, 0), vec3<double>(0, 1, 0));
        img.init_buffer(3, 3, 3);
        img.init_spatial(1.0, 1.0, 1.0, vec3<double>(0, 0, 0), vec3<double>(0, 0, z));

        for (auto &v : img.data) v = 0.0;

        // Set center voxel to have a displacement.
        img.reference(1, 1, 0) = 3.0;
        img.reference(1, 1, 1) = 0.0;
        img.reference(1, 1, 2) = 0.0;

        field_coll.images.push_back(img);
    }

    SyclVolume<double> field = SyclVolume<double>::from_vector_field(field_coll);

    // Verify initial spike.
    CHECK(field.data[field.meta.linear_index(1, 1, 0, 0)] == doctest::Approx(3.0));

    // Smooth with sigma=1.0.
    AlignViaDemonsSYCL::smooth_vector_field_sycl(field, 1.0);

    // After smoothing, center should be reduced and neighbors should have some value.
    CHECK(field.data[field.meta.linear_index(1, 1, 0, 0)] < 3.0);
    CHECK(field.data[field.meta.linear_index(1, 1, 0, 0)] > 0.0);
    CHECK(field.data[field.meta.linear_index(0, 1, 0, 0)] > 0.0);
}


TEST_CASE("SYCL warp_image_sycl identity") {
    auto img = make_sycl_test_image_collection(1, 3, 3,
        [](int64_t, int64_t row, int64_t col) {
            return static_cast<float>(row * 10 + col);
        });

    SyclVolume<float> vol(img, 0);

    // Create zero deformation field.
    SyclVolume<double> def;
    def.meta = vol.meta;
    def.meta.channels = 3;
    def.data.resize(def.meta.total_elements(), 0.0);

    auto warped = AlignViaDemonsSYCL::warp_image_sycl(vol, def);

    // Should be identical to original.
    for (size_t i = 0; i < vol.data.size(); ++i) {
        CHECK(warped.data[i] == doctest::Approx(vol.data[i]));
    }
}


// The following tests require the full DICOMautomaton build environment
// (Alignment_Demons.cc, Alignment_Field.cc, Structs.cc, etc.)
#ifdef DCMA_FULL_BUILD

TEST_CASE("SYCL AlignViaDemons_SYCL identical images") {
    auto img = make_sycl_test_image_collection(1, 5, 5,
        [](int64_t, int64_t row, int64_t col) {
            return static_cast<float>(row + col);
        });

    AlignViaDemonsParams params;
    params.max_iterations = 3;
    params.convergence_threshold = 0.0;
    params.deformation_field_smoothing_sigma = 0.0;
    params.update_field_smoothing_sigma = 0.0;
    params.verbosity = 0;

    auto result = AlignViaDemonsSYCL::AlignViaDemons_SYCL(params, img, img);
    REQUIRE(result.has_value());

    // Deformation should be essentially zero.
    const auto &field_images = result->get_imagecoll_crefw().get().images;
    double max_abs = 0.0;
    for (const auto &img : field_images) {
        for (const auto &val : img.data) {
            max_abs = std::max(max_abs, std::abs(val));
        }
    }
    CHECK(max_abs < 1e-6);
}


TEST_CASE("SYCL AlignViaDemons_SYCL improves MSE") {
    const int64_t N = 10;

    auto stationary = make_sycl_test_image_collection(1, N, N,
        [](int64_t, int64_t row, int64_t col) {
            const double dr = row - 5.0;
            const double dc = col - 5.0;
            return static_cast<float>(100.0 * std::exp(-(dr * dr + dc * dc) / 4.0));
        });

    auto moving = make_sycl_test_image_collection(1, N, N,
        [](int64_t, int64_t row, int64_t col) {
            const double dr = row - 5.0;
            const double dc = col - 5.0 - 1.0;  // Shifted by 1 pixel.
            return static_cast<float>(100.0 * std::exp(-(dr * dr + dc * dc) / 4.0));
        });

    // Compute MSE before registration.
    double mse_before = 0.0;
    int64_t count = 0;
    auto stat_it = stationary.images.begin();
    auto mov_it = moving.images.begin();
    for (; stat_it != stationary.images.end(); ++stat_it, ++mov_it) {
        for (size_t i = 0; i < stat_it->data.size(); ++i) {
            const double diff = stat_it->data[i] - mov_it->data[i];
            mse_before += diff * diff;
            ++count;
        }
    }
    mse_before /= static_cast<double>(count);

    AlignViaDemonsParams params;
    params.max_iterations = 100;
    params.convergence_threshold = 0.0;
    params.deformation_field_smoothing_sigma = 1.0;
    params.update_field_smoothing_sigma = 0.0;
    params.use_diffeomorphic = false;
    params.max_update_magnitude = 2.0;
    params.verbosity = 0;

    auto result = AlignViaDemonsSYCL::AlignViaDemons_SYCL(params, moving, stationary);
    REQUIRE(result.has_value());

    // Warp moving image with the result.
    auto warped = AlignViaDemonsHelpers::warp_image_with_field(moving, *result);

    // Compute MSE after registration.
    double mse_after = 0.0;
    count = 0;
    stat_it = stationary.images.begin();
    auto warp_it = warped.images.begin();
    for (; stat_it != stationary.images.end(); ++stat_it, ++warp_it) {
        for (int64_t row = 0; row < stat_it->rows; ++row) {
            for (int64_t col = 0; col < stat_it->columns; ++col) {
                const float sv = stat_it->value(row, col, 0);
                const float wv = warp_it->value(row, col, 0);
                if (std::isfinite(sv) && std::isfinite(wv)) {
                    const double diff = sv - wv;
                    mse_after += diff * diff;
                    ++count;
                }
            }
        }
    }
    mse_after /= static_cast<double>(count);

    CHECK(mse_after < mse_before);
}


TEST_CASE("SYCL AlignViaDemons_SYCL handles empty inputs") {
    AlignViaDemonsParams params;
    params.verbosity = 0;

    planar_image_collection<float, double> empty;
    auto stationary = make_sycl_test_image_collection(1, 2, 2,
        [](int64_t, int64_t, int64_t) { return 1.0f; });

    auto result = AlignViaDemonsSYCL::AlignViaDemons_SYCL(params, empty, stationary);
    CHECK_FALSE(result.has_value());
}

#endif // DCMA_FULL_BUILD

