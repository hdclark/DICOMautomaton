//CombineMeshes.cc - A part of DICOMautomaton 2022. Written by hal clark.

#include <deque>
#include <optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"

#include "CombineMeshes.h"

OperationDoc OpArgDocCombineMeshes(){
    OperationDoc out;
    out.name = "CombineMeshes";

    out.desc = 
        "This operation deep-copies the selected surface meshes, combining all into a single mesh.";

    out.notes.emplace_back(
        "This operation does *not* implement 3D boolean operations. Using it can lead"
        " to mesh intersections and non-manifoldness, so it is best suited for visualization or"
        " as part of a controlled explode-alter-combine workflow."
    );

    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";
    
    out.args.emplace_back();
    out.args.back().name = "MeshLabel";
    out.args.back().desc = "A label to attach to the combined surface mesh.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "body", "air", "bone", "invalid", "above_zero", "below_5.3" };

    return out;
}

bool CombineMeshes(Drover &DICOM_data,
                  const OperationArgPkg& OptArgs,
                  std::map<std::string, std::string>& /*InvocationMetadata*/,
                  const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();
    const auto MeshLabel = OptArgs.getValueStr("MeshLabel").value();

    //-----------------------------------------------------------------------------------------------------------------

    // Gather a list of the selected meshes.
    std::deque<std::shared_ptr<Surface_Mesh>> sm_selection;

    auto SMs_all = All_SMs( DICOM_data );
    auto SMs = Whitelist( SMs_all, MeshSelectionStr );
    for(auto & smp_it : SMs){
        sm_selection.push_back(*smp_it);
    }

    // Copy the meshes.
    if(!sm_selection.empty()){
        auto sm = std::make_shared<Surface_Mesh>();

        for(auto & l_sm : sm_selection){
            const auto orig_N_verts = static_cast<uint64_t>(sm->meshes.vertices.size());

            sm->meshes.vertices.insert( std::end(sm->meshes.vertices),
                                        std::begin(l_sm->meshes.vertices),
                                        std::end(l_sm->meshes.vertices) );
            for(const auto& f : l_sm->meshes.faces){
                sm->meshes.faces.push_back( f );
                for(auto& n : sm->meshes.faces.back()){
                    n += orig_N_verts;
                }
            }
        }

        DICOM_data.smesh_data.emplace_back( std::move(sm) );
    }

    return true;
}
