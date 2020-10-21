//TransformImages.cc - A part of DICOMautomaton 2020. Written by hal clark.

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

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "TransformImages.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMathIOOFF.h"


OperationDoc OpArgDocTransformImages(){
    OperationDoc out;
    out.name = "TransformImages";

    out.desc = 
        "This operation transforms images by translating, scaling, and rotating the positions of voxels.";
        
    out.notes.emplace_back(
        "A single transformation can be specified at a time. Perform this operation sequentially to enforce order."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "Transform";
    out.args.back().desc = "This parameter is used to specify the transformation that should be performed."
                           " A single transformation can be specified for each invocation of this operation."
                           " Currently translation, scaling, and rotation are available."
                           " Translations have three configurable scalar parameters denoting the translation along"
                           " x, y, and z in the DICOM coordinate system."
                           " Translating $x=1.0$, $y=-2.0$, and $z=0.3$ can be specified as"
                           " 'translate(1.0, -2.0, 0.3)'."
                           " The scale transformation has four configurable scalar parameters denoting the scale"
                           " centre 3-vector and the magnification factor. Note that the magnification factor can"
                           " be negative, which will cause the mesh to be inverted along x, y, and z axes and"
                           " magnified. Take note that face orientations will also become inverted."
                           " Magnifying by 2.7x about $(1.23, -2.34, 3.45)$ can be specified as"
                           " 'scale(1.23, -2.34, 3.45, 2.7)'."
                           " Rotations around an arbitrary axis line can be accomplished."
                           " The rotation transformation has seven configurable scalar parameters denoting"
                           " the rotation centre 3-vector, the rotation axis 3-vector, and the rotation angle"
                           " in radians. A rotation of pi radians around the axis line parallel to vector"
                           " $(1.0, 0.0, 0.0)$ that intersects the point $(4.0, 5.0, 6.0)$ can be specified"
                           " as 'rotate(4.0, 5.0, 6.0,  1.0, 0.0, 0.0,  3.141592653)'.";
    out.args.back().default_val = "translate(0.0, 0.0, 0.0)";
    out.args.back().expected = true;
    out.args.back().examples = { "translate(1.0, -2.0, 0.3)",
                                 "scale(1.23, -2.34, 3.45, 2.7)",
                                 "rotate(4.0, 5.0, 6.0,  1.0, 0.0, 0.0,  3.141592653)" };

    return out;
}



Drover TransformImages(Drover DICOM_data,
                       const OperationArgPkg& OptArgs,
                       const std::map<std::string, std::string>& /*InvocationMetadata*/,
                       const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto TransformStr = OptArgs.getValueStr("Transform").value();

    //-----------------------------------------------------------------------------------------------------------------

    const auto regex_trn = Compile_Regex("^tr?a?n?s?l?a?t?e?.*$");
    const auto regex_scl = Compile_Regex("^sc?a?l?e?.*$");
    const auto regex_rot = Compile_Regex("^ro?t?a?t?.*$");

    const vec3<double> vec3_nan( std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::quiet_NaN() );

    const auto extract_function_parameters = [](const std::string &in) -> std::vector<double> {
            // This rountine extracts numerical function parameters.
            // Input should look like 'func(1.0, 2.0,3.0, -1.23, ...)'.
            auto split = SplitStringToVector(in, '(', 'd');
            split = SplitVector(split, ')', 'd');
            split = SplitVector(split, ',', 'd');

            std::vector<double> numbers;
            for(const auto &w : split){
               try{
                   const auto x = std::stod(w);
                   numbers.emplace_back(x);
               }catch(const std::exception &){ }
            }
            return numbers;
    };

    //-----------------------------------------------------------------------------------------------------------------

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    FUNCINFO("Selected " << IAs.size() << " image arrays");

    for(auto & iap_it : IAs){
        // Translations.
        if(std::regex_match(TransformStr, regex_trn)){
            auto numbers = extract_function_parameters(TransformStr);
            if(numbers.size() != 3){
                throw std::invalid_argument("Unable to parse translation parameters. Cannot continue.");
            }
            const auto Tr = vec3<double>( numbers.at(0),
                                          numbers.at(1),
                                          numbers.at(2) );
            if(!Tr.isfinite()) throw std::invalid_argument("Translation vector invalid. Cannot continue.");

            for(auto & animg : (*iap_it)->imagecoll.images){
                const auto new_offset = animg.offset + Tr;
                animg.init_spatial( animg.pxl_dx, animg.pxl_dy, animg.pxl_dz, animg.anchor, new_offset );
            }


        // Scaling.
        }else if(std::regex_match(TransformStr, regex_scl)){
            auto numbers = extract_function_parameters(TransformStr);
            if(numbers.size() != 4){
                throw std::invalid_argument("Unable to parse scale parameters. Cannot continue.");
            }
            const auto centre = vec3<double>( numbers.at(0),
                                              numbers.at(1),
                                              numbers.at(2) );
            const auto factor = numbers.at(3);
            if(!centre.isfinite()) throw std::invalid_argument("Scale centre invalid. Cannot continue.");
            if(!std::isfinite(factor)) throw std::invalid_argument("Scale factor invalid. Cannot continue.");

            for(auto & animg : (*iap_it)->imagecoll.images){
                const auto R = animg.offset - centre;
                const auto new_offset = centre + (R * factor);

                animg.init_spatial( animg.pxl_dx * factor,
                                    animg.pxl_dy * factor,
                                    animg.pxl_dz * factor,
                                    animg.anchor,
                                    new_offset );
            }

        // Rotations.
        }else if(std::regex_match(TransformStr, regex_rot)){
            auto numbers = extract_function_parameters(TransformStr);
            if(numbers.size() != 7){
                throw std::invalid_argument("Unable to parse rotation parameters. Cannot continue.");
            }
            const auto centre = vec3<double>( numbers.at(0),
                                              numbers.at(1),
                                              numbers.at(2) );
            const auto axis = vec3<double>( numbers.at(3),
                                            numbers.at(4),
                                            numbers.at(5) ).unit();
            const auto angle = numbers.at(6);
            if(!centre.isfinite()) throw std::invalid_argument("Rotation centre invalid. Cannot continue.");
            if(!axis.isfinite()) throw std::invalid_argument("Rotation axis invalid. Cannot continue.");
            if(!std::isfinite(angle)) throw std::invalid_argument("Rotation angle invalid. Cannot continue.");

            for(auto & animg : (*iap_it)->imagecoll.images){
                const auto new_offset = (animg.offset - centre).rotate_around_unit(axis, angle) + centre;

                animg.init_orientation( animg.row_unit.rotate_around_unit(axis, angle).unit(),
                                        animg.col_unit.rotate_around_unit(axis, angle).unit() );
                animg.init_spatial( animg.pxl_dx,
                                    animg.pxl_dy,
                                    animg.pxl_dz,
                                    animg.anchor,
                                    new_offset );
            }

        }else{
            throw std::invalid_argument("Transformation not understood. Cannot continue.");
        }

        // TODO: re-compute image metadata to reflect the transformation.
        //
        // Note: This should be a standalone function that operates on an Image_Array or ewquivalent.
        //       It should re-compute afresh all metadata using the current planar_image data members.
    }

    return DICOM_data;
}
