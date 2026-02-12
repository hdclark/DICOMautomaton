//Alignment_Demons_Tests.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains unit tests for the demons deformable image registration methods defined in Alignment_Demons.cc.
// These tests are separated into their own file because Alignment_Demons_obj is linked into
// shared libraries which don't include doctest implementation.

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
#include "Thread_Pool.h"

#include "Alignment_Rigid.h"
#include "Alignment_Field.h"
#include "Alignment_Demons.h"
#include "Alignment_Buffer3.h"


using namespace AlignViaDemonsHelpers;


static planar_image_collection<float, double>
make_test_image_collection(
    int64_t slices,
    int64_t rows,
    int64_t cols,
    const std::function<float(int64_t, int64_t, int64_t)> &value_fn,
    const vec3<double> &anchor = vec3<double>(0.0, 0.0, 0.0),
    const vec3<double> &offset = vec3<double>(0.0, 0.0, 0.0),
    double pxl_dx = 1.0,
    double pxl_dy = 1.0,
    double pxl_dz = 1.0){

    planar_image_collection<float, double> coll;
    const vec3<double> row_unit(1.0, 0.0, 0.0);
    const vec3<double> col_unit(0.0, 1.0, 0.0);
    const vec3<double> z_unit(0.0, 0.0, 1.0);

    for(int64_t slice = 0; slice < slices; ++slice){
        planar_image<float, double> img;
        img.init_orientation(row_unit, col_unit);
        img.init_buffer(rows, cols, 1);
        const vec3<double> slice_offset = offset + z_unit * (static_cast<double>(slice) * pxl_dz);
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
    const vec3<double> &anchor = vec3<double>(0.0, 0.0, 0.0),
    const vec3<double> &offset = vec3<double>(0.0, 0.0, 0.0),
    double pxl_dx = 1.0,
    double pxl_dy = 1.0,
    double pxl_dz = 1.0){

    planar_image_collection<double, double> coll;
    const vec3<double> row_unit(1.0, 0.0, 0.0);
    const vec3<double> col_unit(0.0, 1.0, 0.0);
    const vec3<double> z_unit(0.0, 0.0, 1.0);

    for(int64_t slice = 0; slice < slices; ++slice){
        planar_image<double, double> img;
        img.init_orientation(row_unit, col_unit);
        img.init_buffer(rows, cols, 3);
        const vec3<double> slice_offset = offset + z_unit * (static_cast<double>(slice) * pxl_dz);
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

    auto uniform_source = make_test_image_collection(1, 2, 2,
        [](int64_t, int64_t, int64_t){ return 5.0f; });
    auto uniform_reference = make_test_image_collection(1, 2, 2,
        [](int64_t, int64_t, int64_t){ return 10.0f; });
    auto constant_matched = histogram_match(uniform_source, uniform_reference, 8, 0.0);
    const auto &constant_img = get_image(constant_matched.images, 0);
    CHECK(constant_img.value(0, 0, 0) == doctest::Approx(5.0));
    CHECK(constant_img.value(1, 1, 0) == doctest::Approx(5.0));

    auto matched_constant_reference = histogram_match(source, uniform_reference, 8, 0.0);
    const auto &matched_constant_ref_img = get_image(matched_constant_reference.images, 0);
    const auto &source_img = get_image(source.images, 0);
    CHECK(matched_constant_ref_img.value(0, 0, 0) == doctest::Approx(source_img.value(0, 0, 0)));
    CHECK(matched_constant_ref_img.value(1, 1, 0) == doctest::Approx(source_img.value(1, 1, 0)));

    auto collect_values = [](const planar_image_collection<float, double> &coll){
        std::vector<double> values;
        for(const auto &img : coll.images){
            for(const auto &val : img.data){
                if(std::isfinite(val)){
                    values.push_back(val);
                }
            }
        }
        std::sort(values.begin(), values.end());
        return values;
    };

    auto median = [](const std::vector<double> &values){
        const size_t mid = values.size() / 2;
        if(values.size() % 2 == 0){
            return 0.5 * (values[mid - 1] + values[mid]);
        }
        return values[mid];
    };

    const auto source_vals = collect_values(source);
    const auto reference_vals = collect_values(reference);
    const auto matched_vals = collect_values(matched);
    const double source_median = median(source_vals);
    const double reference_median = median(reference_vals);
    const double matched_median = median(matched_vals);
    CHECK(std::abs(matched_median - reference_median) < std::abs(source_median - reference_median));
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

    auto uniform_field = make_test_vector_field(1, 3, 3,
        [](int64_t, int64_t, int64_t){
            return vec3<double>(1.5, -2.0, 0.5);
        });
    smooth_vector_field(uniform_field, 1.0);
    const auto &uniform_img = get_image(uniform_field.images, 0);
    CHECK(uniform_img.value(0, 0, 0) == doctest::Approx(1.5));
    CHECK(uniform_img.value(2, 2, 1) == doctest::Approx(-2.0));
    CHECK(uniform_img.value(1, 1, 2) == doctest::Approx(0.5));

    auto multi_slice = make_test_vector_field(2, 2, 2,
        [](int64_t slice, int64_t, int64_t){
            return vec3<double>(slice == 0 ? 0.0 : 2.0, 0.0, 0.0);
        });
    smooth_vector_field(multi_slice, 1.0);
    const auto &slice0 = get_image(multi_slice.images, 0);
    const auto &slice1 = get_image(multi_slice.images, 1);
    CHECK(slice0.value(0, 0, 0) > 0.0);
    CHECK(slice0.value(0, 0, 0) < 2.0);
    CHECK(slice1.value(0, 0, 0) > 0.0);
    CHECK(slice1.value(0, 0, 0) < 2.0);

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

TEST_CASE( "compute_gradient handles 1x1 single-slice images" ){
    auto img = make_test_image_collection(1, 1, 1,
        [](int64_t, int64_t, int64_t){ return 42.0f; });
    auto gradient = compute_gradient(img);
    const auto &grad_img = get_image(gradient.images, 0);
    CHECK(grad_img.value(0, 0, 0) == doctest::Approx(0.0));
    CHECK(grad_img.value(0, 0, 1) == doctest::Approx(0.0));
    CHECK(grad_img.value(0, 0, 2) == doctest::Approx(0.0));
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
    const int64_t rows = 5;
    const int64_t cols = 5;

    auto stationary = make_test_image_collection(1, rows, cols,
        [](int64_t, int64_t, int64_t col){
            return static_cast<float>(col);
        });

    auto moving = make_test_image_collection(1, rows, cols,
        [cols](int64_t, int64_t, int64_t col){
            const int64_t shifted = std::min<int64_t>(cols - 1, col + 1);
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
    const int64_t max_sample_loss_tolerance = std::max(rows, cols); // Allow up to one edge row/column to drop out after warping.

    SUBCASE("standard demons"){
        auto params = base_params;
        params.use_diffeomorphic = false;
        auto result = AlignViaDemons(params, moving, stationary);
        REQUIRE(result.has_value());
        auto warped = warp_image_with_field(moving, *result);
        const auto [mse_after, count_after] = compute_mse_and_count(stationary, warped);
        CHECK(count_after >= count_before - max_sample_loss_tolerance);
        CHECK(mse_after < mse_before);
    }

    SUBCASE("diffeomorphic demons"){
        auto params = base_params;
        params.use_diffeomorphic = true;
        auto result = AlignViaDemons(params, moving, stationary);
        REQUIRE(result.has_value());
        auto warped = warp_image_with_field(moving, *result);
        const auto [mse_after, count_after] = compute_mse_and_count(stationary, warped);
        CHECK(count_after >= count_before - max_sample_loss_tolerance);
        CHECK(mse_after < mse_before);
    }
}


TEST_CASE( "warp_image_with_field uses bilinear interpolation not nearest-neighbour" ){
    // Verify that warping uses sub-pixel interpolation. With nearest-neighbour
    // a half-pixel displacement would snap back to the original pixel position.
    const int64_t N = 10;
    auto img = make_test_image_collection(1, N, N,
        [](int64_t, int64_t, int64_t col){
            return static_cast<float>(col);
        });

    // Create a uniform +0.5 pixel displacement in x (column direction).
    auto field_images = make_test_vector_field(1, N, N,
        [](int64_t, int64_t, int64_t){
            return vec3<double>(0.5, 0.0, 0.0);
        });
    deformation_field field(std::move(field_images));

    auto warped = warp_image_with_field(img, field);
    const auto &warped_img = get_image(warped.images, 0);

    // With proper bilinear interpolation, warped(row, col) = moving(col + 0.5)
    // which should be (col + 0.5) for interior pixels.
    // With nearest-neighbour, the value would snap to either col or col+1.
    for(int64_t col = 1; col < N - 2; ++col){
        const float expected = static_cast<float>(col) + 0.5f;
        CHECK(warped_img.value(5, col, 0) == doctest::Approx(expected).epsilon(0.01));
    }
}


TEST_CASE( "AlignViaDemons converges for Gaussian blob shift" ){
    // A Gaussian blob shifted by a known amount should converge with decreasing MSE.
    const int64_t N = 20;

    auto stationary = make_test_image_collection(1, N, N,
        [](int64_t, int64_t row, int64_t col){
            const double dr = row - 10.0;
            const double dc = col - 10.0;
            return static_cast<float>(100.0 * std::exp(-(dr * dr + dc * dc) / 8.0));
        });

    auto moving = make_test_image_collection(1, N, N,
        [](int64_t, int64_t row, int64_t col){
            const double dr = row - 10.0;
            const double dc = col - 10.0 - 2.0;
            return static_cast<float>(100.0 * std::exp(-(dr * dr + dc * dc) / 8.0));
        });

    const auto [mse_before, count_before] = compute_mse_and_count(stationary, moving);

    AlignViaDemonsParams params;
    params.max_iterations = 200;
    params.convergence_threshold = 0.0;
    params.deformation_field_smoothing_sigma = 1.0;
    params.update_field_smoothing_sigma = 0.0;
    params.use_diffeomorphic = false;
    params.max_update_magnitude = 2.0;
    params.verbosity = 0;

    auto result = AlignViaDemons(params, moving, stationary);
    REQUIRE(result.has_value());
    auto warped = warp_image_with_field(moving, *result);
    const auto [mse_after, count_after] = compute_mse_and_count(stationary, warped);

    // MSE should drop significantly (more than 10x).
    CHECK(mse_after < mse_before * 0.1);
    // Should not lose too many samples. A 2-pixel shift can push up to 2 edge
    // rows/columns out of bounds, so allow loss of up to 3*N pixels.
    CHECK(count_after >= count_before - 3 * N);
}


TEST_CASE( "AlignViaDemons does not drift for identical images" ){
    // Running the algorithm on identical images should not cause the warped
    // image to drift away from the original. The center-of-mass should stay put.
    const int64_t N = 16;

    auto img = make_test_image_collection(1, N, N,
        [](int64_t, int64_t row, int64_t col){
            const double dr = row - 8.0;
            const double dc = col - 8.0;
            return static_cast<float>(100.0 * std::exp(-(dr * dr + dc * dc) / 6.0));
        });

    AlignViaDemonsParams params;
    params.max_iterations = 200;
    params.convergence_threshold = 0.0;
    params.deformation_field_smoothing_sigma = 1.0;
    params.update_field_smoothing_sigma = 0.0;
    params.use_diffeomorphic = false;
    params.max_update_magnitude = 2.0;
    params.verbosity = 0;

    auto result = AlignViaDemons(params, img, img);
    REQUIRE(result.has_value());

    // Deformation should be essentially zero.
    CHECK(max_abs_displacement(*result) < 1e-6);

    // Warped image should be identical to the original.
    auto warped = warp_image_with_field(img, *result);
    const auto [mse, count] = compute_mse_and_count(img, warped);
    CHECK(mse < 1e-6);
    CHECK(count == N * N);
}


TEST_CASE( "AlignViaDemons MSE monotonically decreases" ){
    // Verify that the algorithm does not increase MSE on any iteration.
    // We run a few iterations and check intermediate MSE values.
    const int64_t N = 16;

    auto stationary = make_test_image_collection(1, N, N,
        [](int64_t, int64_t row, int64_t col){
            const double dr = row - 8.0;
            const double dc = col - 8.0;
            return static_cast<float>(100.0 * std::exp(-(dr * dr + dc * dc) / 6.0));
        });

    auto moving = make_test_image_collection(1, N, N,
        [](int64_t, int64_t row, int64_t col){
            const double dr = row - 8.0;
            const double dc = col - 8.0 - 1.0;
            return static_cast<float>(100.0 * std::exp(-(dr * dr + dc * dc) / 6.0));
        });

    AlignViaDemonsParams params;
    params.convergence_threshold = 0.0;
    params.deformation_field_smoothing_sigma = 0.5;
    params.update_field_smoothing_sigma = 0.0;
    params.use_diffeomorphic = false;
    params.max_update_magnitude = 2.0;
    params.verbosity = 0;

    double prev_mse = std::numeric_limits<double>::infinity();
    for(int iters : {5, 20, 50, 100}){
        params.max_iterations = iters;
        auto result = AlignViaDemons(params, moving, stationary);
        REQUIRE(result.has_value());
        auto warped = warp_image_with_field(moving, *result);
        const auto [mse, count] = compute_mse_and_count(stationary, warped);
        CHECK(mse < prev_mse);
        prev_mse = mse;
    }
}


TEST_CASE( "AlignViaDemons Gaussian blob shift does not introduce y drift" ){
    // A pure x-shift should not cause drift in the y (row) direction.
    const int64_t N = 20;

    auto stationary = make_test_image_collection(1, N, N,
        [](int64_t, int64_t row, int64_t col){
            const double dr = row - 10.0;
            const double dc = col - 10.0;
            return static_cast<float>(100.0 * std::exp(-(dr * dr + dc * dc) / 8.0));
        });

    auto moving = make_test_image_collection(1, N, N,
        [](int64_t, int64_t row, int64_t col){
            const double dr = row - 10.0;
            const double dc = col - 10.0 - 2.0;
            return static_cast<float>(100.0 * std::exp(-(dr * dr + dc * dc) / 8.0));
        });

    AlignViaDemonsParams params;
    params.max_iterations = 200;
    params.convergence_threshold = 0.0;
    params.deformation_field_smoothing_sigma = 1.0;
    params.update_field_smoothing_sigma = 0.0;
    params.use_diffeomorphic = false;
    params.max_update_magnitude = 2.0;
    params.verbosity = 0;

    auto result = AlignViaDemons(params, moving, stationary);
    REQUIRE(result.has_value());
    auto warped = warp_image_with_field(moving, *result);

    // Compute weighted center of mass in the row direction for the warped image.
    // It should be close to that of the stationary image.
    double stat_row_com = 0.0, stat_sum = 0.0;
    double warp_row_com = 0.0, warp_sum = 0.0;
    for(int64_t row = 0; row < N; ++row){
        for(int64_t col = 0; col < N; ++col){
            const float sv = get_image(stationary.images, 0).value(row, col, 0);
            const float wv = get_image(warped.images, 0).value(row, col, 0);
            if(std::isfinite(sv) && sv > 0.0f){
                stat_row_com += sv * row;
                stat_sum += sv;
            }
            if(std::isfinite(wv) && wv > 0.0f){
                warp_row_com += wv * row;
                warp_sum += wv;
            }
        }
    }
    const double stat_row_mean = stat_row_com / stat_sum;
    const double warp_row_mean = warp_row_com / warp_sum;

    // Row CoM should not drift more than 0.1 pixels.
    CHECK(std::abs(warp_row_mean - stat_row_mean) < 0.1);
}


TEST_CASE( "AlignViaDemons diffeomorphic converges for Gaussian blob shift" ){
    const int64_t N = 20;

    auto stationary = make_test_image_collection(1, N, N,
        [](int64_t, int64_t row, int64_t col){
            const double dr = row - 10.0;
            const double dc = col - 10.0;
            return static_cast<float>(100.0 * std::exp(-(dr * dr + dc * dc) / 8.0));
        });

    auto moving = make_test_image_collection(1, N, N,
        [](int64_t, int64_t row, int64_t col){
            const double dr = row - 10.0;
            const double dc = col - 10.0 - 2.0;
            return static_cast<float>(100.0 * std::exp(-(dr * dr + dc * dc) / 8.0));
        });

    const auto [mse_before, count_before] = compute_mse_and_count(stationary, moving);

    AlignViaDemonsParams params;
    params.max_iterations = 200;
    params.convergence_threshold = 0.0;
    params.deformation_field_smoothing_sigma = 1.0;
    params.update_field_smoothing_sigma = 0.5;
    params.use_diffeomorphic = true;
    params.max_update_magnitude = 2.0;
    params.verbosity = 0;

    auto result = AlignViaDemons(params, moving, stationary);
    REQUIRE(result.has_value());
    auto warped = warp_image_with_field(moving, *result);
    const auto [mse_after, count_after] = compute_mse_and_count(stationary, warped);

    // MSE should drop significantly.
    CHECK(mse_after < mse_before * 0.1);
    CHECK(count_after >= count_before - 3 * N);
}


TEST_CASE( "AlignViaDemons convergence threshold stops iteration" ){
    const int64_t N = 16;

    auto stationary = make_test_image_collection(1, N, N,
        [](int64_t, int64_t row, int64_t col){
            const double dr = row - 8.0;
            const double dc = col - 8.0;
            return static_cast<float>(100.0 * std::exp(-(dr * dr + dc * dc) / 6.0));
        });

    auto moving = make_test_image_collection(1, N, N,
        [](int64_t, int64_t row, int64_t col){
            const double dr = row - 8.0;
            const double dc = col - 8.0 - 1.0;
            return static_cast<float>(100.0 * std::exp(-(dr * dr + dc * dc) / 6.0));
        });

    const auto [mse_before, count_before] = compute_mse_and_count(stationary, moving);

    AlignViaDemonsParams params;
    params.max_iterations = 10000;
    params.convergence_threshold = 0.001;
    params.deformation_field_smoothing_sigma = 0.5;
    params.update_field_smoothing_sigma = 0.0;
    params.use_diffeomorphic = false;
    params.max_update_magnitude = 2.0;
    params.verbosity = 0;

    auto result = AlignViaDemons(params, moving, stationary);
    REQUIRE(result.has_value());
    auto warped = warp_image_with_field(moving, *result);
    const auto [mse_after, count_after] = compute_mse_and_count(stationary, warped);

    // Should have converged, with MSE substantially reduced.
    CHECK(mse_after < mse_before * 0.5);
}


// ---- buffer3 unit tests ----

TEST_CASE( "buffer3 construction and element access" ){
    buffer3<double> buf(2, 3, 4, 1);

    CHECK(buf.N_slices == 2);
    CHECK(buf.N_rows == 3);
    CHECK(buf.N_cols == 4);
    CHECK(buf.N_channels == 1);
    CHECK(buf.data.size() == static_cast<size_t>(2 * 3 * 4 * 1));

    // All values should be zero-initialized.
    for(const auto &v : buf.data){
        CHECK(v == 0.0);
    }

    buf.reference(0, 1, 2) = 42.0;
    CHECK(buf.value(0, 1, 2) == doctest::Approx(42.0));

    buf.reference(1, 2, 3) = -7.5;
    CHECK(buf.value(1, 2, 3) == doctest::Approx(-7.5));
}

TEST_CASE( "buffer3 multi-channel access" ){
    buffer3<double> buf(1, 2, 2, 3);
    buf.reference(0, 0, 0, 0) = 1.0;
    buf.reference(0, 0, 0, 1) = 2.0;
    buf.reference(0, 0, 0, 2) = 3.0;
    CHECK(buf.value(0, 0, 0, 0) == doctest::Approx(1.0));
    CHECK(buf.value(0, 0, 0, 1) == doctest::Approx(2.0));
    CHECK(buf.value(0, 0, 0, 2) == doctest::Approx(3.0));
}

TEST_CASE( "buffer3 in_bounds" ){
    buffer3<double> buf(3, 4, 5, 1);
    CHECK(buf.in_bounds(0, 0, 0));
    CHECK(buf.in_bounds(2, 3, 4));
    CHECK_FALSE(buf.in_bounds(-1, 0, 0));
    CHECK_FALSE(buf.in_bounds(3, 0, 0));
    CHECK_FALSE(buf.in_bounds(0, -1, 0));
    CHECK_FALSE(buf.in_bounds(0, 4, 0));
    CHECK_FALSE(buf.in_bounds(0, 0, -1));
    CHECK_FALSE(buf.in_bounds(0, 0, 5));
}

TEST_CASE( "buffer3 visitor patterns" ){
    buffer3<double> buf(2, 3, 4, 1);
    int64_t count = 0;
    buf.visit_all([&](int64_t, int64_t, int64_t){ ++count; });
    CHECK(count == 2 * 3 * 4);

    count = 0;
    buf.visit_slice_xy(0, [&](int64_t, int64_t){ ++count; });
    CHECK(count == 3 * 4);
}

TEST_CASE( "buffer3 marshalling round-trip with planar_image_collection" ){
    // Create a test collection.
    auto coll = make_test_vector_field(3, 4, 5,
        [](int64_t s, int64_t r, int64_t c){
            return vec3<double>(s * 100.0 + r * 10.0 + c, -(s + r + c), 0.5 * (s + r));
        });

    // Convert to buffer3.
    auto buf = buffer3<double>::from_planar_image_collection(coll);

    CHECK(buf.N_slices == 3);
    CHECK(buf.N_rows == 4);
    CHECK(buf.N_cols == 5);
    CHECK(buf.N_channels == 3);

    // Check data is correctly loaded (first slice, first voxel).
    CHECK(buf.value(0, 0, 0, 0) == doctest::Approx(0.0));
    CHECK(buf.value(0, 0, 0, 1) == doctest::Approx(0.0));
    CHECK(buf.value(0, 0, 0, 2) == doctest::Approx(0.0));

    // Check a middle voxel.
    CHECK(buf.value(1, 2, 3, 0) == doctest::Approx(1 * 100.0 + 2 * 10.0 + 3));
    CHECK(buf.value(1, 2, 3, 1) == doctest::Approx(-(1 + 2 + 3)));

    // Convert back to planar_image_collection.
    auto coll2 = buf.to_planar_image_collection();
    CHECK(coll2.images.size() == 3);

    // Verify data matches original by checking the sorted collection values.
    auto buf2 = buffer3<double>::from_planar_image_collection(coll2);
    for(int64_t s = 0; s < buf.N_slices; ++s){
        for(int64_t r = 0; r < buf.N_rows; ++r){
            for(int64_t c = 0; c < buf.N_cols; ++c){
                for(int64_t ch = 0; ch < buf.N_channels; ++ch){
                    CHECK(buf2.value(s, r, c, ch) == doctest::Approx(buf.value(s, r, c, ch)));
                }
            }
        }
    }
}

TEST_CASE( "buffer3 write_to_planar_image_collection preserves data" ){
    auto coll = make_test_vector_field(2, 3, 3,
        [](int64_t s, int64_t r, int64_t c){
            return vec3<double>(s + r + c, 0.0, 0.0);
        });

    auto buf = buffer3<double>::from_planar_image_collection(coll);

    // Modify the buffer.
    buf.reference(0, 1, 1, 0) = 999.0;

    // Write back.
    buf.write_to_planar_image_collection(coll);

    // Read back to verify.
    auto buf2 = buffer3<double>::from_planar_image_collection(coll);
    CHECK(buf2.value(0, 1, 1, 0) == doctest::Approx(999.0));
}

TEST_CASE( "buffer3 Gaussian smoothing preserves uniform field" ){
    auto coll = make_test_vector_field(2, 5, 5,
        [](int64_t, int64_t, int64_t){
            return vec3<double>(3.0, -1.0, 0.5);
        });

    auto buf = buffer3<double>::from_planar_image_collection(coll);
    buf.gaussian_smooth(1.0);

    // A uniform field should remain unchanged after smoothing.
    for(int64_t s = 0; s < buf.N_slices; ++s){
        for(int64_t r = 0; r < buf.N_rows; ++r){
            for(int64_t c = 0; c < buf.N_cols; ++c){
                CHECK(buf.value(s, r, c, 0) == doctest::Approx(3.0).epsilon(1e-6));
                CHECK(buf.value(s, r, c, 1) == doctest::Approx(-1.0).epsilon(1e-6));
                CHECK(buf.value(s, r, c, 2) == doctest::Approx(0.5).epsilon(1e-6));
            }
        }
    }
}

TEST_CASE( "buffer3 Gaussian smoothing diffuses a spike" ){
    buffer3<double> buf(1, 5, 5, 1);
    buf.reference(0, 2, 2) = 10.0;

    buf.gaussian_smooth(1.0);

    // Center should have decreased.
    CHECK(buf.value(0, 2, 2) < 10.0);
    CHECK(buf.value(0, 2, 2) > 0.0);

    // Neighbours should have received some of the energy.
    CHECK(buf.value(0, 2, 1) > 0.0);
    CHECK(buf.value(0, 1, 2) > 0.0);
}

TEST_CASE( "buffer3 Gaussian smoothing with work_queue" ){
    auto coll = make_test_vector_field(3, 5, 5,
        [](int64_t s, int64_t, int64_t){
            return vec3<double>(s == 1 ? 10.0 : 0.0, 0.0, 0.0);
        });

    auto buf = buffer3<double>::from_planar_image_collection(coll);

    work_queue<std::function<void()>> wq(2);
    buf.gaussian_smooth(1.0, 1.0, 1.0, wq);

    // The spike in slice 1 should have diffused to slices 0 and 2.
    CHECK(buf.value(0, 2, 2, 0) > 0.0);
    CHECK(buf.value(1, 2, 2, 0) < 10.0);
    CHECK(buf.value(1, 2, 2, 0) > 0.0);
    CHECK(buf.value(2, 2, 2, 0) > 0.0);
}

TEST_CASE( "buffer3 parallel_visit_slices" ){
    buffer3<double> buf(4, 3, 3, 1);

    work_queue<std::function<void()>> wq(2);
    buf.parallel_visit_slices(wq, [&](int64_t s){
        for(int64_t r = 0; r < buf.N_rows; ++r){
            for(int64_t c = 0; c < buf.N_cols; ++c){
                buf.reference(s, r, c) = static_cast<double>(s);
            }
        }
    });

    for(int64_t s = 0; s < buf.N_slices; ++s){
        for(int64_t r = 0; r < buf.N_rows; ++r){
            for(int64_t c = 0; c < buf.N_cols; ++c){
                CHECK(buf.value(s, r, c) == doctest::Approx(static_cast<double>(s)));
            }
        }
    }
}

TEST_CASE( "buffer3 parallel_even_odd_slices" ){
    buffer3<double> buf(6, 2, 2, 1);

    work_queue<std::function<void()>> wq(2);
    buf.parallel_even_odd_slices(wq, [&](int64_t s){
        for(int64_t r = 0; r < buf.N_rows; ++r){
            for(int64_t c = 0; c < buf.N_cols; ++c){
                buf.reference(s, r, c) = static_cast<double>(s * 10);
            }
        }
    });

    for(int64_t s = 0; s < buf.N_slices; ++s){
        for(int64_t r = 0; r < buf.N_rows; ++r){
            for(int64_t c = 0; c < buf.N_cols; ++c){
                CHECK(buf.value(s, r, c) == doctest::Approx(static_cast<double>(s * 10)));
            }
        }
    }
}

TEST_CASE( "buffer3 convolve_separable identity kernel" ){
    buffer3<double> buf(1, 3, 3, 1);
    buf.reference(0, 1, 1) = 5.0;

    // Identity kernel: {1.0}
    work_queue<std::function<void()>> wq(1);
    buf.convolve_separable({1.0}, {1.0}, {1.0}, wq);

    CHECK(buf.value(0, 1, 1) == doctest::Approx(5.0));
    CHECK(buf.value(0, 0, 0) == doctest::Approx(0.0));
}

TEST_CASE( "buffer3 from_planar_image_collection rejects empty" ){
    planar_image_collection<double, double> empty;
    CHECK_THROWS_AS(buffer3<double>::from_planar_image_collection(empty), std::invalid_argument);
}

TEST_CASE( "buffer3 write_to_planar_image_collection rejects size mismatch" ){
    auto coll = make_test_vector_field(2, 3, 3,
        [](int64_t, int64_t, int64_t){ return vec3<double>(0.0, 0.0, 0.0); });
    auto buf = buffer3<double>::from_planar_image_collection(coll);

    // Create a collection with a different number of images.
    auto coll2 = make_test_vector_field(1, 3, 3,
        [](int64_t, int64_t, int64_t){ return vec3<double>(0.0, 0.0, 0.0); });
    CHECK_THROWS_AS(buf.write_to_planar_image_collection(coll2), std::invalid_argument);
}

TEST_CASE( "buffer3 spatial ordering during marshalling" ){
    // Create images that are NOT in spatial order (reverse z).
    planar_image_collection<double, double> coll;
    const vec3<double> row_unit(1.0, 0.0, 0.0);
    const vec3<double> col_unit(0.0, 1.0, 0.0);

    // Add slices in reverse order: z=2, z=1, z=0.
    for(int64_t s = 2; s >= 0; --s){
        planar_image<double, double> img;
        img.init_orientation(row_unit, col_unit);
        img.init_buffer(2, 2, 1);
        const vec3<double> slice_offset(0.0, 0.0, static_cast<double>(s));
        img.init_spatial(1.0, 1.0, 1.0, vec3<double>(0.0, 0.0, 0.0), slice_offset);
        img.reference(0, 0, 0) = static_cast<double>(s * 100);
        coll.images.push_back(img);
    }

    // buffer3 should sort them spatially.
    auto buf = buffer3<double>::from_planar_image_collection(coll);

    // Slice 0 (lowest z) should contain value 0.
    CHECK(buf.value(0, 0, 0) == doctest::Approx(0.0));
    // Slice 1 should contain value 100.
    CHECK(buf.value(1, 0, 0) == doctest::Approx(100.0));
    // Slice 2 should contain value 200.
    CHECK(buf.value(2, 0, 0) == doctest::Approx(200.0));
}

TEST_CASE( "buffer3 Gaussian smoothing sigma zero is identity" ){
    buffer3<double> buf(1, 3, 3, 1);
    buf.reference(0, 1, 1) = 5.0;
    buf.reference(0, 0, 0) = 1.0;

    buf.gaussian_smooth(0.0);

    CHECK(buf.value(0, 1, 1) == doctest::Approx(5.0));
    CHECK(buf.value(0, 0, 0) == doctest::Approx(1.0));
}

TEST_CASE( "buffer3 indexing is consistent" ){
    buffer3<double> buf(2, 3, 4, 2);

    // Verify linear index matches expected layout: [slice][row][col][channel]
    int64_t idx = 0;
    for(int64_t s = 0; s < 2; ++s){
        for(int64_t r = 0; r < 3; ++r){
            for(int64_t c = 0; c < 4; ++c){
                for(int64_t ch = 0; ch < 2; ++ch){
                    CHECK(buf.index(s, r, c, ch) == idx);
                    ++idx;
                }
            }
        }
    }
}
