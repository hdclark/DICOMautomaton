//SpatialSharpen.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Sharpen.h"
#include "SpatialSharpen.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)


OperationDoc OpArgDocSpatialSharpen(){
    OperationDoc out;
    out.name = "SpatialSharpen";

    out.desc = 
        "This operation 'sharpens' pixels (within the plane of the image only) using the specified estimator.";

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "all";
    
    out.args.emplace_back();
    out.args.back().name = "Estimator";
    out.args.back().desc = "Controls the (in-plane) sharpening estimator to use."
                      " Options are currently: sharpen_3x3 and unsharp_mask_5x5. The latter is based"
                      " on a 5x5 Gaussian blur estimator.";
    out.args.back().default_val = "unsharp_mask_5x5";
    out.args.back().expected = true;
    out.args.back().examples = { "sharpen_3x3",
                            "unsharp_mask_5x5" };

    return out;
}

Drover SpatialSharpen(Drover DICOM_data,
                      const OperationArgPkg& OptArgs,
                      const std::map<std::string, std::string>&,
                      const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto EstimatorStr = OptArgs.getValueStr("Estimator").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_shrp3x3 = Compile_Regex("^sh?a?r?p?e?n?_?3?x?3?$");
    const auto regex_unsp5x5 = Compile_Regex("^un?s?h?a?r?p?_?m?a?s?k?_?5?x?5?$");

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        InPlaneImageSharpenUserData ud;

        if( std::regex_match(EstimatorStr, regex_shrp3x3) ){
            ud.estimator = SharpenEstimator::sharpen_3x3;
        }else if( std::regex_match(EstimatorStr, regex_unsp5x5) ){
            ud.estimator = SharpenEstimator::unsharp_mask_5x5;
        }else{
            throw std::invalid_argument("Estimator argument '"_s + EstimatorStr + "' is not valid");
        }

        if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                          InPlaneImageSharpen,
                                                          {}, {}, &ud )){
            throw std::runtime_error("Unable to compute specified sharpen estimator.");
        }
    }

    return DICOM_data;
}
