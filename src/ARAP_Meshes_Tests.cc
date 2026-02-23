//ARAP_Meshes_Tests.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains unit tests for the ARAP (As-Rigid-As-Possible) surface modeling implementation.
// These tests are separated into their own file because ARAP_Meshes_obj is linked into
// shared libraries which don't include doctest implementation.

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "doctest20251212/doctest.h"

#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"

#include "ARAP_Meshes.h"


// Helper function to create a simple triangular mesh (tetrahedron).
static fv_surface_mesh<double, uint64_t> make_tetrahedron_mesh() {
    fv_surface_mesh<double, uint64_t> mesh;

    // Vertices of a regular tetrahedron centered at origin.
    mesh.vertices = {
        vec3<double>( 1.0,  1.0,  1.0),  // 0
        vec3<double>( 1.0, -1.0, -1.0),  // 1
        vec3<double>(-1.0,  1.0, -1.0),  // 2
        vec3<double>(-1.0, -1.0,  1.0)   // 3
    };

    // Four triangular faces.
    mesh.faces = {
        {0, 1, 2},
        {0, 3, 1},
        {0, 2, 3},
        {1, 3, 2}
    };

    return mesh;
}

// Helper function to create a simple cube mesh.
static fv_surface_mesh<double, uint64_t> make_cube_mesh() {
    fv_surface_mesh<double, uint64_t> mesh;

    // Vertices of a unit cube centered at origin.
    mesh.vertices = {
        vec3<double>(-0.5, -0.5, -0.5),  // 0
        vec3<double>( 0.5, -0.5, -0.5),  // 1
        vec3<double>( 0.5,  0.5, -0.5),  // 2
        vec3<double>(-0.5,  0.5, -0.5),  // 3
        vec3<double>(-0.5, -0.5,  0.5),  // 4
        vec3<double>( 0.5, -0.5,  0.5),  // 5
        vec3<double>( 0.5,  0.5,  0.5),  // 6
        vec3<double>(-0.5,  0.5,  0.5)   // 7
    };

    // Six faces (each face is two triangles).
    mesh.faces = {
        // Bottom face (-Z)
        {0, 2, 1}, {0, 3, 2},
        // Top face (+Z)
        {4, 5, 6}, {4, 6, 7},
        // Front face (-Y)
        {0, 1, 5}, {0, 5, 4},
        // Back face (+Y)
        {2, 3, 7}, {2, 7, 6},
        // Left face (-X)
        {0, 4, 7}, {0, 7, 3},
        // Right face (+X)
        {1, 2, 6}, {1, 6, 5}
    };

    return mesh;
}

// Helper function to create a simple plane mesh (grid).
static fv_surface_mesh<double, uint64_t> make_plane_mesh(int rows, int cols, double spacing) {
    fv_surface_mesh<double, uint64_t> mesh;

    // Create vertices in a grid.
    for(int r = 0; r < rows; ++r){
        for(int c = 0; c < cols; ++c){
            mesh.vertices.emplace_back(
                static_cast<double>(c) * spacing,
                static_cast<double>(r) * spacing,
                0.0
            );
        }
    }

    // Create triangular faces.
    for(int r = 0; r < rows - 1; ++r){
        for(int c = 0; c < cols - 1; ++c){
            const uint64_t v00 = static_cast<uint64_t>(r * cols + c);
            const uint64_t v10 = static_cast<uint64_t>(r * cols + c + 1);
            const uint64_t v01 = static_cast<uint64_t>((r + 1) * cols + c);
            const uint64_t v11 = static_cast<uint64_t>((r + 1) * cols + c + 1);

            mesh.faces.push_back({v00, v10, v11});
            mesh.faces.push_back({v00, v11, v01});
        }
    }

    return mesh;
}


TEST_CASE("HardVertexConstraint construction"){
    SUBCASE("default construction"){
        HardVertexConstraint c;
        CHECK(c.vertex_index == 0);
        CHECK(c.target_position.x == doctest::Approx(0.0));
        CHECK(c.target_position.y == doctest::Approx(0.0));
        CHECK(c.target_position.z == doctest::Approx(0.0));
    }

    SUBCASE("parameterized construction"){
        HardVertexConstraint c(5, vec3<double>(1.0, 2.0, 3.0));
        CHECK(c.vertex_index == 5);
        CHECK(c.target_position.x == doctest::Approx(1.0));
        CHECK(c.target_position.y == doctest::Approx(2.0));
        CHECK(c.target_position.z == doctest::Approx(3.0));
    }
}


