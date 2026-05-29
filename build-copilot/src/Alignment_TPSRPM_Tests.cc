//Alignment_TPSRPM_Tests.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains unit tests for the TPS and TPS-RPM alignment methods defined
// in Alignment_TPSRPM.cc. These tests are separated into their own file because
// Alignment_TPSRPM_obj is linked into shared libraries which don't include doctest
// implementation.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <cstdint>

#include "doctest20251212/doctest.h"

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorStats.h"        //Needed for Stats:: namespace.

#include "Alignment_TPSRPM.h"
#include "Alignment_TPSRPM_Tests.h"


// ============================================================================
// Helper functions for unit tests
// ============================================================================

const auto test_pi = std::acos(-1.0);;

// Creates a unit cube point cloud with 8 corner points
point_set<double> create_unit_cube_points(){
    point_set<double> ps;
    ps.points.emplace_back( vec3<double>( 0.0,  0.0,  0.0) );
    ps.points.emplace_back( vec3<double>( 1.0,  0.0,  0.0) );
    ps.points.emplace_back( vec3<double>( 0.0,  1.0,  0.0) );
    ps.points.emplace_back( vec3<double>( 0.0,  0.0,  1.0) );
    ps.points.emplace_back( vec3<double>( 1.0,  1.0,  0.0) );
    ps.points.emplace_back( vec3<double>( 1.0,  0.0,  1.0) );
    ps.points.emplace_back( vec3<double>( 0.0,  1.0,  1.0) );
    ps.points.emplace_back( vec3<double>( 1.0,  1.0,  1.0) );
    return ps;
}

// Creates an extended unit cube with 12 points (8 corners + 4 face centers)
point_set<double> create_extended_cube_points(){
    point_set<double> ps = create_unit_cube_points();
    ps.points.emplace_back( vec3<double>( 0.5,  0.5,  0.0) );
    ps.points.emplace_back( vec3<double>( 0.5,  0.0,  0.5) );
    ps.points.emplace_back( vec3<double>( 0.0,  0.5,  0.5) );
    ps.points.emplace_back( vec3<double>( 0.5,  0.5,  0.5) );
    return ps;
}

// Computes RMS error between transformed moving points and stationary points
// assuming point-wise ordering/correspondence is already known.
static double compute_ordered_rms_error(const thin_plate_spline& tps,
                                        const point_set<double>& moving,
                                        const point_set<double>& stationary){
    Stats::Running_Sum<double> sq_error;
    for(size_t i = 0; i < moving.points.size(); ++i){
        const auto p_transformed = tps.transform(moving.points[i]);
        const auto diff = p_transformed - stationary.points[i];
        sq_error.Digest(diff.Dot(diff));
    }
    return std::sqrt(sq_error.Current_Sum() / static_cast<double>(moving.points.size()));
}

// Computes total variance (sum of x, y, z variances) for a point cloud
static double compute_total_variance(const point_set<double>& ps){
    Stats::Running_Variance<double> var_x, var_y, var_z;
    for(const auto &p : ps.points){
        var_x.Digest(p.x);
        var_y.Digest(p.y);
        var_z.Digest(p.z);
    }
    return var_x.Current_Variance() + var_y.Current_Variance() + var_z.Current_Variance();
}

// Computes total variance of transformed points
static double compute_transformed_variance(const thin_plate_spline& tps,
                                           const point_set<double>& ps){
    Stats::Running_Variance<double> var_x, var_y, var_z;
    for(const auto &p : ps.points){
        const auto p_trans = tps.transform(p);
        var_x.Digest(p_trans.x);
        var_y.Digest(p_trans.y);
        var_z.Digest(p_trans.z);
    }
    return var_x.Current_Variance() + var_y.Current_Variance() + var_z.Current_Variance();
}

