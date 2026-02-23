//DeformMeshesARAP.cc - A part of DICOMautomaton 2026. Written by hal clark.

#ifdef DCMA_USE_EIGEN
#else
    #error "Attempted to compile without Eigen support, which is required."
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"

#include "Explicator.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../String_Parsing.h"
#include "../ARAP_Meshes.h"

#include "DeformMeshesARAP.h"


OperationDoc OpArgDocDeformMeshesARAP(){
    OperationDoc out;
    out.name = "DeformMeshesARAP";

    out.tags.emplace_back("category: mesh processing");

    out.desc = 
        "This operation applies an As-Rigid-As-Possible (ARAP) deformation to the selected surface meshes."
        " The ARAP algorithm preserves local rigidity while deforming the mesh according to user-specified"
        " vertex constraints. It is based on the method described in: Sorkine O, Alexa M."
        " 'As-rigid-as-possible surface modeling.' Symposium on Geometry Processing 2007.";

    out.notes.emplace_back(
        "Constraints can be specified either by vertex index or by proximity to a reference position."
        " When using proximity mode, the vertex closest to the reference position is selected."
    );
    out.notes.emplace_back(
        "Hard constraints fix vertices to exact positions. Soft constraints guide vertices toward"
        " target positions with adjustable stiffness."
    );
    out.notes.emplace_back(
        "This operation requires Eigen library support (WITH_EIGEN=ON)."
    );

    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "MaxIterations";
    out.args.back().desc = "Maximum number of local-global iterations for the ARAP algorithm.";
    out.args.back().default_val = "10";
    out.args.back().expected = true;
    out.args.back().examples = { "5", "10", "20", "50" };

    out.args.emplace_back();
    out.args.back().name = "ConvergenceThreshold";
    out.args.back().desc = "Stop iterating when the energy change between iterations falls below this threshold.";
    out.args.back().default_val = "1e-6";
    out.args.back().expected = true;
    out.args.back().examples = { "1e-8", "1e-6", "1e-4" };

    out.args.emplace_back();
    out.args.back().name = "UseCotangentWeights";
    out.args.back().desc = "If true, use cotangent weights for edges (better geometric properties)."
                           " If false, use uniform weights. Note: Cotangent weights are clamped to be"
                           " strictly positive to ensure numerical stability, which may slightly alter"
                           " behavior for meshes with obtuse triangles.";
    out.args.back().default_val = "true";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };

    out.args.emplace_back();
    out.args.back().name = "HardConstraints";
    out.args.back().desc = "A semicolon-separated list of hard vertex constraints specified using function syntax."
                           " Each constraint is either 'index(vertex_index, x,y,z)' to constrain a vertex by index,"
                           " or 'proximity(rx,ry,rz, x,y,z)' to constrain the vertex closest to reference"
                           " position (rx,ry,rz) to target position (x,y,z)."
                           " Hard constraints fix vertices to exact positions.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "index(0, 1.0,2.0,3.0)",
                                 "proximity(0,0,0, 1,2,3)",
                                 "index(0, 1,2,3) ; index(5, 4,5,6)",
                                 "proximity(0,0,0, 1,2,3) ; proximity(10,0,0, 11,2,3)" };

    out.args.emplace_back();
    out.args.back().name = "SoftConstraints";
    out.args.back().desc = "A semicolon-separated list of soft vertex constraints specified using function syntax."
                           " Each constraint is either 'index(vertex_index, x,y,z, stiffness)' to constrain a"
                           " vertex by index, or 'proximity(rx,ry,rz, x,y,z, stiffness)' to constrain the"
                           " vertex closest to reference position (rx,ry,rz) to target position (x,y,z)"
                           " with the given stiffness."
                           " Higher stiffness values result in stronger attraction to the target position.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "index(4, 1.0,2.0,3.0, 5.0)",
                                 "proximity(0,0,5,  1,2,8,  10.0)" };

    out.args.emplace_back();
    out.args.back().name = "MeshLabel";
    out.args.back().desc = "A label to attach to the deformed surface mesh.";
    out.args.back().default_val = "deformed";
    out.args.back().expected = true;
    out.args.back().examples = { "deformed", "arap_result", "warped" };

    return out;
}