TEST_CASE("SoftVertexConstraint construction"){
    SUBCASE("default construction"){
        SoftVertexConstraint c;
        CHECK(c.vertex_index == 0);
        CHECK(c.target_position.x == doctest::Approx(0.0));
        CHECK(c.stiffness == doctest::Approx(1.0));
    }

    SUBCASE("parameterized construction"){
        SoftVertexConstraint c(3, vec3<double>(4.0, 5.0, 6.0), 2.5);
        CHECK(c.vertex_index == 3);
        CHECK(c.target_position.x == doctest::Approx(4.0));
        CHECK(c.target_position.y == doctest::Approx(5.0));
        CHECK(c.target_position.z == doctest::Approx(6.0));
        CHECK(c.stiffness == doctest::Approx(2.5));
    }

    SUBCASE("default stiffness"){
        SoftVertexConstraint c(1, vec3<double>(1.0, 0.0, 0.0));
        CHECK(c.stiffness == doctest::Approx(1.0));
    }
}


TEST_CASE("DeformMeshesARAPParams default values"){
    DeformMeshesARAPParams params;
    CHECK(params.hard_constraints.empty());
    CHECK(params.soft_constraints.empty());
    CHECK(params.max_iterations == 10);
    CHECK(params.convergence_threshold == doctest::Approx(1e-6));
    CHECK(params.use_cotangent_weights == true);
    CHECK(params.final_energy == doctest::Approx(0.0));
    CHECK(params.initial_energy == doctest::Approx(0.0));
    CHECK(params.iterations_performed == 0);
    CHECK(params.converged == false);
    CHECK(params.energy_history.empty());
    CHECK(params.error_message.empty());
}


TEST_CASE("ARAPHelpers::ComputeVertexNeighbors"){
    SUBCASE("tetrahedron mesh"){
        auto mesh = make_tetrahedron_mesh();
        auto neighbors = ARAPHelpers::ComputeVertexNeighbors(mesh);

        CHECK(neighbors.size() == 4);
        // Each vertex in a tetrahedron is connected to 3 others.
        for(size_t i = 0; i < 4; ++i){
            CHECK(neighbors[i].size() == 3);
        }
    }

    SUBCASE("cube mesh"){
        auto mesh = make_cube_mesh();
        auto neighbors = ARAPHelpers::ComputeVertexNeighbors(mesh);

        CHECK(neighbors.size() == 8);
        // Each corner vertex of a triangulated cube is connected to 3-5 others
        // depending on triangulation.
        for(size_t i = 0; i < 8; ++i){
            CHECK(neighbors[i].size() >= 3);
        }
    }
}


TEST_CASE("DeformMeshesARAP with no constraints"){
    SUBCASE("tetrahedron unchanged"){
        auto mesh = make_tetrahedron_mesh();
        DeformMeshesARAPParams params;
        params.max_iterations = 5;

        auto result = DeformMeshesARAP(mesh, params);

#ifdef DCMA_USE_EIGEN
        // Without constraints, the mesh should remain (approximately) unchanged.
        CHECK(result.vertices.size() == mesh.vertices.size());
        for(size_t i = 0; i < mesh.vertices.size(); ++i){
            CHECK(result.vertices[i].x == doctest::Approx(mesh.vertices[i].x).epsilon(1e-6));
            CHECK(result.vertices[i].y == doctest::Approx(mesh.vertices[i].y).epsilon(1e-6));
            CHECK(result.vertices[i].z == doctest::Approx(mesh.vertices[i].z).epsilon(1e-6));
        }
        CHECK(params.error_message.empty());
#else
        // Without Eigen, the function should return the original mesh.
        CHECK(!params.error_message.empty());
#endif
    }
}


TEST_CASE("DeformMeshesARAP output is independent copy"){
    auto mesh = make_tetrahedron_mesh();
    DeformMeshesARAPParams params;
    params.max_iterations = 1;

    auto result = DeformMeshesARAP(mesh, params);

#ifdef DCMA_USE_EIGEN
    // Modify the result and verify the original is unchanged.
    if(!result.vertices.empty()){
        result.vertices[0].x += 100.0;
        CHECK(mesh.vertices[0].x != doctest::Approx(result.vertices[0].x));
    }
#endif
}


