//WarpMeshes.cc - A part of DICOMautomaton 2021. Written by hal clark.

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

#include "../Alignment_Rigid.h"
#include "../Alignment_TPSRPM.h"
#include "../Alignment_Field.h"

#include "WarpMeshes.h"

OperationDoc OpArgDocWarpMeshes(){
    OperationDoc out;
    out.name = "WarpMeshes";

    out.desc = 
        "This operation applies a transform object to the specified surface meshes, warping them spatially.";
        
    out.notes.emplace_back(
        "A transform object must be selected; this operation cannot create transforms."
        " Transforms can be generated via registration or by parsing user-provided functions."
    );
    out.notes.emplace_back(
        "Meshes are transformed in-place. Metadata may become invalid by this operation."
    );
    out.notes.emplace_back(
        "This operation can only handle individual transforms. If multiple, sequential transforms"
        " are required, this operation must be invoked multiple time. This will guarantee the"
        " ordering of the transforms."
    );
    out.notes.emplace_back(
        "Transformations are not (generally) restricted to the coordinate frame of reference that they were"
        " derived from. This permits a single transformation to be applicable to point clouds, surface meshes,"
        " images, and contours."
    );

    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";
 
    out.args.emplace_back();
    out.args.back() = T3WhitelistOpArgDoc();
    out.args.back().name = "TransformSelection";
    out.args.back().default_val = "last";
 
    return out;
}

bool WarpMeshes(Drover &DICOM_data,
                  const OperationArgPkg& OptArgs,
                  std::map<std::string, std::string>& /*InvocationMetadata*/,
                  const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();

    const auto TFormSelectionStr = OptArgs.getValueStr("TransformSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    auto SMs_all = All_SMs( DICOM_data );
    auto SMs = Whitelist( SMs_all, MeshSelectionStr );
    const auto sm_count = SMs.size();
    FUNCINFO("Selected " << sm_count << " meshes");


    auto T3s_all = All_T3s( DICOM_data );
    auto T3s = Whitelist( T3s_all, TFormSelectionStr );
    FUNCINFO("Selected " << T3s.size() << " transformation objects");
    if(T3s.size() != 1){
        // I can't think of a better way to handle the ordering of multiple transforms right now. Disallowing for now...
        throw std::invalid_argument("Selection of only a single transformation is currently supported. Refusing to continue.");
    }


    for(auto & smp_it : SMs){
        for(auto & t3p_it : T3s){

            std::visit([&](auto && t){
                using V = std::decay_t<decltype(t)>;
                if constexpr (std::is_same_v<V, std::monostate>){
                    throw std::invalid_argument("Transformation is invalid. Unable to continue.");

                // Affine transformations.
                }else if constexpr (std::is_same_v<V, affine_transform<double>>){
                    FUNCINFO("Applying affine transformation now");
                    for(auto &v : (*smp_it)->meshes.vertices){
                        t.apply_to(v);
                    }

                // Thin-plate spline transformations.
                }else if constexpr (std::is_same_v<V, thin_plate_spline>){
                    FUNCINFO("Applying thin-plate spline transformation now");
                    for(auto &v : (*smp_it)->meshes.vertices){
                        t.apply_to(v);
                    }

                // Deformation field transformations.
                }else if constexpr (std::is_same_v<V, deformation_field>){
                    FUNCINFO("Applying deformation field transformation now");
                    for(auto &v : (*smp_it)->meshes.vertices){
                        t.apply_to(v);
                    }

                }else{
                    static_assert(std::is_same_v<V,void>, "Transformation not understood.");
                }
                return;
            }, (*t3p_it)->transform);
        }
    }

    return true;
}

