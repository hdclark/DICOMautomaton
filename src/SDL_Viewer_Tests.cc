// SDL_Viewer_Tests.cc - Unit tests for SDL_Viewer utility functions.
//
// A part of DICOMautomaton 2020-2025. Written by hal clark.
//
// These tests cover non-GUI utility functions that can be tested without 
// an active graphical context.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <tuple>
#include <vector>

#include "doctest20251212/doctest.h"

#include "YgorImages.h"
#include "YgorMath.h"

#include "Operations/SDL_Viewer_Utils.h"
#include "Operations/SDL_Viewer_Brushes.h"


// ============================================================================
// Test cases for get_pixelspace_axis_aligned_bounding_box
// ============================================================================

TEST_CASE( "get_pixelspace_axis_aligned_bounding_box basic point at center" ){
    // Create a simple 10x10 image centered at origin
    planar_image<float, double> img;
    img.init_buffer(10, 10, 1);
    img.init_spatial(1.0, 1.0, 1.0, vec3<double>(0,0,0), vec3<double>(0,0,0));
    img.init_orientation(vec3<double>(1,0,0), vec3<double>(0,1,0));

    // Single point at the center of the image
    std::vector<vec3<double>> points = { vec3<double>(5.0, 5.0, 0.0) };
    double extra_space = 0.5;

    auto [row_min, row_max, col_min, col_max] = get_pixelspace_axis_aligned_bounding_box(img, points, extra_space);

    // Verify we get a valid bounding box
    CHECK(row_min >= 0);
    CHECK(row_max < img.rows);
    CHECK(col_min >= 0);
    CHECK(col_max < img.columns);
    CHECK(row_min <= row_max);
    CHECK(col_min <= col_max);
}

TEST_CASE( "get_pixelspace_axis_aligned_bounding_box empty points" ){
    // Create a simple 10x10 image
    planar_image<float, double> img;
    img.init_buffer(10, 10, 1);
    img.init_spatial(1.0, 1.0, 1.0, vec3<double>(0,0,0), vec3<double>(0,0,0));
    img.init_orientation(vec3<double>(1,0,0), vec3<double>(0,1,0));

    // Empty points list - should handle gracefully
    std::vector<vec3<double>> points = {};
    double extra_space = 1.0;

    // This may return clamped values since bbox is at infinity
    auto [row_min, row_max, col_min, col_max] = get_pixelspace_axis_aligned_bounding_box(img, points, extra_space);

    // Values should at least be clamped to valid bounds
    CHECK(row_min >= 0);
    CHECK(col_min >= 0);
}

TEST_CASE( "get_pixelspace_axis_aligned_bounding_box multiple points" ){
    // Create a 20x20 image
    planar_image<float, double> img;
    img.init_buffer(20, 20, 1);
    img.init_spatial(1.0, 1.0, 1.0, vec3<double>(0,0,0), vec3<double>(0,0,0));
    img.init_orientation(vec3<double>(1,0,0), vec3<double>(0,1,0));

    // Multiple points forming a square
    std::vector<vec3<double>> points = {
        vec3<double>(5.0, 5.0, 0.0),
        vec3<double>(15.0, 5.0, 0.0),
        vec3<double>(15.0, 15.0, 0.0),
        vec3<double>(5.0, 15.0, 0.0)
    };
    double extra_space = 1.0;

    auto [row_min, row_max, col_min, col_max] = get_pixelspace_axis_aligned_bounding_box(img, points, extra_space);

    // The bounding box should encompass all points plus extra_space
    CHECK(row_min >= 0);
    CHECK(row_max < img.rows);
    CHECK(col_min >= 0);
    CHECK(col_max < img.columns);
    
    // With extra_space, the box should be larger than the point extent
    CHECK(row_max - row_min >= 10);  // At least 10 pixels span (15-5 = 10)
    CHECK(col_max - col_min >= 10);
}

TEST_CASE( "get_pixelspace_axis_aligned_bounding_box with non-zero extra_space" ){
    // Create a 100x100 image for more precision
    planar_image<float, double> img;
    img.init_buffer(100, 100, 1);
    img.init_spatial(1.0, 1.0, 1.0, vec3<double>(0,0,0), vec3<double>(0,0,0));
    img.init_orientation(vec3<double>(1,0,0), vec3<double>(0,1,0));

    // Single point in middle
    std::vector<vec3<double>> points = { vec3<double>(50.0, 50.0, 0.0) };
    
    // Test with different extra_space values
    {
        double extra_space = 0.0;
        auto [row_min, row_max, col_min, col_max] = get_pixelspace_axis_aligned_bounding_box(img, points, extra_space);
        CHECK(row_min >= 0);
        CHECK(row_max < img.rows);
    }
    
    {
        double extra_space = 5.0;
        auto [row_min, row_max, col_min, col_max] = get_pixelspace_axis_aligned_bounding_box(img, points, extra_space);
        // With 5.0 extra space, should be a larger bounding box
        CHECK((row_max - row_min) >= 8);  // At least ~2*extra_space coverage
        CHECK((col_max - col_min) >= 8);
    }
}


// ============================================================================
// Test cases for brush_t enum
// ============================================================================

TEST_CASE( "brush_t enum values" ){
    // Verify that all brush types are distinct
    CHECK(static_cast<int>(brush_t::rigid_circle) != static_cast<int>(brush_t::rigid_square));
    CHECK(static_cast<int>(brush_t::gaussian_2D) != static_cast<int>(brush_t::gaussian_3D));
    CHECK(static_cast<int>(brush_t::tanh_2D) != static_cast<int>(brush_t::tanh_3D));
    CHECK(static_cast<int>(brush_t::median_circle) != static_cast<int>(brush_t::median_sphere));
    CHECK(static_cast<int>(brush_t::mean_circle) != static_cast<int>(brush_t::mean_sphere));
    
    // Verify 2D and 3D variants are distinct
    CHECK(static_cast<int>(brush_t::rigid_circle) != static_cast<int>(brush_t::rigid_sphere));
    CHECK(static_cast<int>(brush_t::rigid_square) != static_cast<int>(brush_t::rigid_cube));
}