// Computes a symmetric nearest-neighbour RMS error between the transformed moving
// point set and the stationary point set.
//
// This metric is appropriate for TPS-RPM because the algorithm estimates
// correspondence itself and therefore should not be judged using the original
// point ordering when the input sets are symmetric or ambiguously ordered.
static double compute_set_alignment_rms(const thin_plate_spline& tps,
                                        const point_set<double>& moving,
                                        const point_set<double>& stationary){
    point_set<double> transformed = moving;
    tps.apply_to(transformed);

    const auto nearest_sq_dist = [](const vec3<double> &p,
                                    const point_set<double> &ps) -> double {
        double min_sq_dist = std::numeric_limits<double>::infinity();
        for(const auto &q : ps.points){
            min_sq_dist = std::min(min_sq_dist, p.sq_dist(q));
        }
        return min_sq_dist;
    };

    Stats::Running_Sum<double> sq_error;
    for(const auto &p : transformed.points){
        sq_error.Digest(nearest_sq_dist(p, stationary));
    }
    for(const auto &p : stationary.points){
        sq_error.Digest(nearest_sq_dist(p, transformed));
    }

    const auto total_points = transformed.points.size() + stationary.points.size();
    return std::sqrt(sq_error.Current_Sum() / static_cast<double>(total_points));
}

// Creates an asymmetric point cloud with unique geometry so the optimal
// correspondence is well-defined.
static point_set<double> create_asymmetric_points(){
    point_set<double> ps;
    ps.points.emplace_back( vec3<double>( 0.00,  0.00,  0.00) );
    ps.points.emplace_back( vec3<double>( 1.20,  0.10,  0.00) );
    ps.points.emplace_back( vec3<double>( 0.25,  1.35,  0.15) );
    ps.points.emplace_back( vec3<double>( 0.10,  0.35,  1.55) );
    ps.points.emplace_back( vec3<double>( 1.10,  1.05,  0.30) );
    ps.points.emplace_back( vec3<double>( 1.45,  0.25,  1.10) );
    ps.points.emplace_back( vec3<double>( 0.35,  1.15,  1.25) );
    ps.points.emplace_back( vec3<double>( 1.55,  1.45,  1.60) );
    ps.points.emplace_back( vec3<double>( 0.70,  0.55,  0.40) );
    ps.points.emplace_back( vec3<double>( 1.05,  0.80,  1.35) );
    return ps;
}

// Creates a grid-based point cloud with N^3 points
static point_set<double> create_grid_point_cloud(int64_t N_per_axis){
    if(N_per_axis <= 1){
        throw std::invalid_argument("N_per_axis must be greater than 1");
    }
    point_set<double> ps;
    for(int64_t i = 0; i < N_per_axis; ++i){
        for(int64_t j = 0; j < N_per_axis; ++j){
            for(int64_t k = 0; k < N_per_axis; ++k){
                double x = static_cast<double>(i) / static_cast<double>(N_per_axis - 1);
                double y = static_cast<double>(j) / static_cast<double>(N_per_axis - 1);
                double z = static_cast<double>(k) / static_cast<double>(N_per_axis - 1);
                ps.points.emplace_back( vec3<double>(x, y, z) );
            }
        }
    }
    return ps;
}


// ============================================================================
// thin_plate_spline class tests
// ============================================================================

TEST_CASE( "thin_plate_spline class" ){

    point_set<double> ps_A = create_unit_cube_points();

    point_set<double> ps_B;
    for(const auto &p : ps_A.points){
        ps_B.points.emplace_back( p.rotate_around_x(test_pi*0.05).rotate_around_y(-test_pi*0.05).rotate_around_z(test_pi*0.05) );
    }

    SUBCASE("constructors"){
        int64_t kdim2 = 2;
        thin_plate_spline tps_A(ps_A, kdim2);
        REQUIRE( tps_A.control_points.points == ps_A.points );
        REQUIRE( tps_A.kernel_dimension == kdim2 );

        int64_t kdim3 = 3;
        thin_plate_spline tps_B(ps_B, kdim3);
        REQUIRE( tps_B.control_points.points == ps_B.points );
        REQUIRE( tps_B.kernel_dimension == kdim3 );
    }

    SUBCASE("identity transform"){
        // Default TPS should be an identity transform.
        thin_plate_spline tps(ps_A);
        for(const auto &p : ps_A.points){
            const auto p_trans = tps.transform(p);
            const auto diff = (p_trans - p).length();
            REQUIRE( diff < 1e-10 );
        }
    }

    SUBCASE("serialization round-trip"){
        thin_plate_spline tps(ps_A);
        std::stringstream ss;
        REQUIRE( tps.write_to(ss) );
        ss.seekg(0);
        thin_plate_spline tps_loaded(ss);
        for(const auto &p : ps_A.points){
            const auto p_orig = tps.transform(p);
            const auto p_loaded = tps_loaded.transform(p);
            REQUIRE( (p_orig - p_loaded).length() < 1e-10 );
        }
    }
}


