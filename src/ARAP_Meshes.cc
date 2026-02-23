//ARAP_Meshes.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains an implementation of the ARAP (As-Rigid-As-Possible) surface modeling
// algorithm as described in: Sorkine O, Alexa M. "As-rigid-as-possible surface modeling."
// Symposium on Geometry processing 2007 Jul 4 (Vol. 4, pp. 109-116).

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef DCMA_USE_EIGEN
    #include <eigen3/Eigen/Dense>
    #include <eigen3/Eigen/SVD>
    #include <eigen3/Eigen/Sparse>
    #include <eigen3/Eigen/SparseCholesky>
#endif // DCMA_USE_EIGEN

#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"

#include "ARAP_Meshes.h"

namespace ARAPHelpers {

std::vector<std::vector<uint64_t>> ComputeVertexNeighbors(
    const fv_surface_mesh<double, uint64_t> &mesh) {

    const size_t N = mesh.vertices.size();
    std::vector<std::set<uint64_t>> neighbor_sets(N);

    // Iterate over all faces and record adjacencies.
    for(const auto &face : mesh.faces){
        const size_t face_size = face.size();
        for(size_t i = 0; i < face_size; ++i){
            const uint64_t v1 = face[i];
            const uint64_t v2 = face[(i + 1) % face_size];
            if(v1 < N && v2 < N){
                neighbor_sets[v1].insert(v2);
                neighbor_sets[v2].insert(v1);
            }
        }
    }

    // Convert sets to vectors.
    std::vector<std::vector<uint64_t>> neighbors(N);
    for(size_t i = 0; i < N; ++i){
        neighbors[i] = std::vector<uint64_t>(neighbor_sets[i].begin(), neighbor_sets[i].end());
    }

    return neighbors;
}

std::map<std::pair<uint64_t, uint64_t>, double> ComputeCotangentWeights(
    const fv_surface_mesh<double, uint64_t> &mesh,
    const std::vector<std::vector<uint64_t>> & /*neighbors*/) {

    std::map<std::pair<uint64_t, uint64_t>, double> weights;

    // For each face, compute cotangent weights for edges.
    // The cotangent weight for edge (i,j) is (cot(alpha) + cot(beta)) / 2,
    // where alpha and beta are the angles opposite to edge (i,j) in adjacent faces.

    // First pass: accumulate cotangent contributions.
    for(const auto &face : mesh.faces){
        const size_t face_size = face.size();
        if(face_size < 3) continue;

        // For triangular faces only (most common case).
        if(face_size == 3){
            const uint64_t i0 = face[0];
            const uint64_t i1 = face[1];
            const uint64_t i2 = face[2];

            const vec3<double> &p0 = mesh.vertices.at(i0);
            const vec3<double> &p1 = mesh.vertices.at(i1);
            const vec3<double> &p2 = mesh.vertices.at(i2);

            // Edge vectors.
            const vec3<double> e01 = p1 - p0;
            const vec3<double> e02 = p2 - p0;
            const vec3<double> e12 = p2 - p1;

            // Compute cotangents of angles.
            // cot(angle at p0) = (e01 . e02) / |e01 x e02|
            // This contributes to edge (i1, i2).
            {
                const double dot = e01.Dot(e02);
                const double cross_len = e01.Cross(e02).length();
                if(cross_len > 1e-12){
                    const double cot = dot / cross_len;
                    auto key = std::minmax(i1, i2);
                    weights[key] += cot;
                }
            }

            // cot(angle at p1) contributes to edge (i0, i2).
            {
                const vec3<double> e10 = p0 - p1;
                const vec3<double> e12_local = p2 - p1;
                const double dot = e10.Dot(e12_local);
                const double cross_len = e10.Cross(e12_local).length();
                if(cross_len > 1e-12){
                    const double cot = dot / cross_len;
                    auto key = std::minmax(i0, i2);
                    weights[key] += cot;
                }
            }

            // cot(angle at p2) contributes to edge (i0, i1).
            {
                const vec3<double> e20 = p0 - p2;
                const vec3<double> e21 = p1 - p2;
                const double dot = e20.Dot(e21);
                const double cross_len = e20.Cross(e21).length();
                if(cross_len > 1e-12){
                    const double cot = dot / cross_len;
                    auto key = std::minmax(i0, i1);
                    weights[key] += cot;
                }
            }
        } else {
            // For non-triangular faces, use uniform weights.
            for(size_t i = 0; i < face_size; ++i){
                const uint64_t v1 = face[i];
                const uint64_t v2 = face[(i + 1) % face_size];
                auto key = std::minmax(v1, v2);
                weights[key] += 1.0;
            }
        }
    }

    // Normalize weights (divide by 2 to average the two cotangent contributions).
    for(auto &kv : weights){
        kv.second = std::max(kv.second * 0.5, 1e-12); // Ensure positive weights.
    }

    return weights;
}

} // namespace ARAPHelpers