TEST_CASE("DeformMeshesARAP with hard constraints"){
#ifdef DCMA_USE_EIGEN
    SUBCASE("single vertex moved"){
        auto mesh = make_tetrahedron_mesh();
        DeformMeshesARAPParams params;
        params.max_iterations = 10;

        // Move vertex 0 to a new position.
        const vec3<double> new_pos(5.0, 5.0, 5.0);
        params.hard_constraints.push_back(HardVertexConstraint(0, new_pos));

        // Keep vertex 1 fixed at its original position.
        params.hard_constraints.push_back(HardVertexConstraint(1, mesh.vertices[1]));

        auto result = DeformMeshesARAP(mesh, params);

        CHECK(params.error_message.empty());
        // The constrained vertices should be at their target positions.
        CHECK(result.vertices[0].x == doctest::Approx(new_pos.x).epsilon(1e-9));
        CHECK(result.vertices[0].y == doctest::Approx(new_pos.y).epsilon(1e-9));
        CHECK(result.vertices[0].z == doctest::Approx(new_pos.z).epsilon(1e-9));
        CHECK(result.vertices[1].x == doctest::Approx(mesh.vertices[1].x).epsilon(1e-9));
        CHECK(result.vertices[1].y == doctest::Approx(mesh.vertices[1].y).epsilon(1e-9));
        CHECK(result.vertices[1].z == doctest::Approx(mesh.vertices[1].z).epsilon(1e-9));
    }

    SUBCASE("all vertices fixed at original positions"){
        auto mesh = make_tetrahedron_mesh();
        DeformMeshesARAPParams params;
        params.max_iterations = 5;

        // Fix all vertices at their original positions.
        for(size_t i = 0; i < mesh.vertices.size(); ++i){
            params.hard_constraints.push_back(
                HardVertexConstraint(static_cast<uint64_t>(i), mesh.vertices[i]));
        }

        auto result = DeformMeshesARAP(mesh, params);

        CHECK(params.error_message.empty());
        // All vertices should remain at their original positions.
        for(size_t i = 0; i < mesh.vertices.size(); ++i){
            CHECK(result.vertices[i].x == doctest::Approx(mesh.vertices[i].x).epsilon(1e-9));
            CHECK(result.vertices[i].y == doctest::Approx(mesh.vertices[i].y).epsilon(1e-9));
            CHECK(result.vertices[i].z == doctest::Approx(mesh.vertices[i].z).epsilon(1e-9));
        }
    }
#endif
}


TEST_CASE("DeformMeshesARAP with soft constraints"){
#ifdef DCMA_USE_EIGEN
    SUBCASE("soft constraint attracts vertex"){
        auto mesh = make_plane_mesh(3, 3, 1.0);
        DeformMeshesARAPParams params;
        params.max_iterations = 20;
        params.use_cotangent_weights = false; // Use uniform weights for simplicity.

        // Fix the corner vertices.
        params.hard_constraints.push_back(HardVertexConstraint(0, mesh.vertices[0]));
        params.hard_constraints.push_back(HardVertexConstraint(2, mesh.vertices[2]));
        params.hard_constraints.push_back(HardVertexConstraint(6, mesh.vertices[6]));
        params.hard_constraints.push_back(HardVertexConstraint(8, mesh.vertices[8]));

        // Apply a soft constraint to the center vertex (index 4) pulling it up.
        const vec3<double> target(1.0, 1.0, 1.0);
        params.soft_constraints.push_back(SoftVertexConstraint(4, target, 10.0));

        auto result = DeformMeshesARAP(mesh, params);

        CHECK(params.error_message.empty());
        // The center vertex should have moved toward the target (z should be positive).
        CHECK(result.vertices[4].z > 0.0);
    }
#endif
}


TEST_CASE("DeformMeshesARAP energy decreases or stays same"){
#ifdef DCMA_USE_EIGEN
    auto mesh = make_cube_mesh();
    DeformMeshesARAPParams params;
    params.max_iterations = 20;

    // Apply some constraints.
    params.hard_constraints.push_back(HardVertexConstraint(0, mesh.vertices[0]));
    params.hard_constraints.push_back(
        HardVertexConstraint(6, mesh.vertices[6] + vec3<double>(0.5, 0.0, 0.0)));

    auto result = DeformMeshesARAP(mesh, params);

    CHECK(params.error_message.empty());
    CHECK(params.iterations_performed > 0);
    CHECK(params.energy_history.size() > 0);

    // Energy should generally converge. Check that the final energy is finite and positive.
    CHECK(std::isfinite(params.final_energy));
    CHECK(params.final_energy >= 0.0);

    // Energy should not increase dramatically - final energy should be comparable to or less than
    // energy after a few iterations. Allow for the first iterations to have higher energy
    // as the algorithm finds the optimal configuration.
    if(params.energy_history.size() >= 3){
        // The energy in the latter half should be stable or decreasing.
        const size_t mid = params.energy_history.size() / 2;
        for(size_t i = mid + 1; i < params.energy_history.size(); ++i){
            // Allow small numerical fluctuations (1% tolerance).
            CHECK(params.energy_history[i] <= params.energy_history[i-1] * 1.01 + 1e-6);
        }
    }
#endif
}