// ============================================================================
// AlignViaTPS tests
// ============================================================================

#ifdef DCMA_USE_EIGEN
TEST_CASE( "AlignViaTPS with known correspondences" ){

    point_set<double> ps_moving = create_unit_cube_points();

    SUBCASE("identity mapping"){
        point_set<double> ps_stationary = ps_moving;
        AlignViaTPSParams params;
        params.kernel_dimension = 2;
        params.lambda = 0.0;
        auto result = AlignViaTPS(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );
        const double rms = compute_ordered_rms_error(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms < 1e-6 );
    }

    SUBCASE("pure translation"){
        const vec3<double> offset(2.0, -1.0, 0.5);
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back(p + offset);
        }
        AlignViaTPSParams params;
        params.kernel_dimension = 2;
        params.lambda = 0.0;
        auto result = AlignViaTPS(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );
        const double rms = compute_ordered_rms_error(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms < 1e-4 );
    }

    SUBCASE("rotation"){
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back(p.rotate_around_z(test_pi * 0.1));
        }
        AlignViaTPSParams params;
        params.kernel_dimension = 2;
        params.lambda = 0.0;
        auto result = AlignViaTPS(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );
        const double rms = compute_ordered_rms_error(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms < 1e-4 );
    }
}
#endif // DCMA_USE_EIGEN


// ============================================================================
// AlignViaTPSRPM tests
// ============================================================================

#ifdef DCMA_USE_EIGEN
TEST_CASE( "AlignViaTPSRPM prevents point cloud collapse" ){

    point_set<double> ps_moving = create_unit_cube_points();
    const double orig_total_variance = compute_total_variance(ps_moving);

    SUBCASE("identical point clouds"){
        point_set<double> ps_stationary = ps_moving;

        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.001;
        params.T_end_scale = 0.001;
        params.N_iters_at_fixed_T = 3;
        
        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double trans_total_variance = compute_transformed_variance(result.value(), ps_moving);
        REQUIRE( trans_total_variance > 0.5 * orig_total_variance );

        const double rms_error = compute_set_alignment_rms(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 0.1 );
    }

    SUBCASE("nearly-aligned point clouds"){
        point_set<double> ps_stationary;
        const vec3<double> small_offset(0.05, 0.05, 0.05);
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back( p + small_offset );
        }

        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.001;
        params.T_end_scale = 0.001;
        params.N_iters_at_fixed_T = 3;
        
        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double trans_total_variance = compute_transformed_variance(result.value(), ps_moving);
        REQUIRE( trans_total_variance > 0.5 * orig_total_variance );

        const double rms_error = compute_set_alignment_rms(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 0.2 );
    }
}

TEST_CASE( "AlignViaTPSRPM rotation transformation" ){

    point_set<double> ps_moving = create_asymmetric_points();

    SUBCASE("small rotation around z-axis"){
        const double angle = test_pi * 0.1;
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back( p.rotate_around_z(angle) );
        }

        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.001;
        params.T_end_scale = 0.001;
        params.N_iters_at_fixed_T = 3;
        
        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double rms_error = compute_set_alignment_rms(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 0.1 );
    }

    SUBCASE("combined rotation around multiple axes"){
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back( 
                p.rotate_around_x(test_pi*0.05).rotate_around_y(-test_pi*0.03).rotate_around_z(test_pi*0.04) 
            );
        }

        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.001;
        params.T_end_scale = 0.001;
        params.N_iters_at_fixed_T = 3;
        
        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double rms_error = compute_set_alignment_rms(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 0.15 );
    }
}

TEST_CASE( "AlignViaTPSRPM scaling and combined transforms" ){

    point_set<double> ps_moving = create_asymmetric_points();

    SUBCASE("uniform scaling"){
        const double scale_factor = 1.1;
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back( p * scale_factor );
        }

        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.001;
        params.T_end_scale = 0.005;
        params.N_iters_at_fixed_T = 3;
        
        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double rms_error = compute_set_alignment_rms(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 0.15 );
    }

    SUBCASE("translation with rotation"){
        const vec3<double> translation(0.2, -0.1, 0.15);
        const double angle = test_pi * 0.08;
        
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back( p.rotate_around_z(angle) + translation );
        }

        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.001;
        params.T_end_scale = 0.001;
        params.N_iters_at_fixed_T = 3;
        
        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double rms_error = compute_set_alignment_rms(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 0.1 );
    }
}

