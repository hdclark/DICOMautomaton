//ConvertImageToWarp.cc - A part of DICOMautomaton 2021. Written by hal clark.

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

#include "ConvertImageToWarp.h"


OperationDoc OpArgDocConvertImageToWarp(){
    OperationDoc out;
    out.name = "ConvertImageToWarp";

    out.desc = 
        "This operation attempts to convert an image array into a warp"
        " (i.e., a spatial registration or deformable spatial registration).";

    out.notes.emplace_back(
        "This operation creates a deformation field transformation."
        " The input images are required to have three channels and be regular."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back() = MetadataInjectionOpArgDoc();
    out.args.back().name = "KeyValues";
    out.args.back().default_val = "";

    return out;
}

bool ConvertImageToWarp(Drover &DICOM_data,
                          const OperationArgPkg& OptArgs,
                          std::map<std::string, std::string>& /*InvocationMetadata*/,
                          const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto KeyValuesOpt = OptArgs.getValueStr("KeyValues");
    //-----------------------------------------------------------------------------------------------------------------
    // Parse user-provided metadata, if any has been provided.
    const auto key_values = parse_key_values(KeyValuesOpt.value_or(""));

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    FUNCINFO(IAs.size() << " images selected");

    for(auto & iap_it : IAs){
        FUNCINFO("Converting image array to deformation field");
        planar_image_collection<double,double> pic;
        for(const auto &img : (*iap_it)->imagecoll.images){
            pic.images.emplace_back( planar_image<double,double>().cast_from<float>(img) );
        }

        auto out = std::make_unique<Transform3>();
        deformation_field t(std::move(pic));
        out->transform = t;

        // If we make it here without throwing, the warp was successfully created.
        inject_metadata( out->metadata, coalesce_metadata_for_basic_def_reg({}) );
        DICOM_data.trans_data.emplace_back(std::move(out));
    }

    return true;
}
