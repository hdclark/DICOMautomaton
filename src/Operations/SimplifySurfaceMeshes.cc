//SimplifySurfaceMeshes.cc - A part of DICOMautomaton 2022, 2024. Written by hal clark.
//
// CGAL edge-collapse has been removed. Only the native 'flat' algorithm is available.

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
#include "YgorMathIOOFF.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "SimplifySurfaceMeshes.h"


OperationDoc OpArgDocSimplifySurfaceMeshes(){
    OperationDoc out;
    out.name = "SimplifySurfaceMeshes";

    out.tags.emplace_back("category: mesh processing");

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
                           " Currently supported is 'flat'."
                           "\n\n"
                           "'flat' removes vertices when the immediate surrounding patch is uniformly"
                           " flat within a given tolerance distance. Border and non-manifold vertices"
                           " are not removed, maintaining surface topology."
                           " The 'flat' algorithm works best on redundant, flat meshes, like those produced"
                           " by marching cubes."
                           " Choosing a small tolerance distance should result in a nearly lossless simplification,"
                           " but will only be applicable for meshes with redundant flat sections.";
    out.args.back().default_val = "flat";
    out.args.back().expected = true;
    out.args.back().examples = { "flat" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back().name = "ToleranceDistance";
    out.args.back().desc = "Needed for 'flat' algorithm."
                           " The maximum allowed surface deviation (in DICOM units; mm)"
                           " above which vertices will NOT be simplified."
                           "\n\n"
                           "Note that this number is not the same as the maximum surface deviation after"
                           " simplification, since every nearby vertex can in principle perturb the surface"
                           " up to the tolerance distance. In most practical situations, the tolerance distance"
                           " is representative of the surface deviation after simplification."
                           "\n\n"
                           "Setting this number to a value much smaller than the smallest feature should cause"
                           " effectively lossless simplification of exactly-flat patches.";
    out.args.back().default_val = "0.001";
    out.args.back().expected = true;
    out.args.back().examples = { "0.001", "1E-4", "0.5", "1.5" };


    out.args.emplace_back();
    out.args.back().name = "MinAlignAngle";
    out.args.back().desc = "Needed for 'flat' algorithm."
                           " The minimum angle (in rads) between a candidate surface and the original surface"
                           " patch's area-weighted average normal in order for the candidate surface to be"
                           " accepted."
                           "\n\n"
                           "The range is from zero to pi with zero being perfect alignment and pi (180 degrees)"
                           " accepting any surface, even if it faces away from the original."
                           "\n\n"
                           "Note that being too permissive can result in the surface folding back on itself,"
                           " resulting in (potentially) non-manifold pinches. An angle between zero and pi/2"
                           " is recommended.";
    out.args.back().default_val = "1.045";
    out.args.back().expected = true;
    out.args.back().examples = { "0.01", "0.1", "0.5", "1.0", "1.5", "3.14159" };

    return out;
}


bool SimplifySurfaceMeshes(Drover &DICOM_data,
                             const OperationArgPkg& OptArgs,
                             std::map<std::string, std::string>& /*InvocationMetadata*/,
                             const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();

    const auto MethodStr = OptArgs.getValueStr("Method").value();

    const auto ToleranceDistance = std::stod(OptArgs.getValueStr("ToleranceDistance").value());
    const auto MinAlignAngle = std::stod(OptArgs.getValueStr("MinAlignAngle").value());

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_flat = Compile_Regex("^fl?a?t?$");

    auto SMs_all = All_SMs( DICOM_data );
    auto SMs = Whitelist( SMs_all, MeshSelectionStr );

    int64_t completed = 0;
    const auto sm_count = SMs.size();
    for(auto & smp_it : SMs){

        if(std::regex_match(MethodStr, regex_flat)){
            (*smp_it)->meshes.simplify_inner_triangles(ToleranceDistance, 
                                                       MinAlignAngle);

        }else{
            throw std::invalid_argument("Method argument '"_s + MethodStr + "' is not valid. Only 'flat' is supported.");
        }

        ++completed;
        YLOGINFO("Completed " << completed << " of " << sm_count
              << " --> " << static_cast<int>(1000.0*(completed)/sm_count)/10.0 << "% done");
    }

    return true;
}
