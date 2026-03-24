//OrientMeshes.cc - A part of DICOMautomaton 2026. Written by hal clark.

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
#include "YgorMeshesOrient.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "OrientMeshes.h"


OperationDoc OpArgDocOrientMeshes(){
    OperationDoc out;
    out.name = "OrientMeshes";

    out.tags.emplace_back("category: mesh processing");

    out.desc = 
        "This operation orients faces in surface meshes so that adjacent faces have a consistent"
        " winding order and, where possible, outward-pointing normals."
        " The algorithm uses BFS propagation for local consistency, a bounding-box heuristic for"
        " seed selection, and ray casting for global consistency across disconnected patches.";
        
    out.notes.emplace_back(
        "Selected surface meshes should represent polyhedra."
    );
    out.notes.emplace_back(
        "Non-orientable surfaces (e.g., Moebius strips) will cause a warning but will not"
        " prevent the operation from completing. However the result will not be consistent across the"
        " entire mesh."
    );

    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";

    return out;
}



bool OrientMeshes(Drover &DICOM_data,
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

        // Orient faces for consistent winding and outward normals.
        if(!OrientFaces( (*smp_it)->meshes )){
            YLOGWARN("Face orientation could not be fully resolved (mesh may be non-orientable)");
        }

        ++completed;
        YLOGINFO("Completed " << completed << " of " << sm_count
              << " --> " << static_cast<int>(1000.0*(completed)/sm_count)/10.0 << "% done");
    }

    return true;
}
