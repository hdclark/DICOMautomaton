//ConvertWarpToImage.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Metadata.h"
#include "../Alignment_Rigid.h"
#include "../Alignment_TPSRPM.h"
#include "../Alignment_Field.h"

#include "ConvertWarpToImage.h"


OperationDoc OpArgDocConvertWarpToImage(){
    OperationDoc out;
    out.name = "ConvertWarpToImage";
    out.aliases.emplace_back("ConvertTransformToImage");

    out.tags.emplace_back("category: image processing");
    out.tags.emplace_back("category: spatial transform processing");

    out.desc = 
        "This operation attempts to convert a warp (i.e., a spatial registration or deformable spatial registration)"
        " to an image array suitable for viewing or inspecting the geometry.";

    out.args.emplace_back();
    out.args.back() = T3WhitelistOpArgDoc();
    out.args.back().name = "TransformSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back() = MetadataInjectionOpArgDoc();
    out.args.back().name = "KeyValues";
    out.args.back().default_val = "";

    return out;
}

bool ConvertWarpToImage(Drover &DICOM_data,
                          const OperationArgPkg& OptArgs,
                          std::map<std::string, std::string>& /*InvocationMetadata*/,
                          const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TFormSelectionStr = OptArgs.getValueStr("TransformSelection").value();

    const auto KeyValuesOpt = OptArgs.getValueStr("KeyValues");
    //-----------------------------------------------------------------------------------------------------------------
    // Parse user-provided metadata, if any has been provided.
    const auto key_values = parse_key_values(KeyValuesOpt.value_or(""));

    auto T3s_all = All_T3s( DICOM_data );
    auto T3s = Whitelist( T3s_all, TFormSelectionStr );
    YLOGINFO(T3s.size() << " transformations selected");

    for(auto & t3p_it : T3s){
        std::visit([&](auto && t){
            using V = std::decay_t<decltype(t)>;
            if constexpr (std::is_same_v<V, std::monostate>){
                throw std::invalid_argument("Transformation is invalid. Unable to continue.");

            // Affine transformations.
            }else if constexpr (std::is_same_v<V, affine_transform<double>>){
                YLOGINFO("Converting affine transformation now");
                std::runtime_error("Not yet implemented");

            // Thin-plate spline transformations.
            }else if constexpr (std::is_same_v<V, thin_plate_spline>){
                YLOGINFO("Converting thin-plate spline transformation now");
                std::runtime_error("Not yet implemented");

            // Vector deformation fields.
            }else if constexpr (std::is_same_v<V, deformation_field>){
                YLOGINFO("Converting vector deformation field now");

                auto out = std::make_unique<Image_Array>();
                auto l_meta = coalesce_metadata_for_basic_mr_image({});
                for(const auto &img : t.get_imagecoll_crefw().get().images){
                    out->imagecoll.images.emplace_back( planar_image<float,double>().cast_from<double>(img) );

                    // Insert a copy of the user-provided key-values, but pre-process to replace macros and evaluate known
                    // functions. User-provided values should override the generic values.
                    auto l_l_meta = l_meta;
                    auto l_key_values = key_values;
                    l_key_values.merge(l_l_meta);
                    inject_metadata( out->imagecoll.images.back().metadata, std::move(l_key_values) );

                    l_meta = coalesce_metadata_for_basic_mr_image(l_meta, meta_evolve::iterate);
                }
                DICOM_data.image_data.push_back( std::move( out ) );

            }else{
                static_assert(std::is_same_v<V,void>, "Transformation not understood.");
            }
            return;
        }, (*t3p_it)->transform);
    }
 

    return true;
}
