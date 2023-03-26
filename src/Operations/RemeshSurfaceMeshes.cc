//RemeshSurfaceMeshes.cc - A part of DICOMautomaton 2019. Written by hal clark.

#ifdef DCMA_USE_CGAL
#else
    #error "Attempted to compile without CGAL support, which is required."
#endif

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

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "RemeshSurfaceMeshes.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMathIOOFF.h"

#include "../Surface_Meshes.h"


OperationDoc OpArgDocRemeshSurfaceMeshes(){
    OperationDoc out;
    out.name = "RemeshSurfaceMeshes";

    out.desc = 
        "This operation re-meshes existing surface meshes according to the specified criteria, replacing the"
        " original meshes with remeshed copies.";
        
    out.notes.emplace_back(
        "Selected surface meshes should represent polyhedra."
    );

    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";
 

    out.args.emplace_back();
    out.args.back().name = "Iterations";
    out.args.back().desc = "The number of remeshing iterations to perform.";
    out.args.back().default_val = "5";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "3", "5", "10" };


    out.args.emplace_back();
    out.args.back().name = "TargetEdgeLength";
    out.args.back().desc = "The desired length of all edges in the remeshed mesh in DICOM units (mm).";
    out.args.back().default_val = "1.5";
    out.args.back().expected = true;
    out.args.back().examples = { "0.2", "0.75", "1.0", "1.5", "2.015" };

    return out;
}



bool RemeshSurfaceMeshes(Drover &DICOM_data,
                           const OperationArgPkg& OptArgs,
                           std::map<std::string, std::string>& /*InvocationMetadata*/,
                           const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();
    const auto MeshIterations = std::stol( OptArgs.getValueStr("Iterations").value() );
    const auto MeshTargetEdgeLength = std::stol( OptArgs.getValueStr("TargetEdgeLength").value() );

    //-----------------------------------------------------------------------------------------------------------------


    auto SMs_all = All_SMs( DICOM_data );
    auto SMs = Whitelist( SMs_all, MeshSelectionStr );

    long int completed = 0;
    const auto sm_count = SMs.size();
    for(auto & smp_it : SMs){

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

        // Remesh.
        polyhedron_processing::Remesh(surface_mesh, MeshTargetEdgeLength, MeshIterations);

        // Convert back from CGAL mesh.
        std::stringstream ss_o;
        if(!( ss_o << surface_mesh )){
            throw std::runtime_error("Remeshed mesh could not be treated as a polyhedron. (Is it manifold?)");
        }

        if(!ReadFVSMeshFromOFF( (*smp_it)->meshes, ss_o )){
            throw std::runtime_error("Unable to read mesh in OFF format. Cannot continue.");
        }

        (*smp_it)->meshes.metadata = orig_metadata;

        ++completed;
        YLOGINFO("Completed " << completed << " of " << sm_count
              << " --> " << static_cast<int>(1000.0*(completed)/sm_count)/10.0 << "% done");
    }

    return true;
}
