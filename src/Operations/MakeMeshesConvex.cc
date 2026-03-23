//MakeMeshesConvex.cc - A part of DICOMautomaton 2026. Written by hal clark.

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
#include "YgorMeshesConvexHull.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "MakeMeshesConvex.h"


OperationDoc OpArgDocMakeMeshesConvex(){
    OperationDoc out;
    out.name = "MakeMeshesConvex";

    out.tags.emplace_back("category: mesh processing");

    out.desc = 
        "This operation replaces each selected surface mesh with its convex hull."
        " The convex hull is the smallest convex polyhedron that encloses all vertices"
        " of the original mesh.";
        
    out.notes.emplace_back(
        "The original mesh is replaced in-place; metadata is preserved."
    );
    out.notes.emplace_back(
        "The convex hull is only meaningful for meshes with at least four non-coplanar vertices."
    );

    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";

    return out;
}



bool MakeMeshesConvex(Drover &DICOM_data,
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

        const auto orig_metadata = (*smp_it)->meshes.metadata;

        // Compute the convex hull using Ygor's ConvexHull class.
        ConvexHull<double> ch;
        ch.add_vertices( (*smp_it)->meshes.vertices );
        (*smp_it)->meshes = ch.get_mesh();

        (*smp_it)->meshes.metadata = orig_metadata;

        ++completed;
        YLOGINFO("Completed " << completed << " of " << sm_count
              << " --> " << static_cast<int>(1000.0*(completed)/sm_count)/10.0 << "% done");
    }

    return true;
}
