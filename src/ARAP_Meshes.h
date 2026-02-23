//ARAP_Meshes.h - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains an implementation of the ARAP (As-Rigid-As-Possible) surface modeling
// algorithm as described in: Sorkine O, Alexa M. "As-rigid-as-possible surface modeling."
// Symposium on Geometry processing 2007 Jul 4 (Vol. 4, pp. 109-116).

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <map>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorMath.h"         //Needed for vec3 class and fv_surface_mesh.

// A hard vertex constraint forces a vertex to a specific position.
// The vertex is fixed at the target position during the ARAP optimization.
struct HardVertexConstraint {
    uint64_t vertex_index = 0;  // Index into fv_surface_mesh::vertices.
    vec3<double> target_position = vec3<double>(0.0, 0.0, 0.0);

    HardVertexConstraint() = default;
    HardVertexConstraint(uint64_t idx, const vec3<double> &pos)
        : vertex_index(idx), target_position(pos) {}
};

// A soft vertex constraint guides a vertex toward a target position with a stiffness weight.
// Higher stiffness values result in stronger attraction to the target position.
struct SoftVertexConstraint {
    uint64_t vertex_index = 0;  // Index into fv_surface_mesh::vertices.
    vec3<double> target_position = vec3<double>(0.0, 0.0, 0.0);
    double stiffness = 1.0;     // Weight controlling the strength of the constraint (>= 0).

    SoftVertexConstraint() = default;
    SoftVertexConstraint(uint64_t idx, const vec3<double> &pos, double stiff = 1.0)
        : vertex_index(idx), target_position(pos), stiffness(stiff) {}
};

// Parameters and statistics for the ARAP deformation algorithm.
struct DeformMeshesARAPParams {
    // ---- Input Parameters ----

    // Vertex constraints.
    std::vector<HardVertexConstraint> hard_constraints;
    std::vector<SoftVertexConstraint> soft_constraints;

    // Algorithm parameters.
    int64_t max_iterations = 10;       // Maximum number of local-global iterations.
    double convergence_threshold = 1e-6; // Stop when energy change is below this threshold.

    // Edge weights: if true, use cotangent weights; otherwise, use uniform weights.
    bool use_cotangent_weights = true;

    // ---- Output Statistics (filled by DeformMeshesARAP) ----

    // The final ARAP energy after deformation.
    double final_energy = 0.0;

    // The initial ARAP energy before deformation (after applying constraints).
    double initial_energy = 0.0;

    // The number of iterations actually performed.
    int64_t iterations_performed = 0;

    // Whether the algorithm converged (energy change below threshold).
    bool converged = false;

    // Per-iteration energy values (for debugging/analysis).
    std::vector<double> energy_history;

    // Error messages (if any).
    std::string error_message;
};

// Helper functions in a namespace for internal use.
namespace ARAPHelpers {

// Compute the one-ring neighbors for each vertex in the mesh.
// Returns a vector of vectors, where neighbors[i] contains the indices of vertices adjacent to vertex i.
std::vector<std::vector<uint64_t>> ComputeVertexNeighbors(
    const fv_surface_mesh<double, uint64_t> &mesh);

// Compute cotangent weights for edges in the mesh.
// Returns a map from vertex pair (sorted) to weight.
std::map<std::pair<uint64_t, uint64_t>, double> ComputeCotangentWeights(
    const fv_surface_mesh<double, uint64_t> &mesh,
    const std::vector<std::vector<uint64_t>> &neighbors);

} // namespace ARAPHelpers


// Main ARAP deformation function.
//
// Deforms the input mesh according to the ARAP algorithm with the given constraints.
// Returns a new mesh that is completely independent of the input (no shared memory).
//
// The algorithm alternates between:
// 1. Local step: Estimate optimal rotation for each vertex based on current positions.
// 2. Global step: Solve a linear system to find optimal positions given the rotations.
//
// Parameters:
//   mesh - Input surface mesh (assumed watertight, face-vertex representation).
//   params - Parameters including constraints and algorithm settings.
//            Output statistics are written to this struct.
//
// Returns:
//   A deformed copy of the input mesh.
fv_surface_mesh<double, uint64_t> DeformMeshesARAP(
    const fv_surface_mesh<double, uint64_t> &mesh,
    DeformMeshesARAPParams &params);