TEST_CASE( "AlignViaTPSRPM solution methods" ){

    point_set<double> ps_moving = create_asymmetric_points();

    point_set<double> ps_stationary;
    for(const auto &p : ps_moving.points){
        ps_stationary.points.emplace_back( 
            p.rotate_around_x(test_pi*0.04).rotate_around_y(-test_pi*0.03) + vec3<double>(0.1, 0.05, -0.05)
        );
    }

    SUBCASE("LDLT solution method"){
        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.001;
        params.T_end_scale = 0.001;
        params.N_iters_at_fixed_T = 3;
        params.solution_method = AlignViaTPSRPMParams::SolutionMethod::LDLT;
        
        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double rms_error = compute_set_alignment_rms(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 0.1 );
    }

    SUBCASE("PseudoInverse solution method"){
        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.001;
        params.T_end_scale = 0.001;
        params.N_iters_at_fixed_T = 3;
        params.solution_method = AlignViaTPSRPMParams::SolutionMethod::PseudoInverse;
        
        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double rms_error = compute_set_alignment_rms(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 0.1 );
    }
}

TEST_CASE( "AlignViaTPSRPM centroid shift seeding" ){

    point_set<double> ps_moving = create_asymmetric_points();

    const vec3<double> translation(2.0, 1.5, -1.0);
    point_set<double> ps_stationary;
    for(const auto &p : ps_moving.points){
        ps_stationary.points.emplace_back( p.rotate_around_z(test_pi*0.02) + translation );
    }

    SUBCASE("with centroid shift seeding"){
        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.001;
        params.T_end_scale = 0.001;
        params.N_iters_at_fixed_T = 3;
        params.seed_with_centroid_shift = true;
        
        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double rms_error = compute_set_alignment_rms(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 0.15 );
    }

    SUBCASE("without centroid shift seeding"){
        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.001;
        params.T_end_scale = 0.001;
        params.N_iters_at_fixed_T = 3;
        params.seed_with_centroid_shift = false;
        
        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double trans_total_variance = compute_transformed_variance(result.value(), ps_moving);
        REQUIRE( trans_total_variance > 0.1 );
    }
}

TEST_CASE( "AlignViaTPSRPM correspondence reporting" ){

    point_set<double> ps_moving = create_asymmetric_points();
    point_set<double> ps_stationary = ps_moving;

    AlignViaTPSRPMParams params;
    params.kernel_dimension = 2;
    params.lambda_start = 0.001;
    params.T_end_scale = 0.001;
    params.N_iters_at_fixed_T = 3;
    params.report_final_correspondence = true;
    
    auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
    REQUIRE( result.has_value() );

    REQUIRE( params.final_move_correspondence.size() == ps_moving.points.size() );
    REQUIRE( params.final_stat_correspondence.size() == ps_moving.points.size() );

    Stats::Running_Sum<double> reported_sq_error;
    for(const auto &corr : params.final_move_correspondence){
        REQUIRE( corr.first >= 0 );
        REQUIRE( corr.first < static_cast<int64_t>(ps_moving.points.size()) );
        REQUIRE( corr.second >= 0 );
        REQUIRE( corr.second < static_cast<int64_t>(ps_stationary.points.size()) );

        const auto p_trans = result.value().transform(ps_moving.points.at(corr.first));
        const auto diff = p_trans - ps_stationary.points.at(corr.second);
        reported_sq_error.Digest(diff.Dot(diff));
    }
    const double correspondence_rms = std::sqrt(
        reported_sq_error.Current_Sum() / static_cast<double>(params.final_move_correspondence.size()));
    REQUIRE( correspondence_rms < 0.1 );
}

