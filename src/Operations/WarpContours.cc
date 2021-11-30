//WarpContours.cc - A part of DICOMautomaton 2021. Written by hal clark.

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

#include "WarpContours.h"

OperationDoc OpArgDocWarpContours(){
    OperationDoc out;
    out.name = "WarpContours";

    out.desc = 
        "This operation applies a transform object to the specified contours, warping them spatially.";
        
    out.notes.emplace_back(
        "A transform object must be selected; this operation cannot create transforms."
        " Transforms can be generated via registration or by parsing user-provided functions."
    );
    out.notes.emplace_back(
        "Contours are transformed in-place. Metadata may become invalid by this operation."
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
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = T3WhitelistOpArgDoc();
    out.args.back().name = "TransformSelection";
    out.args.back().default_val = "last";
 
    return out;
}

bool WarpContours(Drover &DICOM_data,
                    const OperationArgPkg& OptArgs,
                    std::map<std::string, std::string>& /*InvocationMetadata*/,
                    const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();

    const auto TFormSelectionStr = OptArgs.getValueStr("TransformSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }
    FUNCINFO("Selected " << cc_ROIs.size() << " contours");


    auto T3s_all = All_T3s( DICOM_data );
    auto T3s = Whitelist( T3s_all, TFormSelectionStr );
    FUNCINFO("Selected " << T3s.size() << " transformation objects");
    if(T3s.size() != 1){
        // I can't think of a better way to handle the ordering of multiple transforms right now. Disallowing for now...
        throw std::invalid_argument("Selection of only a single transformation is currently supported. Refusing to continue.");
    }


    for(auto & cc_refw : cc_ROIs){
        for(auto & t3p_it : T3s){

            std::visit([&](auto && t){
                using V = std::decay_t<decltype(t)>;
                if constexpr (std::is_same_v<V, std::monostate>){
                    throw std::invalid_argument("Transformation is invalid. Unable to continue.");

                // Affine transformations.
                }else if constexpr (std::is_same_v<V, affine_transform<double>>){
                    FUNCINFO("Applying affine transformation now");
                    for(auto &c : cc_refw.get().contours){
                        for(auto &v : c.points){
                            t.apply_to(v);
                        }
                    }

                // Thin-plate spline transformations.
                }else if constexpr (std::is_same_v<V, thin_plate_spline>){
                    FUNCINFO("Applying thin-plate spline transformation now");
                    for(auto &c : cc_refw.get().contours){
                        for(auto &v : c.points){
                            t.apply_to(v);
                        }
                    }

                // Deformation field transformations.
                }else if constexpr (std::is_same_v<V, deformation_field>){
                    FUNCINFO("Applying deformation field transformation now");
                    for(auto &c : cc_refw.get().contours){
                        for(auto &v : c.points){
                            t.apply_to(v);
                        }
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