fv_surface_mesh<double, uint64_t> DeformMeshesARAP(
    const fv_surface_mesh<double, uint64_t> &mesh,
    DeformMeshesARAPParams &params) {

#ifndef DCMA_USE_EIGEN
    params.error_message = "ARAP deformation requires Eigen library support (DCMA_USE_EIGEN).";
    YLOGWARN(params.error_message);
    return mesh;
#else

    // Initialize output statistics.
    params.final_energy = 0.0;
    params.initial_energy = 0.0;
    params.iterations_performed = 0;
    params.converged = false;
    params.energy_history.clear();
    params.error_message.clear();

    const size_t N = mesh.vertices.size();
    if(N == 0){
        params.error_message = "Input mesh has no vertices.";
        return mesh;
    }

    // If there are no constraints at all, return the original mesh unchanged.
    // The ARAP algorithm is underdetermined without constraints.
    if(params.hard_constraints.empty() && params.soft_constraints.empty()){
        params.converged = true;
        params.iterations_performed = 0;
        // Return a deep copy without performing any iterations.
        fv_surface_mesh<double, uint64_t> result;
        result.vertices = mesh.vertices;
        result.vertex_normals = mesh.vertex_normals;
        result.vertex_colours = mesh.vertex_colours;
        result.faces = mesh.faces;
        result.involved_faces = mesh.involved_faces;
        result.metadata = mesh.metadata;
        return result;
    }

    // Create a deep copy of the mesh for output.
    fv_surface_mesh<double, uint64_t> result;
    result.vertices = mesh.vertices;
    result.vertex_normals = mesh.vertex_normals;
    result.vertex_colours = mesh.vertex_colours;
    result.faces = mesh.faces;
    result.involved_faces = mesh.involved_faces;
    result.metadata = mesh.metadata;

    // Compute vertex neighbors.
    const auto neighbors = ARAPHelpers::ComputeVertexNeighbors(mesh);

    // Compute edge weights.
    std::map<std::pair<uint64_t, uint64_t>, double> weights;
    if(params.use_cotangent_weights){
        weights = ARAPHelpers::ComputeCotangentWeights(mesh, neighbors);
    } else {
        // Uniform weights.
        for(size_t i = 0; i < N; ++i){
            for(const auto &j : neighbors[i]){
                if(i < j){
                    weights[{i, j}] = 1.0;
                }
            }
        }
    }

    // Build a lookup for weights.
    auto get_weight = [&weights](uint64_t i, uint64_t j) -> double {
        auto key = std::minmax(i, j);
        auto it = weights.find(key);
        return (it != weights.end()) ? it->second : 1.0;
    };

    // Build set of constrained vertices for fast lookup.
    std::set<uint64_t> hard_constrained_indices;
    for(const auto &c : params.hard_constraints){
        if(c.vertex_index < N){
            hard_constrained_indices.insert(c.vertex_index);
            result.vertices[c.vertex_index] = c.target_position;
        }
    }

    // Map from vertex index to its equation row (for free vertices only).
    std::vector<int64_t> vertex_to_row(N, -1);
    std::vector<uint64_t> row_to_vertex;
    for(size_t i = 0; i < N; ++i){
        if(hard_constrained_indices.count(i) == 0){
            vertex_to_row[i] = static_cast<int64_t>(row_to_vertex.size());
            row_to_vertex.push_back(i);
        }
    }
    const size_t num_free = row_to_vertex.size();

    // Store original edge vectors.
    std::vector<std::map<uint64_t, vec3<double>>> original_edges(N);
    for(size_t i = 0; i < N; ++i){
        for(const auto &j : neighbors[i]){
            original_edges[i][j] = mesh.vertices[i] - mesh.vertices[j];
        }
    }

    // Per-vertex rotation matrices (initialized to identity).
    std::vector<Eigen::Matrix3d> rotations(N, Eigen::Matrix3d::Identity());

    // Build the sparse system matrix L (Laplacian with cotangent weights).
    // This matrix is constant throughout the iterations.
    Eigen::SparseMatrix<double> L(static_cast<int>(num_free), static_cast<int>(num_free));
    std::vector<Eigen::Triplet<double>> triplets;

    for(size_t i = 0; i < N; ++i){
        if(vertex_to_row[i] < 0) continue; // Skip constrained vertices.

        double diag_sum = 0.0;
        for(const auto &j : neighbors[i]){
            const double w = get_weight(i, j);
            diag_sum += w;

            if(vertex_to_row[j] >= 0){
                // Both vertices are free.
                triplets.emplace_back(static_cast<int>(vertex_to_row[i]),
                                      static_cast<int>(vertex_to_row[j]),
                                      -w);
            }
        }
        triplets.emplace_back(static_cast<int>(vertex_to_row[i]),
                              static_cast<int>(vertex_to_row[i]),
                              diag_sum);
    }

    // Add soft constraint contributions to the matrix.
    std::map<uint64_t, double> soft_constraint_stiffness;
    std::map<uint64_t, vec3<double>> soft_constraint_target;
    for(const auto &c : params.soft_constraints){
        if(c.vertex_index < N && vertex_to_row[c.vertex_index] >= 0){
            soft_constraint_stiffness[c.vertex_index] += c.stiffness;
            // Weighted average of targets if multiple soft constraints on same vertex.
            soft_constraint_target[c.vertex_index] = c.target_position;
        }
    }

    for(const auto &kv : soft_constraint_stiffness){
        const int row = static_cast<int>(vertex_to_row[kv.first]);
        triplets.emplace_back(row, row, kv.second);
    }

    L.setFromTriplets(triplets.begin(), triplets.end());
    L.makeCompressed();

    // Factorize the matrix (Cholesky decomposition for SPD matrix).
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver;
    solver.compute(L);
    if(solver.info() != Eigen::Success){
        params.error_message = "Failed to factorize the Laplacian matrix.";
        YLOGWARN(params.error_message);
        return result;
    }

    // Function to compute ARAP energy.
    auto compute_energy = [&]() -> double {
        double energy = 0.0;
        for(size_t i = 0; i < N; ++i){
            for(const auto &j : neighbors[i]){
                if(i < j) continue; // Count each edge once.
                const double w = get_weight(i, j);
                const vec3<double> &e_orig = original_edges[i].at(j);
                const vec3<double> e_deformed = result.vertices[i] - result.vertices[j];

                // Rotate original edge by R_i.
                const Eigen::Vector3d e_orig_eigen(e_orig.x, e_orig.y, e_orig.z);
                const Eigen::Vector3d rotated = rotations[i] * e_orig_eigen;

                const vec3<double> diff(
                    e_deformed.x - rotated(0),
                    e_deformed.y - rotated(1),
                    e_deformed.z - rotated(2));
                energy += w * diff.sq_dist(vec3<double>(0,0,0));
            }
        }
        return energy;
    };

    // Main ARAP iteration loop.
    for(int64_t iter = 0; iter < params.max_iterations; ++iter){
        // ----- Local Step: Compute optimal rotations -----
        for(size_t i = 0; i < N; ++i){
            if(neighbors[i].empty()) continue;

            // Build covariance matrix S_i = sum_j w_ij * e_ij * e'_ij^T
            Eigen::Matrix3d S = Eigen::Matrix3d::Zero();
            for(const auto &j : neighbors[i]){
                const double w = get_weight(i, j);
                const vec3<double> &e_orig = original_edges[i].at(j);
                const vec3<double> e_deformed = result.vertices[i] - result.vertices[j];

                Eigen::Vector3d e(e_orig.x, e_orig.y, e_orig.z);
                Eigen::Vector3d ep(e_deformed.x, e_deformed.y, e_deformed.z);

                S += w * e * ep.transpose();
            }

            // SVD to extract rotation: R = V * U^T
            Eigen::JacobiSVD<Eigen::Matrix3d> svd(S, Eigen::ComputeFullU | Eigen::ComputeFullV);
            Eigen::Matrix3d U = svd.matrixU();
            Eigen::Matrix3d V = svd.matrixV();

            Eigen::Matrix3d R = V * U.transpose();

            // Handle reflection (ensure det(R) = 1).
            if(R.determinant() < 0){
                // Flip the sign of the column of V corresponding to the smallest singular value.
                V.col(2) *= -1.0;
                R = V * U.transpose();
            }

            rotations[i] = R;
        }

        // ----- Global Step: Solve for optimal positions -----
        // Build right-hand side vectors (one for each coordinate).
        Eigen::VectorXd b_x(num_free), b_y(num_free), b_z(num_free);
        b_x.setZero();
        b_y.setZero();
        b_z.setZero();

        for(size_t idx = 0; idx < num_free; ++idx){
            const uint64_t i = row_to_vertex[idx];
            vec3<double> rhs(0.0, 0.0, 0.0);

            for(const auto &j : neighbors[i]){
                const double w = get_weight(i, j);
                const vec3<double> &e_orig = original_edges[i].at(j);

                // Compute (R_i + R_j) * e_ij / 2.
                Eigen::Vector3d e(e_orig.x, e_orig.y, e_orig.z);
                Eigen::Vector3d rotated_i = rotations[i] * e;
                Eigen::Vector3d rotated_j = rotations[j] * e;
                Eigen::Vector3d avg_rotated = (rotated_i + rotated_j) * 0.5;

                rhs.x += w * avg_rotated(0);
                rhs.y += w * avg_rotated(1);
                rhs.z += w * avg_rotated(2);

                // If neighbor is constrained, add its contribution to RHS.
                if(vertex_to_row[j] < 0){
                    rhs.x += w * result.vertices[j].x;
                    rhs.y += w * result.vertices[j].y;
                    rhs.z += w * result.vertices[j].z;
                }
            }

            // Add soft constraint contribution.
            if(soft_constraint_stiffness.count(i) > 0){
                const double s = soft_constraint_stiffness.at(i);
                const vec3<double> &target = soft_constraint_target.at(i);
                rhs.x += s * target.x;
                rhs.y += s * target.y;
                rhs.z += s * target.z;
            }

            b_x(static_cast<int>(idx)) = rhs.x;
            b_y(static_cast<int>(idx)) = rhs.y;
            b_z(static_cast<int>(idx)) = rhs.z;
        }

        // Solve the system.
        Eigen::VectorXd x_x = solver.solve(b_x);
        Eigen::VectorXd x_y = solver.solve(b_y);
        Eigen::VectorXd x_z = solver.solve(b_z);

        if(solver.info() != Eigen::Success){
            params.error_message = "Failed to solve the linear system.";
            YLOGWARN(params.error_message);
            break;
        }

        // Update vertex positions.
        for(size_t idx = 0; idx < num_free; ++idx){
            const uint64_t i = row_to_vertex[idx];
            result.vertices[i].x = x_x(static_cast<int>(idx));
            result.vertices[i].y = x_y(static_cast<int>(idx));
            result.vertices[i].z = x_z(static_cast<int>(idx));
        }

        // Compute energy for convergence check.
        const double energy = compute_energy();
        params.energy_history.push_back(energy);

        if(iter == 0){
            params.initial_energy = energy;
        }

        // Check convergence.
        if(iter > 0){
            const double prev_energy = params.energy_history[params.energy_history.size() - 2];
            const double energy_change = std::abs(energy - prev_energy);
            if(energy_change < params.convergence_threshold){
                params.converged = true;
                params.iterations_performed = iter + 1;
                params.final_energy = energy;
                break;
            }
        }

        params.iterations_performed = iter + 1;
        params.final_energy = energy;
    }

    // Recompute vertex normals if they existed.
    if(!result.vertex_normals.empty()){
        result.vertex_normals.clear();
        result.vertex_normals.resize(N, vec3<double>(0.0, 0.0, 0.0));

        for(const auto &face : result.faces){
            if(face.size() < 3) continue;

            // Compute face normal.
            const vec3<double> &p0 = result.vertices.at(face[0]);
            const vec3<double> &p1 = result.vertices.at(face[1]);
            const vec3<double> &p2 = result.vertices.at(face[2]);
            const vec3<double> normal = (p1 - p0).Cross(p2 - p0);

            // Accumulate to all vertices of the face.
            for(const auto &vi : face){
                if(vi < N){
                    result.vertex_normals[vi] += normal;
                }
            }
        }

        // Normalize.
        for(auto &n : result.vertex_normals){
            const double len = n.length();
            if(len > 1e-12){
                n = n / len;
            }
        }
    }

    return result;
#endif // DCMA_USE_EIGEN
}