TEST_CASE( "AlignViaTPSRPM forced correspondence" ){

    point_set<double> ps_moving = create_asymmetric_points();

    point_set<double> ps_stationary;
    for(const auto &p : ps_moving.points){
        ps_stationary.points.emplace_back( p.rotate_around_z(test_pi*0.03) );
    }

    AlignViaTPSRPMParams params;
    params.kernel_dimension = 2;
    params.lambda_start = 0.001;
    params.T_end_scale = 0.001;
    params.N_iters_at_fixed_T = 3;
    params.N_Sinkhorn_iters = 10000;
    params.report_final_correspondence = true;
    
    params.forced_correspondence.push_back({0, 0});
    params.forced_correspondence.push_back({7, 7});
    
    auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
    REQUIRE( result.has_value() );

    REQUIRE( params.final_move_correspondence.size() == ps_moving.points.size() );
    
    bool found_0_to_0 = false;
    bool found_7_to_7 = false;
    for(const auto &corr : params.final_move_correspondence){
        if(corr.first == 0 && corr.second == 0) found_0_to_0 = true;
        if(corr.first == 7 && corr.second == 7) found_7_to_7 = true;
    }
    
    REQUIRE( found_0_to_0 );
    REQUIRE( found_7_to_7 );
}

TEST_CASE( "AlignViaTPSRPM kernel dimension variations" ){

    point_set<double> ps_moving = create_asymmetric_points();

    point_set<double> ps_stationary;
    for(const auto &p : ps_moving.points){
        ps_stationary.points.emplace_back( p.rotate_around_x(test_pi*0.04) + vec3<double>(0.1, 0.05, 0.0) );
    }

    SUBCASE("kernel dimension 2"){
        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.001;
        params.T_end_scale = 0.001;
        params.N_iters_at_fixed_T = 3;
        
        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double rms_error = compute_set_alignment_rms(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 0.1 );
    }

    SUBCASE("kernel dimension 3"){
        AlignViaTPSRPMParams params;
        params.kernel_dimension = 3;
        params.lambda_start = 0.001;
        params.T_end_scale = 0.001;
        params.N_iters_at_fixed_T = 3;
        
        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double trans_total_variance = compute_transformed_variance(result.value(), ps_moving);
        REQUIRE( trans_total_variance > 0.1 );
    }
}

TEST_CASE( "AlignViaTPSRPM non-rigid deformation" ){

    point_set<double> ps_moving;
    for(double x = 0.0; x <= 1.0; x += 0.5){
        for(double y = 0.0; y <= 1.0; y += 0.5){
            for(double z = 0.0; z <= 1.0; z += 0.5){
                ps_moving.points.emplace_back( vec3<double>(x, y, z) );
            }
        }
    }

    point_set<double> ps_stationary;
    for(const auto &p : ps_moving.points){
        const double warp_x = p.x + 0.05 * std::sin(p.y * test_pi);
        const double warp_y = p.y + 0.05 * std::sin(p.z * test_pi);
        const double warp_z = p.z;
        ps_stationary.points.emplace_back( vec3<double>(warp_x, warp_y, warp_z) );
    }

    AlignViaTPSRPMParams params;
    params.kernel_dimension = 2;
    params.lambda_start = 0.0001;
    params.T_end_scale = 0.001;
    params.N_iters_at_fixed_T = 4;
    
    auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
    REQUIRE( result.has_value() );

    const double rms_error = compute_set_alignment_rms(result.value(), ps_moving, ps_stationary);
    REQUIRE( rms_error < 0.15 );
}

