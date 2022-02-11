//WarpPoints.cc - A part of DICOMautomaton 2021. Written by hal clark.

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
#include <variant>

#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Structs.h"
#include "../Alignment_Rigid.h"
#include "../Alignment_TPSRPM.h"
#include "../Alignment_Field.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "WarpPoints.h"

OperationDoc OpArgDocWarpPoints(){
    OperationDoc out;
    out.name = "WarpPoints";

    out.desc = 
        "This operation applies a transform object to the specified point clouds, warping them spatially.";
        
    out.notes.emplace_back(
        "A transform object must be selected; this operation cannot create transforms."
        " Transforms can be generated via registration or by parsing user-provided functions."
    );
    out.notes.emplace_back(
        "Point clouds are transformed in-place. Metadata may become invalid by this operation."
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
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "PointSelection";
    out.args.back().default_val = "last";
    out.args.back().desc = "The point cloud that will be transformed. "_s
                         + out.args.back().desc;

    out.args.emplace_back();
    out.args.back() = T3WhitelistOpArgDoc();
    out.args.back().name = "TransformSelection";
    out.args.back().default_val = "last";
    out.args.back().desc = "The transformation that will be applied. "_s
                         + out.args.back().desc;

    return out;
}



bool WarpPoints(Drover &DICOM_data,
                  const OperationArgPkg& OptArgs,
                  std::map<std::string, std::string>& /*InvocationMetadata*/,
                  const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto PointSelectionStr = OptArgs.getValueStr("PointSelection").value();
    const auto TFormSelectionStr = OptArgs.getValueStr("TransformSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    auto PCs_all = All_PCs( DICOM_data );
    auto PCs = Whitelist( PCs_all, PointSelectionStr );
    FUNCINFO("Selected " << PCs.size() << " point clouds");

    auto T3s_all = All_T3s( DICOM_data );
    auto T3s = Whitelist( T3s_all, TFormSelectionStr );
    FUNCINFO("Selected " << T3s.size() << " transformation objects");
    if(T3s.size() != 1){
        throw std::invalid_argument("Only a single transformation must be selected to guarantee ordering. Cannot continue.");
    }

    for(auto & pcp_it : PCs){
        FUNCINFO("Processing a point cloud with " << (*pcp_it)->pset.points.size() << " points");
        for(auto & t3p_it : T3s){

            // Apply transformation.
            std::visit([&](auto && t){
                using V = std::decay_t<decltype(t)>;
                if constexpr (std::is_same_v<V, std::monostate>){
                    throw std::invalid_argument("Transformation is invalid. Unable to continue.");

                // Affine transformations.
                }else if constexpr (std::is_same_v<V, affine_transform<double>>){
                    FUNCINFO("Applying affine transformation now");
                    t.apply_to((*pcp_it)->pset);
                    (*pcp_it)->pset.metadata["Description"] = "Warped via affine transform";

                // TPS transformations.
                }else if constexpr (std::is_same_v<V, thin_plate_spline>){
                    FUNCINFO("Applying thin plate spline transformation now");
                    t.apply_to((*pcp_it)->pset);
                    (*pcp_it)->pset.metadata["Description"] = "Warped via thin-plate spline transform";

                // Vector deformation field transformations.
                }else if constexpr (std::is_same_v<V, deformation_field>){
                    FUNCINFO("Applying vector deformation field transformation now");
                    t.apply_to((*pcp_it)->pset);
                    (*pcp_it)->pset.metadata["Description"] = "Warped via vector deformation field transform";

                }else{
                    static_assert(std::is_same_v<V,void>, "Transformation not understood.");
                }
                return;
            }, (*t3p_it)->transform);
        }
    }
 
    return true;
}
