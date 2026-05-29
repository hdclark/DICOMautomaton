//PatchMeshHoles.cc - A part of DICOMautomaton 2026. Written by hal clark.

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
#include <cstdint>

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMeshesHoles.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "PatchMeshHoles.h"


OperationDoc OpArgDocPatchMeshHoles(){
    OperationDoc out;
    out.name = "PatchMeshHoles";

    out.tags.emplace_back("category: mesh processing");

    out.desc = 
        "This operation detects boundary edges (holes) in surface meshes and attempts to"
        " fill them by triangulating each closed boundary chain."
        " This is useful for repairing meshes with missing faces or open boundaries.";
        
    out.notes.emplace_back(
        "Selected surface meshes should represent polyhedra."
    );
    out.notes.emplace_back(
        "Only closed boundary chains with at least 3 vertices are filled."
        " Open chains are left unmodified."
    );
    out.notes.emplace_back(
        "Face orientations are made consistent after hole filling."
    );

    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";

    return out;
}



bool PatchMeshHoles(Drover &DICOM_data,
                      const OperationArgPkg& OptArgs,
                      std::map<std::string, std::string>& /*InvocationMetadata*/,
                      const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    auto SMs_all = All_SMs( DICOM_data );
    auto SMs = Whitelist( SMs_all, MeshSelectionStr );

    int64_t completed = 0;
    const auto sm_count = SMs.size();
    for(auto & smp_it : SMs){

        // Detect boundary chains (holes) in the mesh.
        auto holes = FindBoundaryChains( (*smp_it)->meshes );

        const auto N_holes = holes.chains.size();
        YLOGINFO("Found " << N_holes << " boundary chain(s) in mesh");

        if(N_holes != 0){
            // Fill the detected holes by triangulating each closed boundary chain.
            if(!FillBoundaryChainsByZippering( (*smp_it)->meshes, holes )){
                YLOGWARN("Hole filling encountered issues (e.g., non-manifold edges)");
            }

            // Attempt to ensure consistent face orientation after hole filling.
            EnsureConsistentFaceOrientation( (*smp_it)->meshes );
        }

        ++completed;
        YLOGINFO("Completed " << completed << " of " << sm_count
              << " --> " << static_cast<int>(1000.0*(completed)/sm_count)/10.0 << "% done");
    }

    return true;
}