TEST_CASE("DeformMeshesARAP convergence detection"){
#ifdef DCMA_USE_EIGEN
    auto mesh = make_tetrahedron_mesh();
    DeformMeshesARAPParams params;
    params.max_iterations = 100;
    params.convergence_threshold = 1e-6;

    // Fix all vertices - should converge immediately.
    for(size_t i = 0; i < mesh.vertices.size(); ++i){
        params.hard_constraints.push_back(
            HardVertexConstraint(static_cast<uint64_t>(i), mesh.vertices[i]));
    }

    auto result = DeformMeshesARAP(mesh, params);

    CHECK(params.error_message.empty());
    // With all vertices fixed, convergence should happen quickly.
    CHECK(params.converged == true);
    CHECK(params.iterations_performed < params.max_iterations);
#endif
}


TEST_CASE("DeformMeshesARAP empty mesh handling"){
    fv_surface_mesh<double, uint64_t> empty_mesh;
    DeformMeshesARAPParams params;

    auto result = DeformMeshesARAP(empty_mesh, params);

    // Should handle empty mesh gracefully.
    CHECK(result.vertices.empty());
#ifdef DCMA_USE_EIGEN
    CHECK(!params.error_message.empty()); // Should report error for empty mesh.
#endif
}


TEST_CASE("DeformMeshesARAP uniform weights option"){
#ifdef DCMA_USE_EIGEN
    auto mesh = make_tetrahedron_mesh();
    DeformMeshesARAPParams params1;
    params1.use_cotangent_weights = true;
    params1.max_iterations = 5;

    DeformMeshesARAPParams params2;
    params2.use_cotangent_weights = false;
    params2.max_iterations = 5;

    // Apply same constraints.
    params1.hard_constraints.push_back(HardVertexConstraint(0, mesh.vertices[0]));
    params1.hard_constraints.push_back(
        HardVertexConstraint(1, mesh.vertices[1] + vec3<double>(0.1, 0.0, 0.0)));

    params2.hard_constraints = params1.hard_constraints;

    auto result1 = DeformMeshesARAP(mesh, params1);
    auto result2 = DeformMeshesARAP(mesh, params2);

    CHECK(params1.error_message.empty());
    CHECK(params2.error_message.empty());

    // Both should produce valid results (may differ slightly).
    CHECK(result1.vertices.size() == mesh.vertices.size());
    CHECK(result2.vertices.size() == mesh.vertices.size());
#endif
}


TEST_CASE("DeformMeshesARAP preserves mesh topology"){
#ifdef DCMA_USE_EIGEN
    auto mesh = make_cube_mesh();
    DeformMeshesARAPParams params;
    params.max_iterations = 5;
    params.hard_constraints.push_back(HardVertexConstraint(0, mesh.vertices[0]));
    params.hard_constraints.push_back(
        HardVertexConstraint(6, mesh.vertices[6] + vec3<double>(1.0, 0.0, 0.0)));

    auto result = DeformMeshesARAP(mesh, params);

    // Topology (faces) should be preserved.
    CHECK(result.faces.size() == mesh.faces.size());
    for(size_t i = 0; i < mesh.faces.size(); ++i){
        CHECK(result.faces[i].size() == mesh.faces[i].size());
        for(size_t j = 0; j < mesh.faces[i].size(); ++j){
            CHECK(result.faces[i][j] == mesh.faces[i][j]);
        }
    }
#endif
}


TEST_CASE("DeformMeshesARAP preserves metadata"){
#ifdef DCMA_USE_EIGEN
    auto mesh = make_tetrahedron_mesh();
    mesh.metadata["test_key"] = "test_value";
    mesh.metadata["another_key"] = "another_value";

    DeformMeshesARAPParams params;
    params.max_iterations = 1;

    auto result = DeformMeshesARAP(mesh, params);

    CHECK(result.metadata.size() == mesh.metadata.size());
    CHECK(result.metadata["test_key"] == "test_value");
    CHECK(result.metadata["another_key"] == "another_value");
#endif
}