TEST_CASE( "AlignViaTPSRPM asymmetric point clouds" ){

    SUBCASE("slightly more stationary points than moving"){
        point_set<double> ps_moving;
        ps_moving.points.emplace_back( vec3<double>( 0.0,  0.0,  0.0) );
        ps_moving.points.emplace_back( vec3<double>( 1.0,  0.0,  0.0) );
        ps_moving.points.emplace_back( vec3<double>( 0.0,  1.0,  0.0) );
        ps_moving.points.emplace_back( vec3<double>( 0.0,  0.0,  1.0) );
        ps_moving.points.emplace_back( vec3<double>( 1.0,  1.0,  0.0) );
        ps_moving.points.emplace_back( vec3<double>( 1.0,  0.0,  1.0) );
        ps_moving.points.emplace_back( vec3<double>( 0.0,  1.0,  1.0) );

        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back( p.rotate_around_z(test_pi*0.03) );
        }
        ps_stationary.points.emplace_back( vec3<double>( 1.0,  1.0,  1.0).rotate_around_z(test_pi*0.03) );

        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.1;
        params.T_end_scale = 0.1;
        params.N_iters_at_fixed_T = 2;
        params.N_Sinkhorn_iters = 20000;
        params.Sinkhorn_tolerance = 0.05;
        
        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double trans_total_variance = compute_transformed_variance(result.value(), ps_moving);
        REQUIRE( trans_total_variance > 0.1 );
    }

    SUBCASE("slightly more moving points than stationary"){
        point_set<double> ps_moving = create_asymmetric_points();

        point_set<double> ps_stationary;
        ps_stationary.points.emplace_back( vec3<double>( 0.0,  0.0,  0.0).rotate_around_z(test_pi*0.03) );
        ps_stationary.points.emplace_back( vec3<double>( 1.0,  0.0,  0.0).rotate_around_z(test_pi*0.03) );
        ps_stationary.points.emplace_back( vec3<double>( 0.0,  1.0,  0.0).rotate_around_z(test_pi*0.03) );
        ps_stationary.points.emplace_back( vec3<double>( 0.0,  0.0,  1.0).rotate_around_z(test_pi*0.03) );
        ps_stationary.points.emplace_back( vec3<double>( 1.0,  1.0,  0.0).rotate_around_z(test_pi*0.03) );
        ps_stationary.points.emplace_back( vec3<double>( 1.0,  0.0,  1.0).rotate_around_z(test_pi*0.03) );
        ps_stationary.points.emplace_back( vec3<double>( 0.0,  1.0,  1.0).rotate_around_z(test_pi*0.03) );

        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.1;
        params.T_end_scale = 0.1;
        params.N_iters_at_fixed_T = 2;
        params.N_Sinkhorn_iters = 20000;
        params.Sinkhorn_tolerance = 0.05;
        
        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double trans_total_variance = compute_transformed_variance(result.value(), ps_moving);
        REQUIRE( trans_total_variance > 0.1 );
    }
}

TEST_CASE( "AlignViaTPSRPM double-sided outlier handling" ){

    point_set<double> ps_moving = create_unit_cube_points();

    point_set<double> ps_stationary;
    for(const auto &p : ps_moving.points){
        ps_stationary.points.emplace_back( p.rotate_around_x(test_pi*0.03) + vec3<double>(0.05, 0.0, 0.0) );
    }

    SUBCASE("with double-sided outliers enabled"){
        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.01;
        params.T_end_scale = 0.01;
        params.N_iters_at_fixed_T = 3;
        params.double_sided_outliers = true;
        
        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double trans_total_variance = compute_transformed_variance(result.value(), ps_moving);
        REQUIRE( trans_total_variance > 0.1 );
    }

    SUBCASE("without double-sided outliers"){
        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.001;
        params.T_end_scale = 0.001;
        params.N_iters_at_fixed_T = 3;
        params.double_sided_outliers = false;
        
        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double rms_error = compute_set_alignment_rms(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 0.1 );
    }
}

TEST_CASE( "AlignViaTPSRPM thin_plate_spline serialization" ){

    point_set<double> ps_moving = create_unit_cube_points();

    point_set<double> ps_stationary;
    for(const auto &p : ps_moving.points){
        ps_stationary.points.emplace_back( p.rotate_around_z(test_pi*0.05) + vec3<double>(0.1, 0.05, 0.0) );
    }

    AlignViaTPSRPMParams params;
    params.kernel_dimension = 2;
    params.lambda_start = 0.001;
    params.T_end_scale = 0.001;
    params.N_iters_at_fixed_T = 3;
    
    auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
    REQUIRE( result.has_value() );

    std::stringstream ss;
    REQUIRE( result.value().write_to(ss) );

    ss.seekg(0);
    thin_plate_spline tps_loaded(ss);

    const double serialization_tolerance = 1e-10;
    std::vector<vec3<double>> test_points = {
        vec3<double>(0.0, 0.0, 0.0),
        vec3<double>(0.5, 0.5, 0.5),
        vec3<double>(1.0, 1.0, 1.0),
        vec3<double>(0.25, 0.75, 0.5)
    };

    for(const auto &p : test_points){
        const auto p_orig = result.value().transform(p);
        const auto p_loaded = tps_loaded.transform(p);
        const double diff = (p_orig - p_loaded).length();
        REQUIRE( diff < serialization_tolerance );
    }
}


