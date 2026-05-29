//SubdivideSurfaceMeshes.cc - A part of DICOMautomaton 2019, 2026. Written by hal clark.

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

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "SubdivideSurfaceMeshes.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMeshesRefinement.h"


OperationDoc OpArgDocSubdivideSurfaceMeshes(){
    OperationDoc out;
    out.name = "SubdivideSurfaceMeshes";

    out.tags.emplace_back("category: mesh processing");

    out.desc = 
        "This operation subdivides existing surface meshes according to"
        " the specified criteria, replacing the original meshes with subdivided copies."
        " Loop subdivision is used, which increases the face count by a factor of 4"
        " per iteration.";
        
    out.notes.emplace_back(
        "Selected surface meshes should represent polyhedra."
    );

    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";
 

    out.args.emplace_back();
    out.args.back().name = "Iterations";
    out.args.back().desc = "The number of times subdivision should be performed.";
    out.args.back().default_val = "2";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "2", "5" };

    return out;
}



bool SubdivideSurfaceMeshes(Drover &DICOM_data,
                              const OperationArgPkg& OptArgs,
                              std::map<std::string, std::string>& /*InvocationMetadata*/,
                              const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();
    const auto MeshIterations = std::stol( OptArgs.getValueStr("Iterations").value() );

    //-----------------------------------------------------------------------------------------------------------------


    auto SMs_all = All_SMs( DICOM_data );
    auto SMs = Whitelist( SMs_all, MeshSelectionStr );

    int64_t completed = 0;
    const auto sm_count = SMs.size();
    for(auto & smp_it : SMs){

        const auto orig_metadata = (*smp_it)->meshes.metadata;

        // Subdivide using Ygor's Loop subdivision algorithm.
        loop_subdivide( (*smp_it)->meshes, MeshIterations );

        (*smp_it)->meshes.metadata = orig_metadata;
        (*smp_it)->meshes.recreate_involved_face_index();

        ++completed;
        YLOGINFO("Completed " << completed << " of " << sm_count
              << " --> " << static_cast<int>(1000.0*(completed)/sm_count)/10.0 << "% done");
    }

    return true;
}
