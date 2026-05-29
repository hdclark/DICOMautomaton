//Alignment_Field_Tests.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains unit tests for the alignment field class, which is the basis for vector field-based transforms.
// These tests are separated into their own file because Alignment_Field_obj is linked into
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


using namespace AlignViaFieldHelpers;


static planar_image_collection<double, double>
make_field_test_vector_field(
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

static planar_image_collection<float, double>
make_field_test_image_collection(
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


TEST_CASE( "deformation_field construction from planar_image_collection" ){
    SUBCASE("valid 3-channel images are accepted"){
        auto field_imgs = make_field_test_vector_field(2, 3, 3,
            [](int64_t, int64_t, int64_t){ return vec3<double>(0.0, 0.0, 0.0); });
        REQUIRE_NOTHROW(deformation_field(std::move(field_imgs)));
    }

    SUBCASE("single-channel images are rejected"){
        planar_image_collection<double, double> bad;
        planar_image<double, double> img;
        img.init_orientation(vec3<double>(1,0,0), vec3<double>(0,1,0));
        img.init_buffer(3, 3, 1);
        img.init_spatial(1.0, 1.0, 1.0, vec3<double>(0,0,0), vec3<double>(0,0,0));
        bad.images.push_back(img);
        REQUIRE_THROWS(deformation_field(std::move(bad)));
    }

    SUBCASE("empty collection is rejected"){
        planar_image_collection<double, double> empty;
        REQUIRE_THROWS(deformation_field(std::move(empty)));
    }
}


TEST_CASE( "deformation_field::transform applies displacement" ){
    auto field_imgs = make_field_test_vector_field(1, 3, 3,
        [](int64_t, int64_t, int64_t){ return vec3<double>(1.0, -0.5, 0.0); });
    deformation_field field(std::move(field_imgs));

    const vec3<double> input(1.0, 1.0, 0.0);
    const auto output = field.transform(input);
    CHECK(output.x == doctest::Approx(2.0));
    CHECK(output.y == doctest::Approx(0.5));
    CHECK(output.z == doctest::Approx(0.0));
}


TEST_CASE( "deformation_field::transform zero field is identity" ){
    auto field_imgs = make_field_test_vector_field(1, 5, 5,
        [](int64_t, int64_t, int64_t){ return vec3<double>(0.0, 0.0, 0.0); });
    deformation_field field(std::move(field_imgs));

    const vec3<double> p(2.0, 3.0, 0.0);
    const auto q = field.transform(p);
    CHECK(q.x == doctest::Approx(p.x));
    CHECK(q.y == doctest::Approx(p.y));
    CHECK(q.z == doctest::Approx(p.z));
}


TEST_CASE( "deformation_field::apply_to(vec3) modifies vector" ){
    auto field_imgs = make_field_test_vector_field(1, 3, 3,
        [](int64_t, int64_t, int64_t){ return vec3<double>(0.5, 0.5, 0.0); });
    deformation_field field(std::move(field_imgs));

    vec3<double> v(1.0, 1.0, 0.0);
    field.apply_to(v);
    CHECK(v.x == doctest::Approx(1.5));
    CHECK(v.y == doctest::Approx(1.5));
}


TEST_CASE( "deformation_field::apply_to(point_set) transforms all points" ){
    auto field_imgs = make_field_test_vector_field(1, 5, 5,
        [](int64_t, int64_t, int64_t){ return vec3<double>(1.0, 0.0, 0.0); });
    deformation_field field(std::move(field_imgs));

    point_set<double> ps;
    ps.points.push_back(vec3<double>(1.0, 1.0, 0.0));
    ps.points.push_back(vec3<double>(2.0, 2.0, 0.0));
    field.apply_to(ps);
    CHECK(ps.points[0].x == doctest::Approx(2.0));
    CHECK(ps.points[1].x == doctest::Approx(3.0));
}


TEST_CASE( "deformation_field write_to and read_from roundtrip" ){
    auto field_imgs = make_field_test_vector_field(2, 3, 4,
        [](int64_t slice, int64_t row, int64_t col){
            return vec3<double>(0.1 * col, -0.2 * row, 0.05 * slice);
        });

    // Add metadata (including special characters) to test serialization.
    for(auto &img : field_imgs.images){
        img.metadata["PatientID"] = "test_patient_001";
        img.metadata["key with spaces"] = "value;with@special=chars";
    }

    deformation_field field(std::move(field_imgs));

    std::stringstream ss;
    REQUIRE(field.write_to(ss));

    deformation_field field2(ss);

    // Verify the roundtripped field produces the same transforms.
    const auto &imgs1 = field.get_imagecoll_crefw().get().images;
    const auto &imgs2 = field2.get_imagecoll_crefw().get().images;
    REQUIRE(imgs1.size() == imgs2.size());

    auto it1 = imgs1.begin();
    auto it2 = imgs2.begin();
    for(; it1 != imgs1.end(); ++it1, ++it2){
        REQUIRE(it1->rows == it2->rows);
        REQUIRE(it1->columns == it2->columns);
        REQUIRE(it1->channels == it2->channels);
        CHECK(it1->pxl_dx == doctest::Approx(it2->pxl_dx));
        CHECK(it1->pxl_dy == doctest::Approx(it2->pxl_dy));
        CHECK(it1->pxl_dz == doctest::Approx(it2->pxl_dz));
        for(size_t i = 0; i < it1->data.size(); ++i){
            CHECK(it1->data[i] == doctest::Approx(it2->data[i]));
        }
        // Verify metadata roundtrip.
        CHECK(it1->metadata.size() == it2->metadata.size());
        for(const auto &mp : it1->metadata){
            auto it_m = it2->metadata.find(mp.first);
            REQUIRE(it_m != it2->metadata.end());
            CHECK(it_m->second == mp.second);
        }
    }

    // Verify transform produces the same result.
    const vec3<double> test_pt(1.5, 0.5, 0.5);
    const auto t1 = field.transform(test_pt);
    const auto t2 = field2.transform(test_pt);
    CHECK(t1.x == doctest::Approx(t2.x));
    CHECK(t1.y == doctest::Approx(t2.y));
    CHECK(t1.z == doctest::Approx(t2.z));
}


TEST_CASE( "deformation_field write_to and read_from preserves zero field" ){
    auto field_imgs = make_field_test_vector_field(1, 2, 2,
        [](int64_t, int64_t, int64_t){ return vec3<double>(0.0, 0.0, 0.0); });
    deformation_field field(std::move(field_imgs));

    std::stringstream ss;
    REQUIRE(field.write_to(ss));

    deformation_field field2(ss);
    const vec3<double> test_pt(0.5, 0.5, 0.0);
    const auto result = field2.transform(test_pt);
    CHECK(result.x == doctest::Approx(test_pt.x));
    CHECK(result.y == doctest::Approx(test_pt.y));
    CHECK(result.z == doctest::Approx(test_pt.z));
}


TEST_CASE( "deformation_field read_from rejects invalid input" ){
    SUBCASE("empty stream"){
        std::stringstream ss("");
        REQUIRE_THROWS(deformation_field(ss));
    }

    SUBCASE("negative image count"){
        std::stringstream ss("-1\n");
        REQUIRE_THROWS(deformation_field(ss));
    }

    SUBCASE("zero image count"){
        std::stringstream ss("0\n");
        REQUIRE_THROWS(deformation_field(ss));
    }

    SUBCASE("truncated data"){
        std::stringstream ss("1\n3 3 3\n");
        REQUIRE_THROWS(deformation_field(ss));
    }
}


TEST_CASE( "deformation_field::apply_to(image) pull method with zero field" ){
    auto field_imgs = make_field_test_vector_field(1, 5, 5,
        [](int64_t, int64_t, int64_t){ return vec3<double>(0.0, 0.0, 0.0); });
    deformation_field field(std::move(field_imgs));

    auto img_coll = make_field_test_image_collection(1, 5, 5,
        [](int64_t, int64_t row, int64_t col){ return static_cast<float>(row * 10 + col); });
    auto &img = img_coll.images.front();
    const auto orig_img = img;

    field.apply_to(img, deformation_field_warp_method::pull);

    for(int64_t row = 0; row < img.rows; ++row){
        for(int64_t col = 0; col < img.columns; ++col){
            CHECK(img.value(row, col, 0) == doctest::Approx(orig_img.value(row, col, 0)).epsilon(0.01));
        }
    }
}


TEST_CASE( "deformation_field::apply_to(image) push method with zero field" ){
    auto field_imgs = make_field_test_vector_field(1, 5, 5,
        [](int64_t, int64_t, int64_t){ return vec3<double>(0.0, 0.0, 0.0); });
    deformation_field field(std::move(field_imgs));

    auto img_coll = make_field_test_image_collection(1, 5, 5,
        [](int64_t, int64_t row, int64_t col){ return static_cast<float>(row * 10 + col); });
    auto &img = img_coll.images.front();
    const auto orig_img = img;

    field.apply_to(img, deformation_field_warp_method::push);

    for(int64_t row = 0; row < img.rows; ++row){
        for(int64_t col = 0; col < img.columns; ++col){
            CHECK(img.value(row, col, 0) == doctest::Approx(orig_img.value(row, col, 0)).epsilon(0.01));
        }
    }
}


TEST_CASE( "deformation_field::apply_to(image_collection) pull method" ){
    auto field_imgs = make_field_test_vector_field(2, 4, 4,
        [](int64_t, int64_t, int64_t){ return vec3<double>(0.0, 0.0, 0.0); });
    deformation_field field(std::move(field_imgs));

    auto img_coll = make_field_test_image_collection(2, 4, 4,
        [](int64_t slice, int64_t row, int64_t col){
            return static_cast<float>(slice * 100 + row * 10 + col);
        });
    const auto orig_coll = img_coll;

    field.apply_to(img_coll, deformation_field_warp_method::pull);

    auto it_orig = orig_coll.images.begin();
    auto it_new = img_coll.images.begin();
    for(; it_orig != orig_coll.images.end(); ++it_orig, ++it_new){
        for(int64_t row = 0; row < it_orig->rows; ++row){
            for(int64_t col = 0; col < it_orig->columns; ++col){
                CHECK(it_new->value(row, col, 0) == doctest::Approx(it_orig->value(row, col, 0)).epsilon(0.01));
            }
        }
    }
}


TEST_CASE( "deformation_field::apply_to(image) pull method with uniform translation" ){
    // A uniform displacement of +1 in x (col direction).
    // In the pull model, the inverse displacement is -1 in x.
    // So for an image where value = col, the output at col should look up col-1, giving value (col-1).
    const int64_t N = 10;
    auto field_imgs = make_field_test_vector_field(1, N, N,
        [](int64_t, int64_t, int64_t){ return vec3<double>(1.0, 0.0, 0.0); });
    deformation_field field(std::move(field_imgs));

    auto img_coll = make_field_test_image_collection(1, N, N,
        [](int64_t, int64_t, int64_t col){ return static_cast<float>(col); });
    auto &img = img_coll.images.front();

    field.apply_to(img, deformation_field_warp_method::pull);

    // Interior pixels (away from boundaries) should have value ≈ col - 1.
    for(int64_t row = 1; row < N - 1; ++row){
        for(int64_t col = 2; col < N - 2; ++col){
            CHECK(img.value(row, col, 0) == doctest::Approx(static_cast<float>(col - 1)).epsilon(0.2));
        }
    }
}


TEST_CASE( "deformation_field::apply_to(image) push method with uniform translation" ){
    // A uniform displacement of +1 in x (col direction).
    // In the push model, content at col is pushed to col+1.
    // So for an image where value = col, the output at col+1 should be col.
    const int64_t N = 10;
    auto field_imgs = make_field_test_vector_field(1, N, N,
        [](int64_t, int64_t, int64_t){ return vec3<double>(1.0, 0.0, 0.0); });
    deformation_field field(std::move(field_imgs));

    auto img_coll = make_field_test_image_collection(1, N, N,
        [](int64_t, int64_t, int64_t col){ return static_cast<float>(col); });
    auto &img = img_coll.images.front();

    field.apply_to(img, deformation_field_warp_method::push);

    // Interior pixels should have the value from the previous column.
    for(int64_t row = 1; row < N - 1; ++row){
        for(int64_t col = 1; col < N - 1; ++col){
            const float expected = static_cast<float>(col - 1);
            CHECK(img.value(row, col, 0) == doctest::Approx(expected).epsilon(0.5));
        }
    }
    // Column 0 should be NaN (nothing was pushed here from col=-1 which doesn't exist).
    for(int64_t row = 0; row < N; ++row){
        CHECK(std::isnan(img.value(row, 0, 0)));
    }
}


TEST_CASE( "deformation_field::apply_to default method is pull" ){
    auto field_imgs = make_field_test_vector_field(1, 3, 3,
        [](int64_t, int64_t, int64_t){ return vec3<double>(0.0, 0.0, 0.0); });
    deformation_field field(std::move(field_imgs));

    auto img_coll = make_field_test_image_collection(1, 3, 3,
        [](int64_t, int64_t row, int64_t col){ return static_cast<float>(row + col); });
    auto &img = img_coll.images.front();
    const auto orig = img;

    // Call apply_to without specifying method (should default to pull).
    field.apply_to(img);
    for(int64_t row = 0; row < 3; ++row){
        for(int64_t col = 0; col < 3; ++col){
            CHECK(img.value(row, col, 0) == doctest::Approx(orig.value(row, col, 0)).epsilon(0.01));
        }
    }
}


TEST_CASE( "deformation_field move and copy preserve transform" ){
    // After a move or copy, the adjacency index must be rebuilt so that
    // transform() continues to work correctly with bilinear interpolation.
    auto field_images = make_field_test_vector_field(1, 5, 5,
        [](int64_t, int64_t, int64_t){
            return vec3<double>(0.5, -0.25, 0.0);
        });

    deformation_field original(std::move(field_images));
    const vec3<double> p(2.0, 2.0, 0.0);
    const auto orig_result = original.transform(p);

    SUBCASE("move constructor"){
        deformation_field moved(std::move(original));
        const auto moved_result = moved.transform(p);
        CHECK(moved_result.x == doctest::Approx(orig_result.x).epsilon(1e-9));
        CHECK(moved_result.y == doctest::Approx(orig_result.y).epsilon(1e-9));
        CHECK(moved_result.z == doctest::Approx(orig_result.z).epsilon(1e-9));
    }

    SUBCASE("copy constructor"){
        deformation_field copied(original);
        const auto copy_result = copied.transform(p);
        CHECK(copy_result.x == doctest::Approx(orig_result.x).epsilon(1e-9));
        CHECK(copy_result.y == doctest::Approx(orig_result.y).epsilon(1e-9));
        CHECK(copy_result.z == doctest::Approx(orig_result.z).epsilon(1e-9));
    }
}


TEST_CASE( "deformation_field transform uses bilinear interpolation" ){
    // Verify that transform() uses proper bilinear interpolation, not
    // nearest-neighbour. A query at a half-pixel position should return
    // an interpolated displacement, not a snapped value.
    auto field_images = make_field_test_vector_field(1, 5, 5,
        [](int64_t, int64_t, int64_t col){
            // Displacement linearly varies with column: dx = col
            return vec3<double>(static_cast<double>(col), 0.0, 0.0);
        });

    deformation_field field(std::move(field_images));

    // Query at position (1.5, 2, 0) which is between col=1 (dx=1) and col=2 (dx=2).
    // With bilinear interpolation, dx should be 1.5.
    // With nearest-neighbour, dx would snap to either 1.0 or 2.0.
    const vec3<double> p(1.5, 2.0, 0.0);
    const auto result = field.transform(p);
    const double displacement_x = result.x - p.x;
    CHECK(displacement_x == doctest::Approx(1.5).epsilon(0.01));
}
