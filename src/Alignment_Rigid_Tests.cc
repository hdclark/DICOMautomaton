//Alignment_Rigid_Tests.cc - A part of DICOMautomaton 2020. Written by hal clark.
//
// This file contains unit tests for the rigid alignment methods defined in Alignment_Rigid.cc.
// These tests are separated into their own file because Alignment_Rigid_obj is linked into
// shared libraries which don't include doctest implementation.

#include <algorithm>
#include <chrono>
#include <optional>
#include <vector>
#include <cstdint>

#include "doctest20251212/doctest.h"

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorStats.h"        //Needed for Stats:: namespace.

#include "Alignment_Rigid.h"


// ============================================================================
// Helper functions for unit tests
// ============================================================================

static constexpr double test_pi = 3.14159265358979;

// Creates a unit cube with 8 corner points
static point_set<double> create_unit_cube_points(){
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
static point_set<double> create_extended_cube_points(){
    point_set<double> ps = create_unit_cube_points();
    ps.points.emplace_back( vec3<double>( 0.5,  0.5,  0.0) );
    ps.points.emplace_back( vec3<double>( 0.5,  0.0,  0.5) );
    ps.points.emplace_back( vec3<double>( 0.0,  0.5,  0.5) );
    ps.points.emplace_back( vec3<double>( 0.5,  0.5,  0.5) );
    return ps;
}

// Creates an asymmetric point cloud suitable for PCA and ICP algorithms.
// The asymmetric point breaks the symmetry of the cube to ensure PCA can
// determine unique principal components.
static point_set<double> create_asymmetric_cube_points(){
    point_set<double> ps = create_extended_cube_points();
    // Add asymmetric points to break symmetry
    ps.points.emplace_back( vec3<double>( 0.7,  0.3,  0.2) );
    ps.points.emplace_back( vec3<double>( 0.2,  0.8,  0.4) );
    return ps;
}

// Computes RMS error between transformed moving points and stationary points using affine_transform
static double compute_rms_error_affine(const affine_transform<double>& t,
                                       const point_set<double>& moving,
                                       const point_set<double>& stationary){
    Stats::Running_Sum<double> sq_error;
    for(size_t i = 0; i < moving.points.size(); ++i){
        auto p_transformed = moving.points[i];
        t.apply_to(p_transformed);
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

// Computes total variance of transformed points using affine_transform
static double compute_transformed_variance_affine(const affine_transform<double>& t,
                                                   const point_set<double>& ps){
    Stats::Running_Variance<double> var_x, var_y, var_z;
    for(const auto &p : ps.points){
        auto p_trans = p;
        t.apply_to(p_trans);
        var_x.Digest(p_trans.x);
        var_y.Digest(p_trans.y);
        var_z.Digest(p_trans.z);
    }
    return var_x.Current_Variance() + var_y.Current_Variance() + var_z.Current_Variance();
}

// ============================================================================
// Test cases for AlignViaCentroid
// ============================================================================

TEST_CASE( "AlignViaCentroid identical point clouds" ){
    // Test with identical point clouds - should result in identity transform (no translation)
    point_set<double> ps_moving = create_unit_cube_points();
    point_set<double> ps_stationary = ps_moving;

    auto result = AlignViaCentroid(ps_moving, ps_stationary);
    REQUIRE( result.has_value() );

    // The transformation should be essentially identity (no translation needed)
    const double rms_error = compute_rms_error_affine(result.value(), ps_moving, ps_stationary);
    REQUIRE( rms_error < 1e-10 );
}

TEST_CASE( "AlignViaCentroid translation transformation" ){
    point_set<double> ps_moving = create_unit_cube_points();

    // Apply a translation
    const vec3<double> translation(2.0, -1.5, 3.0);
    point_set<double> ps_stationary;
    for(const auto &p : ps_moving.points){
        ps_stationary.points.emplace_back( p + translation );
    }

    auto result = AlignViaCentroid(ps_moving, ps_stationary);
    REQUIRE( result.has_value() );

    // Centroid alignment should perfectly recover pure translation
    const double rms_error = compute_rms_error_affine(result.value(), ps_moving, ps_stationary);
    REQUIRE( rms_error < 1e-10 );

    // Variance should be preserved
    const double trans_variance = compute_transformed_variance_affine(result.value(), ps_moving);
    const double orig_variance = compute_total_variance(ps_moving);
    REQUIRE( std::abs(trans_variance - orig_variance) < 1e-10 );
}

TEST_CASE( "AlignViaCentroid asymmetric point clouds" ){
    // Test with different numbers of points in moving and stationary clouds
    point_set<double> ps_moving = create_unit_cube_points();
    point_set<double> ps_stationary = create_extended_cube_points();

    // Apply translation to stationary points
    const vec3<double> translation(1.0, 1.0, 1.0);
    for(auto &p : ps_stationary.points){
        p = p + translation;
    }

    auto result = AlignViaCentroid(ps_moving, ps_stationary);
    REQUIRE( result.has_value() );

    // Variance should be preserved
    const double trans_variance = compute_transformed_variance_affine(result.value(), ps_moving);
    const double orig_variance = compute_total_variance(ps_moving);
    REQUIRE( std::abs(trans_variance - orig_variance) < 1e-10 );
}

TEST_CASE( "AlignViaCentroid error handling" ){
    point_set<double> ps_empty;
    point_set<double> ps_valid = create_unit_cube_points();

    SUBCASE("empty moving point set"){
        REQUIRE_THROWS( AlignViaCentroid(ps_empty, ps_valid) );
    }

    SUBCASE("empty stationary point set"){
        REQUIRE_THROWS( AlignViaCentroid(ps_valid, ps_empty) );
    }
}

// ============================================================================
// Test cases for AlignViaPCA
// ============================================================================

#ifdef DCMA_USE_EIGEN
TEST_CASE( "AlignViaPCA identical point clouds" ){
    point_set<double> ps_moving = create_extended_cube_points();
    point_set<double> ps_stationary = ps_moving;

    auto result = AlignViaPCA(ps_moving, ps_stationary);
    REQUIRE( result.has_value() );

    // Transformation should be nearly identity
    const double rms_error = compute_rms_error_affine(result.value(), ps_moving, ps_stationary);
    REQUIRE( rms_error < 0.1 );
}

TEST_CASE( "AlignViaPCA rotation transformation" ){
    point_set<double> ps_moving = create_extended_cube_points();
    const double orig_variance = compute_total_variance(ps_moving);

    SUBCASE("small rotation around z-axis"){
        const double angle = test_pi * 0.1; // 18 degrees
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back( p.rotate_around_z(angle) );
        }

        auto result = AlignViaPCA(ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        // PCA alignment may not perfectly recover rotations for symmetric point clouds,
        // but it should preserve variance (no collapse)
        const double trans_variance = compute_transformed_variance_affine(result.value(), ps_moving);
        REQUIRE( trans_variance > 0.5 * orig_variance );
    }

    SUBCASE("combined rotation around multiple axes"){
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back( 
                p.rotate_around_x(test_pi*0.05).rotate_around_y(-test_pi*0.03).rotate_around_z(test_pi*0.04) 
            );
        }

        auto result = AlignViaPCA(ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        // PCA alignment may not perfectly recover rotations for symmetric point clouds,
        // but it should preserve variance (no collapse)
        const double trans_variance = compute_transformed_variance_affine(result.value(), ps_moving);
        REQUIRE( trans_variance > 0.5 * orig_variance );
    }
}

TEST_CASE( "AlignViaPCA translation with rotation" ){
    point_set<double> ps_moving = create_extended_cube_points();
    const double orig_variance = compute_total_variance(ps_moving);
    
    const vec3<double> translation(0.5, -0.3, 0.2);
    const double angle = test_pi * 0.08;
    
    point_set<double> ps_stationary;
    for(const auto &p : ps_moving.points){
        ps_stationary.points.emplace_back( p.rotate_around_z(angle) + translation );
    }

    auto result = AlignViaPCA(ps_moving, ps_stationary);
    REQUIRE( result.has_value() );

    // PCA alignment should at least preserve variance (no collapse)
    const double trans_variance = compute_transformed_variance_affine(result.value(), ps_moving);
    REQUIRE( trans_variance > 0.5 * orig_variance );
}

TEST_CASE( "AlignViaPCA variance preservation" ){
    point_set<double> ps_moving = create_extended_cube_points();
    const double orig_variance = compute_total_variance(ps_moving);

    // Apply a transformation
    point_set<double> ps_stationary;
    for(const auto &p : ps_moving.points){
        ps_stationary.points.emplace_back( p.rotate_around_x(test_pi*0.06) + vec3<double>(0.1, 0.0, 0.05) );
    }

    auto result = AlignViaPCA(ps_moving, ps_stationary);
    REQUIRE( result.has_value() );

    // Variance should be preserved (no collapse)
    const double trans_variance = compute_transformed_variance_affine(result.value(), ps_moving);
    REQUIRE( trans_variance > 0.5 * orig_variance );
}

TEST_CASE( "AlignViaPCA error handling" ){
    point_set<double> ps_empty;
    point_set<double> ps_valid = create_unit_cube_points();

    SUBCASE("empty moving point set"){
        REQUIRE_THROWS( AlignViaPCA(ps_empty, ps_valid) );
    }

    SUBCASE("empty stationary point set"){
        REQUIRE_THROWS( AlignViaPCA(ps_valid, ps_empty) );
    }
}
#endif // DCMA_USE_EIGEN

// ============================================================================
// Test cases for AlignViaOrthogonalProcrustes
// ============================================================================

#ifdef DCMA_USE_EIGEN
TEST_CASE( "AlignViaOrthogonalProcrustes identical point clouds" ){
    point_set<double> ps_moving = create_unit_cube_points();
    point_set<double> ps_stationary = ps_moving;

    AlignViaOrthogonalProcrustesParams params;
    params.permit_mirroring = false;
    params.permit_isotropic_scaling = false;

    auto result = AlignViaOrthogonalProcrustes(params, ps_moving, ps_stationary);
    REQUIRE( result.has_value() );

    // Transformation should be essentially identity
    const double rms_error = compute_rms_error_affine(result.value(), ps_moving, ps_stationary);
    REQUIRE( rms_error < 1e-10 );
}

TEST_CASE( "AlignViaOrthogonalProcrustes rotation transformation" ){
    point_set<double> ps_moving = create_unit_cube_points();

    SUBCASE("small rotation around z-axis"){
        const double angle = test_pi * 0.1;
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back( p.rotate_around_z(angle) );
        }

        AlignViaOrthogonalProcrustesParams params;
        params.permit_mirroring = false;

        auto result = AlignViaOrthogonalProcrustes(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double rms_error = compute_rms_error_affine(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 1e-10 );
    }

    SUBCASE("combined rotation around multiple axes"){
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back( 
                p.rotate_around_x(test_pi*0.05).rotate_around_y(-test_pi*0.03).rotate_around_z(test_pi*0.04) 
            );
        }

        AlignViaOrthogonalProcrustesParams params;
        params.permit_mirroring = false;

        auto result = AlignViaOrthogonalProcrustes(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        const double rms_error = compute_rms_error_affine(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 1e-10 );
    }
}

TEST_CASE( "AlignViaOrthogonalProcrustes translation with rotation" ){
    point_set<double> ps_moving = create_unit_cube_points();
    
    const vec3<double> translation(2.0, -1.0, 0.5);
    const double angle = test_pi * 0.12;
    
    point_set<double> ps_stationary;
    for(const auto &p : ps_moving.points){
        ps_stationary.points.emplace_back( p.rotate_around_y(angle) + translation );
    }

    AlignViaOrthogonalProcrustesParams params;
    params.permit_mirroring = false;

    auto result = AlignViaOrthogonalProcrustes(params, ps_moving, ps_stationary);
    REQUIRE( result.has_value() );

    const double rms_error = compute_rms_error_affine(result.value(), ps_moving, ps_stationary);
    REQUIRE( rms_error < 1e-10 );
}

TEST_CASE( "AlignViaOrthogonalProcrustes with isotropic scaling" ){
    point_set<double> ps_moving = create_unit_cube_points();

    // Apply scaling
    const double scale_factor = 1.5;
    point_set<double> ps_stationary;
    for(const auto &p : ps_moving.points){
        ps_stationary.points.emplace_back( p * scale_factor );
    }

    SUBCASE("without scaling permission"){
        AlignViaOrthogonalProcrustesParams params;
        params.permit_mirroring = false;
        params.permit_isotropic_scaling = false;

        auto result = AlignViaOrthogonalProcrustes(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        // Without scaling, the RMS error will not be zero
        const double rms_error = compute_rms_error_affine(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error > 0.1 ); // Expect significant error without scaling
    }

    SUBCASE("with scaling permission"){
        AlignViaOrthogonalProcrustesParams params;
        params.permit_mirroring = false;
        params.permit_isotropic_scaling = true;

        auto result = AlignViaOrthogonalProcrustes(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        // With scaling permitted, error should be small
        const double rms_error = compute_rms_error_affine(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 1e-10 );
    }
}

TEST_CASE( "AlignViaOrthogonalProcrustes with mirroring" ){
    point_set<double> ps_moving = create_unit_cube_points();

    // Apply a mirror transformation (flip x axis)
    point_set<double> ps_stationary;
    for(const auto &p : ps_moving.points){
        ps_stationary.points.emplace_back( vec3<double>(-p.x, p.y, p.z) );
    }

    SUBCASE("without mirroring permission"){
        AlignViaOrthogonalProcrustesParams params;
        params.permit_mirroring = false;
        params.permit_isotropic_scaling = false;

        auto result = AlignViaOrthogonalProcrustes(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        // Without mirroring, the RMS error will not be zero
        const double rms_error = compute_rms_error_affine(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error > 0.1 );
    }

    SUBCASE("with mirroring permission"){
        AlignViaOrthogonalProcrustesParams params;
        params.permit_mirroring = true;
        params.permit_isotropic_scaling = false;

        auto result = AlignViaOrthogonalProcrustes(params, ps_moving, ps_stationary);
        REQUIRE( result.has_value() );

        // With mirroring permitted, error should be small
        const double rms_error = compute_rms_error_affine(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 1e-10 );
    }
}

TEST_CASE( "AlignViaOrthogonalProcrustes variance preservation" ){
    point_set<double> ps_moving = create_unit_cube_points();
    const double orig_variance = compute_total_variance(ps_moving);

    // Apply a rotation
    point_set<double> ps_stationary;
    for(const auto &p : ps_moving.points){
        ps_stationary.points.emplace_back( p.rotate_around_x(test_pi*0.08) );
    }

    AlignViaOrthogonalProcrustesParams params;
    params.permit_mirroring = false;

    auto result = AlignViaOrthogonalProcrustes(params, ps_moving, ps_stationary);
    REQUIRE( result.has_value() );

    // Variance should be preserved (no collapse)
    const double trans_variance = compute_transformed_variance_affine(result.value(), ps_moving);
    REQUIRE( std::abs(trans_variance - orig_variance) < 0.01 );
}

TEST_CASE( "AlignViaOrthogonalProcrustes error handling" ){
    point_set<double> ps_empty;
    point_set<double> ps_valid = create_unit_cube_points();
    point_set<double> ps_different_size = create_extended_cube_points();

    AlignViaOrthogonalProcrustesParams params;

    SUBCASE("empty moving point set"){
        REQUIRE_THROWS( AlignViaOrthogonalProcrustes(params, ps_empty, ps_valid) );
    }

    SUBCASE("empty stationary point set"){
        REQUIRE_THROWS( AlignViaOrthogonalProcrustes(params, ps_valid, ps_empty) );
    }

    SUBCASE("different sized point sets"){
        REQUIRE_THROWS( AlignViaOrthogonalProcrustes(params, ps_valid, ps_different_size) );
    }
}
#endif // DCMA_USE_EIGEN

// ============================================================================
// Test cases for AlignViaExhaustiveICP
// ============================================================================

#ifdef DCMA_USE_EIGEN
TEST_CASE( "AlignViaExhaustiveICP identical point clouds" ){
    // Use asymmetric point cloud to avoid PCA failures
    point_set<double> ps_moving = create_asymmetric_cube_points();
    point_set<double> ps_stationary = ps_moving;

    auto result = AlignViaExhaustiveICP(ps_moving, ps_stationary, 10, 1e-6);
    REQUIRE( result.has_value() );

    // Transformation should be essentially identity
    const double rms_error = compute_rms_error_affine(result.value(), ps_moving, ps_stationary);
    REQUIRE( rms_error < 0.1 );
}

TEST_CASE( "AlignViaExhaustiveICP rotation transformation" ){
    // Use asymmetric point cloud to avoid PCA failures
    point_set<double> ps_moving = create_asymmetric_cube_points();

    SUBCASE("small rotation around z-axis"){
        const double angle = test_pi * 0.1;
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back( p.rotate_around_z(angle) );
        }

        auto result = AlignViaExhaustiveICP(ps_moving, ps_stationary, 20, 1e-6);
        REQUIRE( result.has_value() );

        const double rms_error = compute_rms_error_affine(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 0.15 );
    }

    SUBCASE("combined rotation around multiple axes"){
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back( 
                p.rotate_around_x(test_pi*0.04).rotate_around_y(-test_pi*0.03).rotate_around_z(test_pi*0.03) 
            );
        }

        auto result = AlignViaExhaustiveICP(ps_moving, ps_stationary, 20, 1e-6);
        REQUIRE( result.has_value() );

        const double rms_error = compute_rms_error_affine(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 0.15 );
    }
}

TEST_CASE( "AlignViaExhaustiveICP translation with rotation" ){
    // Use asymmetric point cloud to avoid PCA failures
    point_set<double> ps_moving = create_asymmetric_cube_points();
    
    const vec3<double> translation(0.3, -0.2, 0.1);
    const double angle = test_pi * 0.06;
    
    point_set<double> ps_stationary;
    for(const auto &p : ps_moving.points){
        ps_stationary.points.emplace_back( p.rotate_around_z(angle) + translation );
    }

    auto result = AlignViaExhaustiveICP(ps_moving, ps_stationary, 25, 1e-6);
    REQUIRE( result.has_value() );

    const double rms_error = compute_rms_error_affine(result.value(), ps_moving, ps_stationary);
    REQUIRE( rms_error < 0.15 );
}

TEST_CASE( "AlignViaExhaustiveICP variance preservation" ){
    // Use asymmetric point cloud to avoid PCA failures
    point_set<double> ps_moving = create_asymmetric_cube_points();
    const double orig_variance = compute_total_variance(ps_moving);

    // Apply a transformation
    point_set<double> ps_stationary;
    for(const auto &p : ps_moving.points){
        ps_stationary.points.emplace_back( p.rotate_around_y(test_pi*0.05) + vec3<double>(0.2, 0.0, 0.0) );
    }

    auto result = AlignViaExhaustiveICP(ps_moving, ps_stationary, 20, 1e-6);
    REQUIRE( result.has_value() );

    // Variance should be preserved (no collapse)
    const double trans_variance = compute_transformed_variance_affine(result.value(), ps_moving);
    REQUIRE( trans_variance > 0.5 * orig_variance );
}

TEST_CASE( "AlignViaExhaustiveICP convergence behavior" ){
    // Use asymmetric point cloud to avoid PCA failures
    point_set<double> ps_moving = create_asymmetric_cube_points();
    
    // Apply a small transformation
    point_set<double> ps_stationary;
    for(const auto &p : ps_moving.points){
        ps_stationary.points.emplace_back( p.rotate_around_z(test_pi*0.03) + vec3<double>(0.1, 0.05, 0.0) );
    }

    SUBCASE("with tight tolerance"){
        auto result = AlignViaExhaustiveICP(ps_moving, ps_stationary, 50, 1e-8);
        REQUIRE( result.has_value() );

        const double rms_error = compute_rms_error_affine(result.value(), ps_moving, ps_stationary);
        REQUIRE( rms_error < 0.1 );
    }

    SUBCASE("with few iterations"){
        // Should still produce a valid result even with few iterations
        auto result = AlignViaExhaustiveICP(ps_moving, ps_stationary, 3, std::numeric_limits<double>::quiet_NaN());
        REQUIRE( result.has_value() );

        // Variance should be preserved
        const double trans_variance = compute_transformed_variance_affine(result.value(), ps_moving);
        const double orig_variance = compute_total_variance(ps_moving);
        REQUIRE( trans_variance > 0.3 * orig_variance );
    }
}

TEST_CASE( "AlignViaExhaustiveICP asymmetric point clouds" ){
    // Test with different numbers of points using non-symmetric point clouds
    // Note: We avoid fully symmetric grids because ICP internally primes with PCA,
    // which may fail for symmetric point clouds.
    point_set<double> ps_moving;
    ps_moving.points.emplace_back( vec3<double>( 0.0,  0.0,  0.0) );
    ps_moving.points.emplace_back( vec3<double>( 1.0,  0.0,  0.0) );
    ps_moving.points.emplace_back( vec3<double>( 0.0,  1.0,  0.0) );
    ps_moving.points.emplace_back( vec3<double>( 0.0,  0.0,  1.0) );
    ps_moving.points.emplace_back( vec3<double>( 1.0,  1.0,  0.0) );
    ps_moving.points.emplace_back( vec3<double>( 1.0,  0.0,  1.0) );
    ps_moving.points.emplace_back( vec3<double>( 0.0,  1.0,  1.0) );
    ps_moving.points.emplace_back( vec3<double>( 1.0,  1.0,  1.0) );
    // Add an asymmetric point to break symmetry
    ps_moving.points.emplace_back( vec3<double>( 0.7,  0.3,  0.5) );

    point_set<double> ps_stationary;
    for(const auto &p : ps_moving.points){
        ps_stationary.points.emplace_back( p.rotate_around_z(test_pi*0.04) );
    }
    // Add extra points to make it asymmetric in count
    ps_stationary.points.emplace_back( vec3<double>( 0.5,  0.5,  0.0).rotate_around_z(test_pi*0.04) );
    ps_stationary.points.emplace_back( vec3<double>( 0.5,  0.0,  0.5).rotate_around_z(test_pi*0.04) );

    auto result = AlignViaExhaustiveICP(ps_moving, ps_stationary, 15, 1e-6);
    REQUIRE( result.has_value() );

    // Variance should be preserved
    const double trans_variance = compute_transformed_variance_affine(result.value(), ps_moving);
    const double orig_variance = compute_total_variance(ps_moving);
    REQUIRE( trans_variance > 0.3 * orig_variance );
}
#endif // DCMA_USE_EIGEN

// ============================================================================
// Benchmark tests
// ============================================================================

TEST_CASE( "Rigid alignment benchmark" ){
    // Benchmark test to measure performance with various point cloud sizes.
    // Uses MESSAGE statements to report timing results.

    // Create a grid-based point cloud with N^3 points plus an asymmetric point
    // The asymmetric point helps avoid degenerate cases for PCA
    auto create_grid_point_cloud = [](int64_t N_per_axis) -> point_set<double> {
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
        // Add asymmetric point to help PCA convergence
        ps.points.emplace_back( vec3<double>(0.7, 0.3, 0.5) );
        return ps;
    };

    SUBCASE("Centroid alignment (28 points)"){
        auto ps_moving = create_grid_point_cloud(3);
        const int64_t N = static_cast<int64_t>(ps_moving.points.size());
        
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back( p + vec3<double>(0.5, 0.3, -0.2) );
        }

        auto t_start = std::chrono::steady_clock::now();
        auto result = AlignViaCentroid(ps_moving, ps_stationary);
        auto t_end = std::chrono::steady_clock::now();
        
        REQUIRE( result.has_value() );
        
        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count();
        MESSAGE("Centroid alignment (N=" << N << " points): " << elapsed_us << " us");
    }

#ifdef DCMA_USE_EIGEN
    SUBCASE("PCA alignment (28 points)"){
        auto ps_moving = create_grid_point_cloud(3);
        const int64_t N = static_cast<int64_t>(ps_moving.points.size());
        
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back( 
                p.rotate_around_x(test_pi*0.05).rotate_around_z(test_pi*0.03) + vec3<double>(0.1, 0.05, 0.0) 
            );
        }

        auto t_start = std::chrono::steady_clock::now();
        auto result = AlignViaPCA(ps_moving, ps_stationary);
        auto t_end = std::chrono::steady_clock::now();
        
        REQUIRE( result.has_value() );
        
        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count();
        MESSAGE("PCA alignment (N=" << N << " points): " << elapsed_us << " us");
    }

    SUBCASE("Orthogonal Procrustes alignment (28 points)"){
        auto ps_moving = create_grid_point_cloud(3);
        const int64_t N = static_cast<int64_t>(ps_moving.points.size());
        
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back( 
                p.rotate_around_x(test_pi*0.05).rotate_around_z(test_pi*0.03) + vec3<double>(0.1, 0.05, 0.0) 
            );
        }

        AlignViaOrthogonalProcrustesParams params;
        params.permit_mirroring = false;

        auto t_start = std::chrono::steady_clock::now();
        auto result = AlignViaOrthogonalProcrustes(params, ps_moving, ps_stationary);
        auto t_end = std::chrono::steady_clock::now();
        
        REQUIRE( result.has_value() );
        
        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count();
        MESSAGE("Orthogonal Procrustes alignment (N=" << N << " points): " << elapsed_us << " us");
    }

    SUBCASE("Exhaustive ICP alignment (28 points, 10 iterations)"){
        auto ps_moving = create_grid_point_cloud(3);
        const int64_t N = static_cast<int64_t>(ps_moving.points.size());
        
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back( 
                p.rotate_around_x(test_pi*0.05).rotate_around_z(test_pi*0.03) + vec3<double>(0.1, 0.05, 0.0) 
            );
        }

        auto t_start = std::chrono::steady_clock::now();
        auto result = AlignViaExhaustiveICP(ps_moving, ps_stationary, 10, 1e-6);
        auto t_end = std::chrono::steady_clock::now();
        
        REQUIRE( result.has_value() );
        
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
        MESSAGE("Exhaustive ICP alignment (N=" << N << " points, 10 iters): " << elapsed_ms << " ms");
    }

    SUBCASE("Exhaustive ICP alignment (65 points, 10 iterations)"){
        auto ps_moving = create_grid_point_cloud(4);
        const int64_t N = static_cast<int64_t>(ps_moving.points.size());
        
        point_set<double> ps_stationary;
        for(const auto &p : ps_moving.points){
            ps_stationary.points.emplace_back( 
                p.rotate_around_x(test_pi*0.05).rotate_around_z(test_pi*0.03) + vec3<double>(0.1, 0.05, 0.0) 
            );
        }

        auto t_start = std::chrono::steady_clock::now();
        auto result = AlignViaExhaustiveICP(ps_moving, ps_stationary, 10, 1e-6);
        auto t_end = std::chrono::steady_clock::now();
        
        REQUIRE( result.has_value() );
        
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
        MESSAGE("Exhaustive ICP alignment (N=" << N << " points, 10 iters): " << elapsed_ms << " ms");
    }
#endif // DCMA_USE_EIGEN
}

