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

TEST_CASE( "compute_gradient respects image orientation" ){
    planar_image_collection<float, double> coll;
    planar_image<float, double> img;
    // Flip row/column orientation to ensure gradient is expressed in world coordinates.
    const vec3<double> row_unit(-1.0, 0.0, 0.0);
    const vec3<double> col_unit(0.0, -1.0, 0.0);
    img.init_orientation(row_unit, col_unit);
    img.init_buffer(3, 3, 1);
    img.init_spatial(1.0, 1.0, 1.0, vec3<double>(0.0, 0.0, 0.0), vec3<double>(0.0, 0.0, 0.0));
    for(int64_t row = 0; row < img.rows; ++row){
        for(int64_t col = 0; col < img.columns; ++col){
            img.reference(row, col, 0) = static_cast<float>(2.0 * row + col);
        }
    }
    coll.images.push_back(img);

    auto gradient = compute_gradient(coll);
    const auto &grad_img = get_image(gradient.images, 0);
    CHECK(grad_img.value(1, 1, 0) == doctest::Approx(-1.0));
    CHECK(grad_img.value(1, 1, 1) == doctest::Approx(-2.0));
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
