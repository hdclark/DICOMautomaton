//SubtractImages.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <optional>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Transform/Subtract_Spatially_Overlapping_Images.h"
#include "SubtractImages.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocSubtractImages(){
    OperationDoc out;
    out.name = "SubtractImages";
    out.tags.emplace_back("category: image processing");

    out.desc = 
        "This routine subtracts images that spatially overlap.";

    out.notes.emplace_back(
        "The ReferenceImageSelection is subtracted from the ImageSelection and the result is stored"
        " in ImageSelection. So this operation implements $A = A - B$ where A is ImageSelection and"
        " B is ReferenceImageSelection. The ReferenceImageSelection images are not altered."
    );
    out.notes.emplace_back(
        "Multiple image volumes can be selected by both ImageSelection and ReferenceImageSelection."
        " For each ImageSelection volume, each of the ReferenceImageSelection volumes are subtracted"
        " sequentially."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ReferenceImageSelection";
    out.args.back().default_val = "!last";

    return out;
}

bool SubtractImages(Drover &DICOM_data,
                      const OperationArgPkg& OptArgs,
                      std::map<std::string, std::string>& /*InvocationMetadata*/,
                      const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto ReferenceImageSelectionStr = OptArgs.getValueStr("ReferenceImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    auto RIAs_all = All_IAs( DICOM_data );
    auto RIAs = Whitelist( RIAs_all, ReferenceImageSelectionStr );

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){

        for(auto & riap_it : RIAs){

            std::list<std::reference_wrapper<planar_image_collection<float,double>>> external_imgs;
            external_imgs.push_back( std::ref( (*riap_it)->imagecoll ) );

            if(!(*iap_it)->imagecoll.Transform_Images( SubtractSpatiallyOverlappingImages, 
                                                       std::move(external_imgs), {} )){
                throw std::runtime_error("Unable to subtract images.");
            }
        }
    }

    return true;
}