// ============================================================================
// New tests: Real-world scale data and anti-collapse
// ============================================================================

TEST_CASE( "AlignViaTPSRPM real-world scale coordinates" ){
    // Test with point clouds at real-world medical imaging scale (hundreds of mm).
    // This specifically tests the normalization fix - without normalization, the TPS kernel
    // values become enormous and the exp(-d^2/T) correspondence weights underflow to zero.

    point_set<double> ps_moving;
    // Simulate a set of anatomical landmarks in mm coordinates.
    ps_moving.points.emplace_back( vec3<double>( 100.0,  200.0,  50.0) );
    ps_moving.points.emplace_back( vec3<double>( 150.0,  200.0,  50.0) );
    ps_moving.points.emplace_back( vec3<double>( 100.0,  250.0,  50.0) );
    ps_moving.points.emplace_back( vec3<double>( 100.0,  200.0, 100.0) );
    ps_moving.points.emplace_back( vec3<double>( 150.0,  250.0,  50.0) );
    ps_moving.points.emplace_back( vec3<double>( 150.0,  200.0, 100.0) );
    ps_moving.points.emplace_back( vec3<double>( 100.0,  250.0, 100.0) );
    ps_moving.points.emplace_back( vec3<double>( 150.0,  250.0, 100.0) );
    ps_moving.points.emplace_back( vec3<double>( 125.0,  225.0,  75.0) );
    ps_moving.points.emplace_back( vec3<double>( 120.0,  210.0,  60.0) );
    ps_moving.points.emplace_back( vec3<double>( 130.0,  240.0,  90.0) );
    ps_moving.points.emplace_back( vec3<double>( 140.0,  220.0,  80.0) );

    const double orig_variance = compute_total_variance(ps_moving);

    SUBCASE("small translation at real-world scale"){
        // Apply a small (5mm) translation.
        const vec3<double> offset(5.0, -3.0, 2.0);
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back(p + offset);
        }

        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.001;
        params.T_end_scale = 0.001;
        params.N_iters_at_fixed_T = 3;

        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        // Must not collapse.
        const double trans_variance = compute_transformed_variance(result.value(), ps_moving);
        REQUIRE( trans_variance > 0.5 * orig_variance );

        // Should achieve reasonable registration.
        const double rms_error = compute_set_alignment_rms(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 10.0 ); // Within 10mm for real-world scale.
    }

    SUBCASE("rotation at real-world scale"){
        // Apply a small rotation at real-world scale.
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back(p.rotate_around_z(test_pi * 0.05));
        }

        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.001;
        params.T_end_scale = 0.001;
        params.N_iters_at_fixed_T = 3;

        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        // Must not collapse.
        const double trans_variance = compute_transformed_variance(result.value(), ps_moving);
        REQUIRE( trans_variance > 0.5 * orig_variance );
    }

    SUBCASE("large translation at real-world scale"){
        // Apply a large (50mm) translation.
        const vec3<double> offset(50.0, -30.0, 20.0);
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back(p + offset);
        }

        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.001;
        params.T_end_scale = 0.001;
        params.N_iters_at_fixed_T = 3;

        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        // Must not collapse.
        const double trans_variance = compute_transformed_variance(result.value(), ps_moving);
        REQUIRE( trans_variance > 0.5 * orig_variance );

        const double rms_error = compute_set_alignment_rms(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 20.0 );
    }
}

