//SpatialSharpen.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <experimental/any>
#include <experimental/optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Sharpen.h"
#include "SpatialSharpen.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)


std::list<OperationArgDoc> OpArgDocSpatialSharpen(void){
    std::list<OperationArgDoc> out;

    // This operation 'sharpens' pixels (within the plane of the image only) using the specified estimator.

    out.emplace_back();
    out.back().name = "ImageSelection";
    out.back().desc = "Images to operate on. Either 'none', 'last', 'first', or 'all'.";
    out.back().default_val = "all";
    out.back().expected = true;
    out.back().examples = { "none", "last", "first", "all" };
    
    out.emplace_back();
    out.back().name = "Estimator";
    out.back().desc = "Controls the (in-plane) sharpening estimator to use."
                      " Options are currently: sharpen_3x3 and unsharp_mask_5x5. The latter is based"
                      " on a 5x5 Gaussian blur estimator.";
    out.back().default_val = "unsharp_mask";
    out.back().expected = true;
    out.back().examples = { "sharpen_3x3",
                            "unsharp_mask_5x5" };

    return out;
}

Drover SpatialSharpen(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> , std::string){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto EstimatorStr = OptArgs.getValueStr("Estimator").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_none  = std::regex("^no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_first = std::regex("^fi?r?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last  = std::regex("^la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all   = std::regex("^al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto regex_shrp3x3 = std::regex("^sh?a?r?p?e?n?_?3x?3?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_unsp5x5 = std::regex("^un?s?h?a?r?p?_?m?a?s?k?_?5x?5?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    if( !std::regex_match(ImageSelectionStr, regex_none)
    &&  !std::regex_match(ImageSelectionStr, regex_first)
    &&  !std::regex_match(ImageSelectionStr, regex_last)
    &&  !std::regex_match(ImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Image selection is not valid. Cannot continue.");
    }

    // --- Cycle over all images, performing the sharpen ---

    //Image data.
    auto iap_it = DICOM_data.image_data.begin();
    if(false){
    }else if(std::regex_match(ImageSelectionStr, regex_none)){ iap_it = DICOM_data.image_data.end();
    }else if(std::regex_match(ImageSelectionStr, regex_last)){
        if(!DICOM_data.image_data.empty()) iap_it = std::prev(DICOM_data.image_data.end());
    }
    while(iap_it != DICOM_data.image_data.end()){
        InPlaneImageSharpenUserData ud;

        if(false){
        }else if( std::regex_match(EstimatorStr, regex_shrp3x3) ){
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
        ++iap_it;
        if(std::regex_match(ImageSelectionStr, regex_first)) break;
    }

    return DICOM_data;
}
