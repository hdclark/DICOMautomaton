//RigidWarpImages.cc - A part of DICOMautomaton 2022. Written by hal clark.

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
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMathIOOFF.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "../Alignment_Rigid.h"
#include "../Alignment_TPSRPM.h"
#include "../Alignment_Field.h"

#include "RigidWarpImages.h"

OperationDoc OpArgDocRigidWarpImages(){
    OperationDoc out;
    out.name = "RigidWarpImages";

    out.desc = 
        "This operation applies a rigid transform object to the specified image arrays, warping"
        " (i.e., rotating, scaling, and translating) them spatially.";
        
    out.notes.emplace_back(
        "A transform object must be selected; this operation cannot create transforms."
        " Transforms can be generated via registration or by parsing user-provided functions."
    );
    out.notes.emplace_back(
        "Images are transformed in-place. Metadata may become invalid by this operation."
    );
    out.notes.emplace_back(
        "This operation can only handle individual transforms. If multiple, sequential transforms"
        " are required, this operation must be invoked multiple time. This will guarantee the"
        " ordering of the transforms."
    );
    out.notes.emplace_back(
        "This operation supports only the rigid subset of affine transformations."
        " Local transformations and those requiring shear require special handling"
        " and voxel resampling that is not yet implemented."
    );
    out.notes.emplace_back(
        "Transformations are not (generally) restricted to the coordinate frame of reference that they were"
        " derived from. This permits a single transformation to be applicable to point clouds, surface meshes,"
        " images, and contours."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back() = T3WhitelistOpArgDoc();
    out.args.back().name = "TransformSelection";
    out.args.back().default_val = "last";
 
    return out;
}


bool RigidWarpImages(Drover &DICOM_data,
                       const OperationArgPkg& OptArgs,
                       std::map<std::string, std::string>& /*InvocationMetadata*/,
                       const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto TFormSelectionStr = OptArgs.getValueStr("TransformSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    YLOGINFO("Selected " << IAs.size() << " image arrays");

    auto T3s_all = All_T3s( DICOM_data );
    auto T3s = Whitelist( T3s_all, TFormSelectionStr );
    YLOGINFO("Selected " << T3s.size() << " transformation objects");
    if(T3s.size() != 1){
        // I can't think of a better way to handle the ordering of multiple transforms right now. Disallowing for now...
        throw std::invalid_argument("Selection of only a single transformation is currently supported. Refusing to continue.");
    }


    for(auto & iap_it : IAs){
        for(auto & t3p_it : T3s){

            std::visit([&](auto && t){
                using V = std::decay_t<decltype(t)>;
                if constexpr (std::is_same_v<V, std::monostate>){
                    throw std::invalid_argument("Transformation is invalid. Unable to continue.");

                // Affine transformations.
                }else if constexpr (std::is_same_v<V, affine_transform<double>>){
                    YLOGINFO("Applying affine transformation now");

                    for(auto & animg : (*iap_it)->imagecoll.images){

                        // We have to decompose the orientation vectors from the position vectors, since the orientation
                        // vectors can only be operated on by the rotational part of the Affine transform.
                        affine_transform<double> rot_scale_only = t;
                        rot_scale_only.coeff(0,3) = 0.0;
                        rot_scale_only.coeff(1,3) = 0.0;
                        rot_scale_only.coeff(2,3) = 0.0;

                        auto new_offset = animg.offset;
                        t.apply_to(new_offset);

                        auto new_r_axis = animg.row_unit.unit() * animg.pxl_dx;
                        auto new_c_axis = animg.col_unit.unit() * animg.pxl_dy;
                        auto new_o_axis = animg.row_unit.Cross( animg.col_unit ).unit() * animg.pxl_dz;

                        rot_scale_only.apply_to(new_r_axis);
                        rot_scale_only.apply_to(new_c_axis);
                        rot_scale_only.apply_to(new_o_axis);

                        const auto new_pxl_dx = new_r_axis.length();
                        const auto new_pxl_dy = new_c_axis.length();
                        const auto new_pxl_dz = new_o_axis.length();

                        const auto new_r_unit = new_r_axis.unit();
                        const auto new_c_unit = new_c_axis.unit();
                        const auto new_o_unit = new_o_axis.unit();

                        const auto eps = std::sqrt( std::numeric_limits<double>::epsilon() );
                        if( (eps < new_r_unit.Dot(new_c_unit))
                        ||  (eps < new_c_unit.Dot(new_o_unit))
                        ||  (eps < new_o_unit.Dot(new_r_unit)) ){
                            throw std::invalid_argument("Affine transformation includes shear. Refusing to continue.");
                            // Shear will require voxel resampling. This is not currently supported.
                            // To implement, Gram-schmidt orthogonalize the new units, make another image out of them,
                            // and then sample voxels into the square image.
                        }

                        animg.init_orientation( new_r_unit, new_c_unit );
                        animg.init_spatial( new_pxl_dx,
                                            new_pxl_dy,
                                            new_pxl_dz,
                                            animg.anchor, // Anchor is tied to the underlying space, not a specific object.
                                            new_offset );
                    }

                // Thin-plate spline transformations.
                }else if constexpr (std::is_same_v<V, thin_plate_spline>){
                    throw std::invalid_argument("TPS transformations cannot be converted to a rigid transformation. Unable to continue.");

                // Deformation field transformations.
                }else if constexpr (std::is_same_v<V, deformation_field>){
                    throw std::invalid_argument("Deformation field transformations cannot be converted to a rigid transformation. Unable to continue.");

                }else{
                    static_assert(std::is_same_v<V,void>, "Transformation not understood.");
                }
                return;
            }, (*t3p_it)->transform);
        }

        // TODO: re-compute image metadata to reflect the transformation.
        //
        // Note: This should be a standalone function that operates on an Image_Array or equivalent.
        //       It should re-compute afresh all metadata using the current planar_image data members.
    }

    return true;
}