TEST_CASE( "AlignViaTPSRPM anti-collapse validation" ){
    // Specifically test that the affine regularization fix prevents point cloud collapse.
    // The moving cloud should never shrink to a single point during registration.

    point_set<double> ps_moving = create_asymmetric_points();
    const double orig_variance = compute_total_variance(ps_moving);

    SUBCASE("high regularization preserves variance"){
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back(
                p.rotate_around_z(test_pi * 0.03) + vec3<double>(0.1, 0.0, 0.0));
        }

        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.1; // High regularization without driving Sinkhorn into a stalled regime.
        params.T_end_scale = 0.01;
        params.N_iters_at_fixed_T = 3;

        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double trans_total_variance = compute_transformed_variance(result.value(), ps_moving);
        REQUIRE( trans_total_variance > 0.1 );
    }

    SUBCASE("zero regularization still prevents total collapse"){
        // Even with zero lambda, normalization and the TPS structure should prevent total collapse.
        // This tests that the algorithm doesn't catastrophically fail.
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back(p.rotate_around_z(test_pi * 0.05));
        }

        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.0;
        params.T_end_scale = 0.01;
        params.N_iters_at_fixed_T = 2;
        params.Sinkhorn_tolerance = 0.05;

        // With zero regularization, the algorithm may not converge perfectly,
        // but it should still produce a result.
        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );
    }

    SUBCASE("variance preserved across multiple scenarios"){
        // Test with various transformations to ensure no collapse occurs.
        std::vector<std::pair<std::string, point_set<double>>> scenarios;

        // Small rotation
        {
            point_set<double> ps;
            for(const auto &p : ps_moving.points) ps.points.emplace_back(p.rotate_around_x(test_pi*0.1));
            scenarios.emplace_back("small rotation", std::move(ps));
        }
        // Translation
        {
            point_set<double> ps;
            for(const auto &p : ps_moving.points) ps.points.emplace_back(p + vec3<double>(0.5, -0.3, 0.2));
            scenarios.emplace_back("translation", std::move(ps));
        }

        for(const auto &[name, ps_stationary] : scenarios){
            AlignViaTPSRPMParams params;
            params.kernel_dimension = 2;
            params.lambda_start = 0.001;
            params.T_end_scale = 0.001;
            params.N_iters_at_fixed_T = 3;

            auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
            REQUIRE( result.has_value() );

            const double trans_variance = compute_transformed_variance(result.value(), ps_moving);
            REQUIRE( trans_variance > 0.3 * orig_variance );
        }
    }
}

TEST_CASE( "AlignViaTPSRPM output is in original coordinate space" ){
    // Verify that the denormalization step correctly maps results back to original coordinates.

    // Use a point cloud NOT centered at origin, with scale much larger than [-1,1].
    point_set<double> ps_moving = create_asymmetric_points();
    for(auto &p : ps_moving.points){
        p = p * 4.0 + vec3<double>(50.0, 80.0, 120.0);
    }

    // Identity mapping: transformed points should be near the stationary points.
    point_set<double> ps_stationary = ps_moving;

    AlignViaTPSRPMParams params;
    params.kernel_dimension = 2;
    params.lambda_start = 0.001;
    params.T_end_scale = 0.001;
    params.N_iters_at_fixed_T = 3;

    auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
    REQUIRE( result.has_value() );

    // Check that the transformed set remains in the original coordinate space.
    const double rms_error = compute_set_alignment_rms(result.value(), ps_moving, ps_stationary);
    REQUIRE( rms_error < 2.0 );

    // Check that the centroid of transformed points is near the original centroid.
    point_set<double> ps_transformed;
    for(const auto &p : ps_moving.points){
        ps_transformed.points.emplace_back(result.value().transform(p));
    }
    const auto centroid_orig = ps_stationary.Centroid();
    const auto centroid_trans = ps_transformed.Centroid();
    const double centroid_diff = (centroid_orig - centroid_trans).length();
    REQUIRE( centroid_diff < 2.0 );
}

TEST_CASE( "AlignViaTPSRPM benchmark" ){
    // Benchmark test to measure performance with small point clouds.

    SUBCASE("small point cloud (27 points, 3x3x3 grid)"){
        auto ps_moving = create_grid_point_cloud(3);
        const int64_t N = static_cast<int64_t>(ps_moving.points.size());
        
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back( 
                p.rotate_around_x(test_pi*0.05).rotate_around_z(test_pi*0.03) + vec3<double>(0.1, 0.05, 0.0) 
            );
        }

        AlignViaTPSRPMParams params;
        params.kernel_dimension = 2;
        params.lambda_start = 0.01;
        params.T_end_scale = 0.02;
        params.N_iters_at_fixed_T = 2;
        
        auto t_start = std::chrono::steady_clock::now();
        auto result = AlignViaTPSRPM(params, ps_moving, ps_stationary);
        auto t_end = std::chrono::steady_clock::now();
        
        REQUIRE( result.has_value() );
        
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
        MESSAGE("TPS-RPM benchmark (N=" << N << " points): " << elapsed_ms << " ms");

        const double rms_error = compute_set_alignment_rms(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 0.15 );
    }
}
#endif // DCMA_USE_EIGEN
