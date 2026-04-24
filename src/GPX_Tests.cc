//GPX_Tests.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <string>
#include <vector>

#include "doctest20251212/doctest.h"

#include "YgorMath.h"

#include "GPX.h"

TEST_CASE( "predefined lower mainland MTB boundary contours are available" ){
    const auto boundaries = dcma::gpx::generate_predefined_boundary_contours_lower_mainland_mtb();

    REQUIRE( 13 <= boundaries.size() );
    REQUIRE( boundaries.front().contour.closed );

    bool found_fromme = false;
    bool found_vedder = false;
    for(const auto &boundary : boundaries){
        found_fromme = found_fromme || (boundary.name == "Fromme");
        found_vedder = found_vedder || (boundary.name == "Vedder Mountain");
        CHECK( 3 <= boundary.contour.points.size() );
        CHECK( boundary.contour.closed );
    }

    CHECK( found_fromme );
    CHECK( found_vedder );
}

TEST_CASE( "custom GPX boundary specifications are parsed" ){
    const auto boundaries = dcma::gpx::parse_boundary_contours(
        "boundary(Test Area, 49.0, -123.0, 49.0, -122.9, 49.1, -122.9, 49.1, -123.0);"
        "boundary(49.2, -123.1, 49.2, -123.0, 49.3, -123.0, 49.3, -123.1)" );

    REQUIRE( boundaries.size() == 2 );
    CHECK( boundaries.at(0).name == "Test Area" );
    CHECK( boundaries.at(1).name == "Unnamed Boundary 1" );
    CHECK( boundaries.at(0).contour.closed );
    CHECK( boundaries.at(1).contour.closed );
}

TEST_CASE( "open GPX contours split on debounced boundary crossings" ){
    static constexpr double boundary_left = 1.0;
    static constexpr double boundary_right = 3.0;
    static constexpr double boundary_bottom = -1.0;
    static constexpr double boundary_top = 1.0;
    static constexpr double debounce_distance = 4.0;

    dcma::gpx::named_boundary_contour boundary;
    boundary.name = "Trailhead";
    boundary.contour.closed = true;
    boundary.contour.points = {
        vec3<double>(boundary_left,  boundary_bottom, 0.0),
        vec3<double>(boundary_right, boundary_bottom, 0.0),
        vec3<double>(boundary_right, boundary_top,    0.0),
        vec3<double>(boundary_left,  boundary_top,    0.0),
    };

    contour_of_points<double> track;
    track.closed = false;
    track.points = {
        vec3<double>(0.0, 0.0, 0.0),
        vec3<double>(2.0, 0.0, 0.0),   // crossing #1
        vec3<double>(1.2, 0.0, 0.0),   // jitter
        vec3<double>(0.8, 0.0, 0.0),   // jitter across boundary, should be ignored
        vec3<double>(1.1, 0.0, 0.0),   // jitter across boundary, should be ignored
        vec3<double>(6.0, 0.0, 0.0),   // re-arm debounce
        vec3<double>(2.0, 0.0, 0.0),   // crossing #2
        vec3<double>(4.0, 0.0, 0.0),
    };

    const auto segments = dcma::gpx::split_open_contour_on_boundary_crossings(track, { boundary }, debounce_distance);

    REQUIRE( segments.size() == 4 );
    CHECK( segments.at(0).contour.points.size() == 2 );
    CHECK( segments.at(1).contour.points.size() == 5 );
    CHECK( segments.at(2).contour.points.size() == 2 );
    CHECK( segments.at(3).contour.points.size() == 2 );
    CHECK( segments.at(0).location_name == "Trailhead" );
    CHECK( segments.at(1).location_name == "Trailhead" );
    CHECK( segments.at(2).location_name == "Trailhead" );
    CHECK( segments.at(3).location_name == "Trailhead transit" );
}
