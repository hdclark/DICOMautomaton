//MakeMeshesManifold.cc - A part of DICOMautomaton 2019. Written by hal clark.

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
#include "MakeMeshesManifold.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMathIOOFF.h"

#ifdef DCMA_USE_CGAL
#else
    #error "Attempted to compile without CGAL support, which is required."
#endif

#include "../Surface_Meshes.h"


OperationDoc OpArgDocMakeMeshesManifold(void){
    OperationDoc out;
    out.name = "MakeMeshesManifold";

    out.desc = 
        "This operation attempts to make non-manifold surface meshes into manifold meshes."
        " This operation is needed for operations requiring meshes to represent polyhedra.";
        
    out.notes.emplace_back(
        "This routine will invalidate any imbued special attributes from the original mesh."
    );
    out.notes.emplace_back(
        "It may not be possible to accomplish manifold-ness."
    );
    out.notes.emplace_back(
        "Mesh features (vertices, faces, edges) may disappear in this routine."
    );
        

    out.args.emplace_back();
    out.args.back().name = "MeshLabel";
    out.args.back().desc = "A label to attach to the new manifold mesh.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "body", "air", "bone", "invalid", "above_zero", "below_5.3" };


    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";

    return out;
}



Drover MakeMeshesManifold(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshLabel = OptArgs.getValueStr("MeshLabel").value();
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();

    //-----------------------------------------------------------------------------------------------------------------


    auto SMs_all = All_SMs( DICOM_data );
    auto SMs = Whitelist( SMs_all, MeshSelectionStr );

    long int completed = 0;
    const auto sm_count = SMs.size();
    for(auto & smp_it : SMs){

        DICOM_data.smesh_data.emplace_back( std::make_shared<Surface_Mesh>() );

        // Attempt to convert directly to a polyhedron.
        std::stringstream ss;
        if(!WriteFVSMeshToOFF( (*smp_it)->meshes, ss )){
            throw std::runtime_error("Unable to write mesh in OFF format. Cannot continue.");
        }



        dcma_surface_meshes::Polyhedron surface_mesh;
        if(ss >> surface_mesh){
            // Mesh was manifold, so no conversion is necessary. Store the mesh.
            *(DICOM_data.smesh_data.back()) = *(*smp_it);

        }else{
            // Mesh is likely non-manifold, but some other issue could have been encountered.
            // Attempt a conversion using a method robust to non-manifold meshes.
            surface_mesh = dcma_surface_meshes::FVSMeshToPolyhedron((*smp_it)->meshes);

            // Success. Convert back and store the mesh.
            std::stringstream ss_o;
            if(!( ss_o << surface_mesh )){
                throw std::runtime_error("Remeshed mesh could still not be treated as a polyhedron. Cannot continue.");
            }

            if(!ReadFVSMeshFromOFF( DICOM_data.smesh_data.back()->meshes, ss_o )){
                throw std::runtime_error("Unable to read mesh in OFF format. Cannot continue.");
            }
            DICOM_data.smesh_data.back()->meshes.metadata = (*smp_it)->meshes.metadata;

        }

        // Updated the metadata.
        DICOM_data.smesh_data.back()->meshes.metadata["MeshLabel"] = MeshLabel;
        
        ++completed;
        FUNCINFO("Completed " << completed << " of " << sm_count
              << " --> " << static_cast<int>(1000.0*(completed)/sm_count)/10.0 << "\% done");
    }

    return DICOM_data;
}
