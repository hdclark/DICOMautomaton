//GenerateWarp.cc - A part of DICOMautomaton 2021. Written by hal clark.

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

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "GenerateWarp.h"


OperationDoc OpArgDocGenerateWarp(){
    OperationDoc out;
    out.name = "GenerateWarp";

    out.desc = 
        "This operation can be used to create a transformation object. The transformation object can later"
        " be applied to objects with spatial extent.";

    out.args.emplace_back();
    out.args.back().name = "Transforms";
    out.args.back().desc = "This parameter is used to specify one or more transformations."
                           " Current primitives include translation, scaling, mirroring, and rotation."
                           "\n\n"
                           "Translations have three configurable scalar parameters denoting the translation along"
                           " x, y, and z in the DICOM coordinate system."
                           " Translating $x=1.0$, $y=-2.0$, and $z=0.3$ can be specified as"
                           " 'translate(1.0, -2.0, 0.3)'."
                           "\n\n"
                           "The scale (actually 'homothetic') transformation has four configurable scalar"
                           " parameters denoting the scale centre 3-vector and the magnification factor."
                           " Note that the magnification factor can"
                           " be negative, which will cause the mesh to be inverted along x, y, and z axes and"
                           " magnified. Take note that face orientations will also become inverted."
                           " Magnifying by 2.7x about $(1.23, -2.34, 3.45)$ can be specified as"
                           " 'scale(1.23, -2.34, 3.45, 2.7)'."
                           " A standard scale transformation can be achieved by taking the centre to be the origin."
                           "\n\n"
                           "The mirror transformation has six configurable scalar parameters denoting an oriented"
                           " plane about which a mirror is performed."
                           " Mirroring in the plane that intersects $(1,2,3)$ and has a normal toward $(1,0,0)$"
                           " can be specified as 'mirror(1,2,3, 1,0,0)'."
                           "\n\n"
                           "Rotations around an arbitrary axis line can be accomplished."
                           " The rotation transformation has seven configurable scalar parameters denoting"
                           " the rotation centre 3-vector, the rotation axis 3-vector, and the rotation angle"
                           " in radians. A rotation of pi radians around the axis line parallel to vector"
                           " $(1.0, 0.0, 0.0)$ that intersects the point $(4.0, 5.0, 6.0)$ can be specified"
                           " as 'rotate(4.0, 5.0, 6.0,  1.0, 0.0, 0.0,  3.141592653)'."
                           "\n\n"
                           "A transformation can be composed of one or more primitive transformations"
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
    out.args.back().name = "TransformName";
    out.args.back().desc = "A name or label to attach to the transformation.";
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

bool GenerateWarp(Drover &DICOM_data,
                    const OperationArgPkg& OptArgs,
                    std::map<std::string, std::string>& /*InvocationMetadata*/,
                    const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TransformsStr = OptArgs.getValueStr("Transforms").value();
    const auto TransformName = OptArgs.getValueStr("TransformName").value();
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
    YLOGINFO("Processing " << user_transform_strs.size() << " transformations");

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

            final_affine = static_cast<affine_transform<double>>(
                               static_cast<num_array<double>>(final_affine)
                             * static_cast<num_array<double>>(affine_translate(Tr)) );

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

            final_affine = static_cast<affine_transform<double>>(
                               static_cast<num_array<double>>(final_affine)
                             * static_cast<num_array<double>>(affine_scale(centre, factor)) );

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
            plane<double> p(normal, centre);

            final_affine = static_cast<affine_transform<double>>(
                               static_cast<num_array<double>>(final_affine)
                             * static_cast<num_array<double>>(affine_mirror(p)) );

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

            final_affine = static_cast<affine_transform<double>>(
                               static_cast<num_array<double>>(final_affine)
                             * static_cast<num_array<double>>(affine_rotate(centre, axis, angle)) );
        }else{
            throw std::invalid_argument("Transformation '"_s + trans_str + "' not understood. Cannot continue.");
        }

    }

    //YLOGINFO("final_affine = ");
    //final_affine.write_to(std::cout);

    DICOM_data.trans_data.emplace_back( std::make_shared<Transform3>( ) );
    DICOM_data.trans_data.back()->transform = final_affine;
    DICOM_data.trans_data.back()->metadata["TransformName"] = TransformName;

    // Insert user-specified metadata.
    //
    // Note: This must occur last so it overwrites incumbent metadata entries.
    for(const auto &kvp : Metadata){
        DICOM_data.trans_data.back()->metadata[kvp.first] = kvp.second;
    }

    return true;
}
