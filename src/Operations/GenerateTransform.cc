//GenerateTransform.cc - A part of DICOMautomaton 2021. Written by hal clark.

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

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "GenerateTransform.h"


OperationDoc OpArgDocGenerateTransform(){
    OperationDoc out;
    out.name = "GenerateTransform";

    out.desc = 
        "This operation can be used to create a transformation object. The transformation object can later"
        " be applied to objects with spatial extent.";

    out.args.emplace_back();
    out.args.back().name = "Transforms";
    out.args.back().desc = "This parameter is used to specify one or more transformations."
                           " Current primitives include translation, scaling, mirroring, and rotation."
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
                           " The mirror transformation has six configurable scalar parameters denoting an oriented"
                           " plane about which a mirror is performed."
                           " Mirroring in the plane that intersects $(1,2,3)$ and has a normal toward $(1,0,0)$"
                           " can be specified as"
                           " 'mirror(1,2,3, 1,0,0)'."
                           " Rotations around an arbitrary axis line can be accomplished."
                           " The rotation transformation has seven configurable scalar parameters denoting"
                           " the rotation centre 3-vector, the rotation axis 3-vector, and the rotation angle"
                           " in radians. A rotation of pi radians around the axis line parallel to vector"
                           " $(1.0, 0.0, 0.0)$ that intersects the point $(4.0, 5.0, 6.0)$ can be specified"
                           " as 'rotate(4.0, 5.0, 6.0,  1.0, 0.0, 0.0,  3.141592653)'."
                           " A transformation can be composed of one or more primitive transformations"
                           " applied sequentially."
                           " Primitives can be separated by a ';' and are evaluated from left to right.";
    out.args.back().default_val = "translate(0.0, 0.0, 0.0)";
    out.args.back().expected = true;
    out.args.back().examples = { "translate(1.0, -2.0, 0.3)",
                                 "scale(1.23, -2.34, 3.45, 2.7)",
                                 "mirror(0,0,0, 1,0,0)",
                                 "rotate(4.0, 5.0, 6.0,  1.0, 0.0, 0.0,  3.141592653)",
                                 "translate(1,0,0) ; scale(0,0,0, 5) ; translate(-1,0,0)" };


    out.args.emplace_back();
    out.args.back().name = "TransformLabel";
    out.args.back().desc = "A label to attach to the transformation.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "offset", "expansion", "rotation_around_xyz", "move_to_origin" };


    out.args.emplace_back();
    out.args.back().name = "Metadata";
    out.args.back().desc = "A semicolon-separated list of 'key@value' metadata to imbue into the transform."
                           " This metadata will overwrite any existing keys with the provided values.";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "keyA@valueA;keyB@valueB" };


    return out;
}



