//TransformMeshes.cc - A part of DICOMautomaton 2019. Written by hal clark.

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
#include "TransformMeshes.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMathIOOFF.h"


OperationDoc OpArgDocTransformMeshes(){
    OperationDoc out;
    out.name = "TransformMeshes";

    out.desc = 
        "This operation transforms meshes by translating, scaling, and rotating vertices.";
        
    out.notes.emplace_back(
        "A single transformation can be specified at a time. Perform this operation sequentially to enforce order."
    );

    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
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



Drover TransformMeshes(Drover DICOM_data, const OperationArgPkg& OptArgs, const std::map<std::string,std::string>& /*InvocationMetadata*/, const std::string&  /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();
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


    auto SMs_all = All_SMs( DICOM_data );
    auto SMs = Whitelist( SMs_all, MeshSelectionStr );
    const auto sm_count = SMs.size();
    FUNCINFO("Selected " << sm_count << " meshes");

    long int completed = 0;
    for(auto & smp_it : SMs){

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

            // Implement the transformation.
            for(auto &v : (*smp_it)->meshes.vertices){
                v += Tr;
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

            for(auto &v : (*smp_it)->meshes.vertices){
                const auto R = v - centre;
                v = centre + (R * factor);
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

            for(auto &v : (*smp_it)->meshes.vertices){
                v = v - centre;
                v = v.rotate_around_unit(axis, angle); 
                v = v + centre;
            }

        }else{
            throw std::invalid_argument("Transformation not understood. Cannot continue.");
        }


        //// Compute some basic measures for the transformed mesh.
        //{
        //    vec3<double> avg_vert(0.0, 0.0, 0.0);
        //    for(auto &v : (*smp_it)->meshes.vertices){
        //        avg_vert += v;
        //    }
        //    avg_vert /= static_cast<double>((*smp_it)->meshes.vertices.size());
        //    FUNCINFO("Average mesh vertex position = " << avg_vert);
        //}

        ++completed;
        FUNCINFO("Completed " << completed << " of " << sm_count
              << " --> " << static_cast<int>(1000.0*(completed)/sm_count)/10.0 << "% done");
    }

    return DICOM_data;
}
