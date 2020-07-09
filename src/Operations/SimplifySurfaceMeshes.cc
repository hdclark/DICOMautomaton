//SimplifySurfaceMeshes.cc - A part of DICOMautomaton 2019. Written by hal clark.

#ifdef DCMA_USE_CGAL
#else
    #error "Attempted to compile without CGAL support, which is required."
#endif

#include <asio.hpp>
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
#include "SimplifySurfaceMeshes.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMathIOOFF.h"

#include "../Surface_Meshes.h"


OperationDoc OpArgDocSimplifySurfaceMeshes(){
    OperationDoc out;
    out.name = "SimplifySurfaceMeshes";

    out.desc = 
        "This operation performs mesh simplification on existing surface meshes according to"
        " the specified criteria, replacing the original meshes with simplified copies.";
        
    out.notes.emplace_back(
        "Selected surface meshes should represent polyhedra."
    );

    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";
 

    out.args.emplace_back();
    out.args.back().name = "EdgeCountLimit";
    out.args.back().desc = "The maximum number of edges simplified meshes should contain.";
    out.args.back().default_val = "250000";
    out.args.back().expected = true;
    out.args.back().examples = { "20000", "100000", "500000", "5000000" };

    return out;
}



Drover SimplifySurfaceMeshes(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();
    const auto MeshEdgeCountLimit = std::stol( OptArgs.getValueStr("EdgeCountLimit").value() );

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

        ++completed;
        FUNCINFO("Completed " << completed << " of " << sm_count
              << " --> " << static_cast<int>(1000.0*(completed)/sm_count)/10.0 << "% done");
    }

    return DICOM_data;
}
