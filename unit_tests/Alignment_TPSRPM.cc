
#include <limits>
#include <utility>
#include <iostream>

#include "YgorMath.h"

#include "doctest/doctest.h"

#include "Structs.h"
#include "Alignment_TPSRPM.h"


TEST_CASE( "thin_plate_spline class" ){
    //const auto nan = std::numeric_limits<double>::quiet_NaN();
    //const auto inf = std::numeric_limits<double>::infinity();
    const double pi = 3.141592653;

    point_set<double> ps_A;
    ps_A.points.emplace_back( vec3<double>( 0.0,  0.0,  0.0) );
    ps_A.points.emplace_back( vec3<double>( 1.0,  0.0,  0.0) );
    ps_A.points.emplace_back( vec3<double>( 0.0,  1.0,  0.0) );
    ps_A.points.emplace_back( vec3<double>( 0.0,  0.0,  1.0) );
    ps_A.points.emplace_back( vec3<double>( 1.0,  1.0,  0.0) );
    ps_A.points.emplace_back( vec3<double>( 1.0,  0.0,  1.0) );
    ps_A.points.emplace_back( vec3<double>( 0.0,  1.0,  1.0) );
    ps_A.points.emplace_back( vec3<double>( 1.0,  1.0,  1.0) );

    point_set<double> ps_B;
    for(const auto &p : ps_A.points){
        ps_B.points.emplace_back( p.rotate_around_x(pi*0.05).rotate_around_y(-pi*0.05).rotate_around_z(pi*0.05) );
    }

    SUBCASE("constructors"){
        long int kdim2 = 2;
        thin_plate_spline tps_A(ps_A, kdim2);
        REQUIRE( tps_A.control_points.points == ps_A.points );
        REQUIRE( tps_A.kernel_dimension == kdim2 );

        long int kdim3 = 3;
        thin_plate_spline tps_B(ps_B, kdim3);
        REQUIRE( tps_B.control_points.points == ps_B.points );
        REQUIRE( tps_B.kernel_dimension == kdim3 );
    }

    SUBCASE("transform") {
        AlignViaTPSParams params;
        std::optional<thin_plate_spline> transform_opt = AlignViaTPS(params, ps_B, ps_A);
        REQUIRE(transform_opt);

        transform_opt.value().apply_to(ps_B);
        const double margin = 0.00001;        // error margin

        for (int i = 0; i < ps_B.points.size(); ++i) {
            REQUIRE(std::abs(ps_B.points[i].x - ps_A.points[i].x) < margin);
            REQUIRE(std::abs(ps_B.points[i].y - ps_A.points[i].y) < margin);
            REQUIRE(std::abs(ps_B.points[i].z - ps_A.points[i].z) < margin);
        }
    }
}

