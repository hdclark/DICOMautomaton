//SimplifySurfaceMeshes.cc - A part of DICOMautomaton 2022. Written by hal clark.

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
#include <utility>            //Needed for std::pair.
#include <vector>

#include <asio.hpp>

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMathIOOFF.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#ifdef DCMA_USE_CGAL
    #include "../Surface_Meshes.h"
#endif //DCMA_USE_CGAL

#include "SimplifySurfaceMeshes.h"


OperationDoc OpArgDocSimplifySurfaceMeshes(){
    OperationDoc out;
    out.name = "SimplifySurfaceMeshes";

    out.desc = 
        "This operation performs mesh simplification on existing surface meshes according to"
        " the specified criteria, replacing the original meshes with simplified meshes.";
        
    out.notes.emplace_back(
        "Selected surface meshes should represent polyhedra."
    );

    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";
 

    out.args.emplace_back();
    out.args.back().name = "Method";
    out.args.back().desc = "Controls which simplification algorithm is used."
                           " Currently supported are 'flat'"
#ifdef DCMA_USE_CGAL
                           " and 'edge-collapse'"
#endif //DCMA_USE_CGAL
                           "."
                           "\n"
                           "'flat' removes vertices when the immediate surrounding patch is uniformly"
                           " flat within a given tolerance distance. Border and non-manifold vertices"
                           " are not removed, maintaining surface topology."
                           " The 'flat' algorithm works best on redundant, flat meshes, like those produced"
                           " by marching cubes."
                           " Choosing a small tolerance distance should result in a nearly lossless simplification,"
                           " but will only be applicable for meshes with redundant flat sections."
#ifdef DCMA_USE_CGAL
                           "\n"
                           "'edge-collapse' builds a priority queue of edges that can be collapsed"
                           " (converting two vertices into one) one at a time"
                           " with minimal impact on the surface."
                           " Collapse stops when a given edge count limit is reached."
                           " 'edge-collapse' is a general-purpose simplification algorithm that works well on"
                           " a variety of meshes."
#endif //DCMA_USE_CGAL
                           "";
#ifdef DCMA_USE_CGAL
    out.args.back().default_val = "edge-collapse";
#else
    out.args.back().default_val = "flat";
#endif //DCMA_USE_CGAL
    out.args.back().expected = true;
    out.args.back().examples = { "flat",
#ifdef DCMA_USE_CGAL
                                 "edge-collapse",
#endif //DCMA_USE_CGAL
                                 };
    out.args.back().samples = OpArgSamples::Exhaustive;


#ifdef DCMA_USE_CGAL
    out.args.emplace_back();
    out.args.back().name = "EdgeCountLimit";
    out.args.back().desc = "Needed for 'edge-collapse' algorithm."
                           " The maximum number of edges simplified meshes should contain.";
    out.args.back().default_val = "250000";
    out.args.back().expected = true;
    out.args.back().examples = { "20000", "100000", "500000", "5000000" };
#endif //DCMA_USE_CGAL


    out.args.emplace_back();
    out.args.back().name = "ToleranceDistance";
    out.args.back().desc = "Needed for 'flat' algorithm."
                           " The maximum allowed surface deviation (in DICOM units; mm)"
                           " above which vertices will NOT be simplified."
                           "\n"
                           "Note that this number is not the same as the maximum surface deviation after"
                           " simplification, since every nearby vertex can in principle perturb the surface"
                           " up to the tolerance distance. In most practical situations, the tolerance distance"
                           " is representative of the surface deviation after simplification."
                           "\n"
                           "Setting this number to a value much smaller than the smallest feature should cause"
                           " effectively lossless simplification of exactly-flat patches.";
    out.args.back().default_val = "0.001";
    out.args.back().expected = true;
    out.args.back().examples = { "0.001", "1E-4", "0.5", "1.5" };

    return out;
}


bool SimplifySurfaceMeshes(Drover &DICOM_data,
                             const OperationArgPkg& OptArgs,
                             std::map<std::string, std::string>& /*InvocationMetadata*/,
                             const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();

    const auto MethodStr = OptArgs.getValueStr("Method").value();

    const auto MeshEdgeCountLimit = std::stol( OptArgs.getValueStr("EdgeCountLimit").value() );
    const auto ToleranceDistance = std::stod(OptArgs.getValueStr("ToleranceDistance").value());

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_edge_collapse = Compile_Regex("^ed?g?e?[-_]?c?o?l?l?a?p?s?e?$");
    const auto regex_flat = Compile_Regex("^fl?a?t?$");

    auto SMs_all = All_SMs( DICOM_data );
    auto SMs = Whitelist( SMs_all, MeshSelectionStr );

    long int completed = 0;
    const auto sm_count = SMs.size();
    for(auto & smp_it : SMs){

        if(std::regex_match(MethodStr, regex_flat)){
            (*smp_it)->meshes.simplify_inner_triangles(ToleranceDistance);

#ifdef DCMA_USE_CGAL
        }else if(std::regex_match(MethodStr, regex_edge_collapse)){
            const auto orig_metadata = (*smp_it)->meshes.metadata;

            // Convert to a CGAL mesh.
            std::stringstream ss_i;
            if(!WriteFVSMeshToOFF( (*smp_it)->meshes, ss_i )){
                throw std::runtime_error("Unable to write mesh in OFF format. Cannot continue.");
            }

            dcma_surface_meshes::Polyhedron surface_mesh;
            if(!( ss_i >> surface_mesh )){
                throw std::runtime_error("Mesh could not be treated as a polyhedron. (Is it manifold?)");
            }

            // Simplify.
            polyhedron_processing::Simplify(surface_mesh, MeshEdgeCountLimit);

            // Convert back from CGAL mesh.
            std::stringstream ss_o;
            if(!( ss_o << surface_mesh )){
                throw std::runtime_error("Simplified mesh could not be treated as a polyhedron. (Is it manifold?)");
            }

            if(!ReadFVSMeshFromOFF( (*smp_it)->meshes, ss_o )){
                throw std::runtime_error("Unable to read mesh in OFF format. Cannot continue.");
            }

            (*smp_it)->meshes.metadata = orig_metadata;
#endif //DCMA_USE_CGAL

        }else{
            throw std::invalid_argument("Method argument '"_s + MethodStr + "' is not valid");
        }

        ++completed;
        FUNCINFO("Completed " << completed << " of " << sm_count
              << " --> " << static_cast<int>(1000.0*(completed)/sm_count)/10.0 << "% done");
    }

    return true;
}