bool DeformMeshesARAP(Drover &DICOM_data,
                      const OperationArgPkg& OptArgs,
                      std::map<std::string, std::string>& /*InvocationMetadata*/,
                      const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();
    const auto MaxIterations = std::stol(OptArgs.getValueStr("MaxIterations").value());
    const auto ConvergenceThreshold = std::stod(OptArgs.getValueStr("ConvergenceThreshold").value());
    const auto UseCotangentWeightsStr = OptArgs.getValueStr("UseCotangentWeights").value();
    const auto HardConstraintsStr = OptArgs.getValueStr("HardConstraints").value();
    const auto SoftConstraintsStr = OptArgs.getValueStr("SoftConstraints").value();
    const auto MeshLabel = OptArgs.getValueStr("MeshLabel").value();

    //-----------------------------------------------------------------------------------------------------------------

    // Parse boolean for cotangent weights.
    const bool UseCotangentWeights = (UseCotangentWeightsStr == "true" || UseCotangentWeightsStr == "1" ||
                                      UseCotangentWeightsStr == "yes" || UseCotangentWeightsStr == "True");

    // Helper to find the vertex index closest to a given position.
    const auto find_closest_vertex = [](const fv_surface_mesh<double, uint64_t> &mesh,
                                        const vec3<double> &pos) -> uint64_t {
        if(mesh.vertices.empty()){
            throw std::runtime_error("Unable to locate nearest vertex; mesh contains no vertices");
        }
        uint64_t closest_idx = 0;
        double min_dist_sq = std::numeric_limits<double>::max();
        for(size_t i = 0; i < mesh.vertices.size(); ++i){
            const double dist_sq = mesh.vertices[i].sq_dist(pos);
            if(dist_sq < min_dist_sq){
                min_dist_sq = dist_sq;
                closest_idx = static_cast<uint64_t>(i);
            }
        }
        return closest_idx;
    };

    // Helper to extract a numeric value from a parsed function parameter.
    const auto get_num = [](const function_parameter &fp, const std::string &desc) -> double {
        if(!fp.number.has_value()){
            throw std::invalid_argument("Expected a number for " + desc + " but got '" + fp.raw + "'");
        }
        return fp.number.value();
    };

    // Regex patterns for constraint function names.
    const auto regex_index     = Compile_Regex("^in?d?e?x?$");
    const auto regex_proximity = Compile_Regex("^pr?o?x?i?m?i?t?y?$");

    // Select meshes.
    auto SMs_all = All_SMs(DICOM_data);
    auto SMs = Whitelist(SMs_all, MeshSelectionStr);

    int64_t completed = 0;
    const auto sm_count = SMs.size();

    for(auto &smp_it : SMs){
        const auto &input_mesh = (*smp_it)->meshes;

        // Set up ARAP parameters.
        DeformMeshesARAPParams params;
        params.max_iterations = MaxIterations;
        params.convergence_threshold = ConvergenceThreshold;
        params.use_cotangent_weights = UseCotangentWeights;

        // Parse hard constraints.
        if(!HardConstraintsStr.empty()){
            const auto pfs = parse_functions(HardConstraintsStr);
            for(const auto &pf : pfs){
                if(!pf.children.empty()){
                    throw std::invalid_argument("Children functions are not accepted in hard constraints");
                }
                if(std::regex_match(pf.name, regex_proximity)){
                    // Format: proximity(rx, ry, rz, x, y, z)
                    if(pf.parameters.size() != 6){
                        throw std::invalid_argument("Hard proximity constraint requires 6 parameters:"
                                                    " proximity(rx, ry, rz, x, y, z)");
                    }
                    const vec3<double> ref_pos(
                        get_num(pf.parameters.at(0), "rx"),
                        get_num(pf.parameters.at(1), "ry"),
                        get_num(pf.parameters.at(2), "rz"));
                    const vec3<double> target_pos(
                        get_num(pf.parameters.at(3), "x"),
                        get_num(pf.parameters.at(4), "y"),
                        get_num(pf.parameters.at(5), "z"));
                    const uint64_t vertex_idx = find_closest_vertex(input_mesh, ref_pos);
                    params.hard_constraints.emplace_back(vertex_idx, target_pos);
                    YLOGINFO("Hard constraint: vertex " << vertex_idx << " (closest to " << ref_pos << ") -> " << target_pos);
                } else if(std::regex_match(pf.name, regex_index)){
                    // Format: index(vertex_index, x, y, z)
                    if(pf.parameters.size() != 4){
                        throw std::invalid_argument("Hard index constraint requires 4 parameters:"
                                                    " index(vertex_index, x, y, z)");
                    }
                    const double raw_idx = get_num(pf.parameters.at(0), "vertex_index");
                    if(raw_idx < 0.0 || raw_idx != std::floor(raw_idx)){
                        throw std::invalid_argument("vertex_index must be a non-negative integer but got '"
                                                    + pf.parameters.at(0).raw + "'");
                    }
                    const uint64_t vertex_idx = static_cast<uint64_t>(raw_idx);
                    const vec3<double> target_pos(
                        get_num(pf.parameters.at(1), "x"),
                        get_num(pf.parameters.at(2), "y"),
                        get_num(pf.parameters.at(3), "z"));
                    params.hard_constraints.emplace_back(vertex_idx, target_pos);
                    YLOGINFO("Hard constraint: vertex " << vertex_idx << " -> " << target_pos);
                } else {
                    throw std::invalid_argument("Unrecognized hard constraint function '" + pf.name + "'."
                                                " Expected 'index' or 'proximity'.");
                }
            }
        }

        // Parse soft constraints.
        if(!SoftConstraintsStr.empty()){
            const auto pfs = parse_functions(SoftConstraintsStr);
            for(const auto &pf : pfs){
                if(!pf.children.empty()){
                    throw std::invalid_argument("Children functions are not accepted in soft constraints");
                }
                if(std::regex_match(pf.name, regex_proximity)){
                    // Format: proximity(rx, ry, rz, x, y, z, stiffness)
                    if(pf.parameters.size() != 7){
                        throw std::invalid_argument("Soft proximity constraint requires 7 parameters:"
                                                    " proximity(rx, ry, rz, x, y, z, stiffness)");
                    }
                    const vec3<double> ref_pos(
                        get_num(pf.parameters.at(0), "rx"),
                        get_num(pf.parameters.at(1), "ry"),
                        get_num(pf.parameters.at(2), "rz"));
                    const vec3<double> target_pos(
                        get_num(pf.parameters.at(3), "x"),
                        get_num(pf.parameters.at(4), "y"),
                        get_num(pf.parameters.at(5), "z"));
                    const double stiffness = get_num(pf.parameters.at(6), "stiffness");
                    const uint64_t vertex_idx = find_closest_vertex(input_mesh, ref_pos);
                    params.soft_constraints.emplace_back(vertex_idx, target_pos, stiffness);
                    YLOGINFO("Soft constraint: vertex " << vertex_idx << " (closest to " << ref_pos
                             << ") -> " << target_pos << " with stiffness " << stiffness);
                } else if(std::regex_match(pf.name, regex_index)){
                    // Format: index(vertex_index, x, y, z, stiffness)
                    if(pf.parameters.size() != 5){
                        throw std::invalid_argument("Soft index constraint requires 5 parameters:"
                                                    " index(vertex_index, x, y, z, stiffness)");
                    }
                    const double raw_idx = get_num(pf.parameters.at(0), "vertex_index");
                    if(raw_idx < 0.0 || raw_idx != std::floor(raw_idx)){
                        throw std::invalid_argument("vertex_index must be a non-negative integer but got '"
                                                    + pf.parameters.at(0).raw + "'");
                    }
                    const uint64_t vertex_idx = static_cast<uint64_t>(raw_idx);
                    const vec3<double> target_pos(
                        get_num(pf.parameters.at(1), "x"),
                        get_num(pf.parameters.at(2), "y"),
                        get_num(pf.parameters.at(3), "z"));
                    const double stiffness = get_num(pf.parameters.at(4), "stiffness");
                    params.soft_constraints.emplace_back(vertex_idx, target_pos, stiffness);
                    YLOGINFO("Soft constraint: vertex " << vertex_idx << " -> " << target_pos
                             << " with stiffness " << stiffness);
                } else {
                    throw std::invalid_argument("Unrecognized soft constraint function '" + pf.name + "'."
                                                " Expected 'index' or 'proximity'.");
                }
            }
        }

        // Perform ARAP deformation.
        YLOGINFO("Performing ARAP deformation on mesh with " << input_mesh.vertices.size() << " vertices");
        auto deformed_mesh = ::DeformMeshesARAP(input_mesh, params);

        // Log statistics.
        if(!params.error_message.empty()){
            YLOGWARN("ARAP deformation warning/error: " << params.error_message);
        }
        YLOGINFO("ARAP deformation completed:");
        YLOGINFO("  Iterations performed: " << params.iterations_performed);
        YLOGINFO("  Converged: " << (params.converged ? "yes" : "no"));
        YLOGINFO("  Initial energy: " << params.initial_energy);
        YLOGINFO("  Final energy: " << params.final_energy);

        // Store statistics in mesh metadata.
        deformed_mesh.metadata["MeshLabel"] = MeshLabel;
        deformed_mesh.metadata["Description"] = "ARAP-deformed surface mesh";
        deformed_mesh.metadata["ARAP_iterations_performed"] = std::to_string(params.iterations_performed);
        deformed_mesh.metadata["ARAP_converged"] = params.converged ? "true" : "false";
        deformed_mesh.metadata["ARAP_initial_energy"] = std::to_string(params.initial_energy);
        deformed_mesh.metadata["ARAP_final_energy"] = std::to_string(params.final_energy);
        deformed_mesh.metadata["ARAP_max_iterations"] = std::to_string(params.max_iterations);
        deformed_mesh.metadata["ARAP_convergence_threshold"] = std::to_string(params.convergence_threshold);
        deformed_mesh.metadata["ARAP_use_cotangent_weights"] = UseCotangentWeights ? "true" : "false";
        deformed_mesh.metadata["ARAP_hard_constraints_count"] = std::to_string(params.hard_constraints.size());
        deformed_mesh.metadata["ARAP_soft_constraints_count"] = std::to_string(params.soft_constraints.size());

        if(!params.error_message.empty()){
            deformed_mesh.metadata["ARAP_error_message"] = params.error_message;
        }

        // Create new surface mesh and add to data.
        auto sm = std::make_shared<Surface_Mesh>();
        sm->meshes = std::move(deformed_mesh);
        DICOM_data.smesh_data.emplace_back(std::move(sm));

        ++completed;
        YLOGINFO("Completed " << completed << " of " << sm_count
                 << " --> " << static_cast<int>(1000.0 * completed / sm_count) / 10.0 << "% done");
    }

    return true;
}