Drover GenerateTransform(Drover DICOM_data,
                         const OperationArgPkg& OptArgs,
                         const std::map<std::string, std::string>& /*InvocationMetadata*/,
                         const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TransformsStr = OptArgs.getValueStr("Transforms").value();
    const auto TransformLabel = OptArgs.getValueStr("TransformLabel").value();
    const auto MetadataOpt = OptArgs.getValueStr("Metadata");

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_trn = Compile_Regex("^tr?a?n?s?l?a?t?e?.*$");
    const auto regex_scl = Compile_Regex("^sc?a?l?e?.*$");
    const auto regex_mir = Compile_Regex("^mi?r?r?o?r?.*$");
    const auto regex_rot = Compile_Regex("^ro?t?a?t?.*$");

    const vec3<double> vec3_nan( std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::quiet_NaN() );


    // Parse user-provided metadata.
    std::map<std::string, std::string> Metadata;
    if(MetadataOpt){
        // First, check if this is a multi-key@value statement.
        const auto regex_split = Compile_Regex("^.*;.*$");
        const auto ismulti = std::regex_match(MetadataOpt.value(), regex_split);

        std::vector<std::string> v_kvs;
        if(!ismulti){
            v_kvs.emplace_back(MetadataOpt.value());
        }else{
            v_kvs = SplitStringToVector(MetadataOpt.value(), ';', 'd');
            if(v_kvs.size() < 1){
                throw std::logic_error("Unable to separate multiple key@value tokens");
            }
        }

        // Now attempt to parse each key@value statement.
        for(auto & k_at_v : v_kvs){
            const auto regex_split = Compile_Regex("^.*@.*$");
            const auto iskv = std::regex_match(k_at_v, regex_split);
            if(!iskv){
                throw std::logic_error("Unable to parse key@value token: '"_s + k_at_v + "'. Refusing to continue.");
            }
            
            auto v_k_v = SplitStringToVector(k_at_v, '@', 'd');
            if(v_k_v.size() <= 1) throw std::logic_error("Unable to separate key@value specifier");
            if(v_k_v.size() != 2) break; // Not a key@value statement (hint: maybe multiple @'s present?).
            Metadata[v_k_v.front()] = v_k_v.back();
        }
    }

    const auto extract_function_parameters = [](const std::string &in) -> std::vector<double> {
            // This routine extracts numerical function parameters.
            // Input should look like 'func(1.0, 2.0,3.0, -1.23, ...)'.
            auto split = SplitStringToVector(in, '(', 'd');
            split = SplitVector(split, ')', 'd');
            split = SplitVector(split, ',', 'd');
            split = SplitVector(split, ' ', 'd');

            std::vector<double> numbers;
            for(const auto &w : split){
               if(w.empty()) continue;
               try{
                   const auto x = std::stod(w);
                   numbers.emplace_back(x);
               }catch(const std::exception &){ }
            }
            return numbers;
    };

    //-----------------------------------------------------------------------------------------------------------------

    const auto user_transform_strs = SplitStringToVector( PurgeCharsFromString(TransformsStr, " "), ';', 'd');
    if(user_transform_strs.empty()){
        throw std::invalid_argument("No transformations specified. Refusing to continue.");
    }
    FUNCINFO("Processing " << user_transform_strs.size() << " transformations");

    affine_transform<double> final_affine;
    for(const auto& trans_str : user_transform_strs){


        // Translations.
        if(std::regex_match(trans_str, regex_trn)){
            auto numbers = extract_function_parameters(trans_str);
            if(numbers.size() != 3){
                throw std::invalid_argument("Unable to parse translation parameters. Cannot continue.");
            }
            const auto Tr = vec3<double>( numbers.at(0),
                                          numbers.at(1),
                                          numbers.at(2) );
            if(!Tr.isfinite()) throw std::invalid_argument("Translation vector invalid. Cannot continue.");

            affine_transform<double> l_affine;
            l_affine.coeff(0,3) = Tr.x;
            l_affine.coeff(1,3) = Tr.y;
            l_affine.coeff(2,3) = Tr.z;
            final_affine = static_cast<affine_transform<double>>(
                               static_cast<num_array<double>>(final_affine)
                             * static_cast<num_array<double>>(l_affine) );

        // Scaling.
        }else if(std::regex_match(trans_str, regex_scl)){
            auto numbers = extract_function_parameters(trans_str);
            if(numbers.size() != 4){
                throw std::invalid_argument("Unable to parse scale parameters. Cannot continue.");
            }
            const auto centre = vec3<double>( numbers.at(0),
                                              numbers.at(1),
                                              numbers.at(2) );
            const auto factor = numbers.at(3);
            if(!centre.isfinite()) throw std::invalid_argument("Scale centre invalid. Cannot continue.");
            if(!std::isfinite(factor)) throw std::invalid_argument("Scale factor invalid. Cannot continue.");

            affine_transform<double> shift;
            shift.coeff(0,3) = centre.x * -1.0;
            shift.coeff(1,3) = centre.y * -1.0;
            shift.coeff(2,3) = centre.z * -1.0;

            affine_transform<double> scale;
            scale.coeff(0,0) = factor;
            scale.coeff(1,1) = factor;
            scale.coeff(2,2) = factor;

            affine_transform<double> shift_back;
            shift_back.coeff(0,3) = centre.x;
            shift_back.coeff(1,3) = centre.y;
            shift_back.coeff(2,3) = centre.z;

            final_affine = static_cast<affine_transform<double>>(
                               static_cast<num_array<double>>(final_affine)
                             * static_cast<num_array<double>>(shift_back)
                             * static_cast<num_array<double>>(scale)
                             * static_cast<num_array<double>>(shift) );

        // Mirroring.
        }else if(std::regex_match(trans_str, regex_mir)){
            auto numbers = extract_function_parameters(trans_str);
            if(numbers.size() != 6){
                throw std::invalid_argument("Unable to parse mirror parameters. Cannot continue.");
            }
            const auto centre = vec3<double>( numbers.at(0),
                                              numbers.at(1),
                                              numbers.at(2) );
            const auto normal = vec3<double>( numbers.at(3),
                                              numbers.at(4),
                                              numbers.at(5) ).unit();
            if(!centre.isfinite()) throw std::invalid_argument("Mirror centre invalid. Cannot continue.");
            if(!normal.isfinite()) throw std::invalid_argument("Mirror normal invalid. Cannot continue.");

            affine_transform<double> shift;
            shift.coeff(0,3) = centre.x * -1.0;
            shift.coeff(1,3) = centre.y * -1.0;
            shift.coeff(2,3) = centre.z * -1.0;

            // This is the Householder transformation.
            affine_transform<double> mirror;
            mirror.coeff(0,0) = 1.0 - 2.0 * normal.x * normal.x;
            mirror.coeff(1,0) = 0.0 - 2.0 * normal.x * normal.y;
            mirror.coeff(2,0) = 0.0 - 2.0 * normal.x * normal.z;

            mirror.coeff(0,1) = 0.0 - 2.0 * normal.y * normal.x;
            mirror.coeff(1,1) = 1.0 - 2.0 * normal.y * normal.y;
            mirror.coeff(2,1) = 0.0 - 2.0 * normal.y * normal.z;

            mirror.coeff(0,2) = 0.0 - 2.0 * normal.z * normal.x;
            mirror.coeff(1,2) = 0.0 - 2.0 * normal.z * normal.y;
            mirror.coeff(2,2) = 1.0 - 2.0 * normal.z * normal.z;

            affine_transform<double> shift_back;
            shift_back.coeff(0,3) = centre.x;
            shift_back.coeff(1,3) = centre.y;
            shift_back.coeff(2,3) = centre.z;

            final_affine = static_cast<affine_transform<double>>(
                               static_cast<num_array<double>>(final_affine)
                             * static_cast<num_array<double>>(shift_back)
                             * static_cast<num_array<double>>(mirror)
                             * static_cast<num_array<double>>(shift) );

        // Rotations.
        }else if(std::regex_match(trans_str, regex_rot)){
            auto numbers = extract_function_parameters(trans_str);
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

            affine_transform<double> shift;
            shift.coeff(0,3) = centre.x * -1.0;
            shift.coeff(1,3) = centre.y * -1.0;
            shift.coeff(2,3) = centre.z * -1.0;

            // Rotation matrix for an arbitrary rotation around unit vector at origin.
            const auto s = std::sin(angle);
            const auto c = std::cos(angle);
            affine_transform<double> rotate;
            rotate.coeff(0,0) = ((1.0 - c) * axis.x * axis.x) + c;
            rotate.coeff(1,0) = ((1.0 - c) * axis.y * axis.x) + (s * axis.z);
            rotate.coeff(2,0) = ((1.0 - c) * axis.z * axis.x) - (s * axis.y);
            rotate.coeff(0,1) = ((1.0 - c) * axis.x * axis.y) - (s * axis.z);
            rotate.coeff(1,1) = ((1.0 - c) * axis.y * axis.y) + c;
            rotate.coeff(2,1) = ((1.0 - c) * axis.z * axis.y) + (s * axis.x);
            rotate.coeff(0,2) = ((1.0 - c) * axis.x * axis.z) + (s * axis.y);
            rotate.coeff(1,2) = ((1.0 - c) * axis.y * axis.z) - (s * axis.x);
            rotate.coeff(2,2) = ((1.0 - c) * axis.z * axis.z) + c;

            affine_transform<double> shift_back;
            shift_back.coeff(0,3) = centre.x;
            shift_back.coeff(1,3) = centre.y;
            shift_back.coeff(2,3) = centre.z;

            final_affine = static_cast<affine_transform<double>>(
                               static_cast<num_array<double>>(final_affine)
                             * static_cast<num_array<double>>(shift_back)
                             * static_cast<num_array<double>>(rotate)
                             * static_cast<num_array<double>>(shift) );

        }else{
            throw std::invalid_argument("Transformation '"_s + trans_str + "' not understood. Cannot continue.");
        }

    }

    //FUNCINFO("final_affine = ");
    //final_affine.write_to(std::cout);

    DICOM_data.trans_data.emplace_back( std::make_shared<Transform3>( ) );
    DICOM_data.trans_data.back()->transform = final_affine;
    DICOM_data.trans_data.back()->metadata["TransformLabel"] = TransformLabel;

    // Insert user-specified metadata.
    //
    // Note: This must occur last so it overwrites incumbent metadata entries.
    for(const auto &kvp : Metadata){
        DICOM_data.trans_data.back()->metadata[kvp.first] = kvp.second;
    }

    return DICOM_data;
}
