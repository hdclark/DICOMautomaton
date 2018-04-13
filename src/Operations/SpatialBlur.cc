//SpatialBlur.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

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
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Blur.h"
#include "SpatialBlur.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)


OperationDoc OpArgDocSpatialBlur(void){
    OperationDoc out;
    out.name = "SpatialBlur";
    out.desc = "";

    out.notes.emplace_back("");


    // This operation blurs pixels (within the plane of the image only) using the specified estimator.

    out.args.emplace_back();
    out.args.back().name = "ImageSelection";
    out.args.back().desc = "Images to operate on. Either 'none', 'last', 'first', or 'all'.";
    out.args.back().default_val = "all";
    out.args.back().expected = true;
    out.args.back().examples = { "none", "last", "first", "all" };
    
    out.args.emplace_back();
    out.args.back().name = "Estimator";
    out.args.back().desc = "Controls the (in-plane) blur estimator to use."
                      " Options are currently: box_3x3, box_5x5, gaussian_3x3, gaussian_5x5, and gaussian_open."
                      " The latter (gaussian_open) is adaptive and requires a supplementary parameter that controls"
                      " the number of adjacent pixels to consider. The former ('...3x3' and '...5x5') are 'fixed'"
                      " estimators that use a convolution kernel with a fixed size (3x3 or 5x5 pixel neighbourhoods)."
                      " All estimators operate in 'pixel-space' and are ignorant about the image spatial extent."
                      " All estimators are normalized, and thus won't significantly affect the pixel magnitude scale.";
    out.args.back().default_val = "gaussian_open";
    out.args.back().expected = true;
    out.args.back().examples = { "box_3x3",
                            "box_5x5",
                            "gaussian_3x3",
                            "gaussian_5x5",
                            "gaussian_open" };

    out.args.emplace_back();
    out.args.back().name = "GaussianOpenSigma";
    out.args.back().desc = "Controls the number of neighbours to consider (only) when using the gaussian_open estimator."
                      " The number of pixels is computed automatically to accommodate the specified sigma"
                      " (currently ignored pixels have 3*sigma or less weighting). Be aware this operation can take"
                      " an enormous amount of time, since the pixel neighbourhoods quickly grow large.";
    out.args.back().default_val = "1.5";
    out.args.back().expected = true;
    out.args.back().examples = { "0.5",
                            "1.0",
                            "1.5",
                            "2.5",
                            "5.0" };

    return out;
}

Drover SpatialBlur(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> , std::string){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto EstimatorStr = OptArgs.getValueStr("Estimator").value();
    const auto GaussianOpenSigma = std::stod( OptArgs.getValueStr("GaussianOpenSigma").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_none  = std::regex("^no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_first = std::regex("^fi?r?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last  = std::regex("^la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all   = std::regex("^al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto regex_box3x3 = std::regex("^bo?x?_?3x?3?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_box5x5 = std::regex("^bo?x?_?5x?5?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_gau3x3 = std::regex("^ga?u?s?s?i?a?n?_?3x?3?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_gau5x5 = std::regex("^ga?u?s?s?i?a?n?_?5x?5?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_gauopn = std::regex("^ga?u?s?s?i?a?n?_?op?e?n?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    if( !std::regex_match(ImageSelectionStr, regex_none)
    &&  !std::regex_match(ImageSelectionStr, regex_first)
    &&  !std::regex_match(ImageSelectionStr, regex_last)
    &&  !std::regex_match(ImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Image selection is not valid. Cannot continue.");
    }

    // --- Cycle over all images, performing the blur ---

    //Image data.
    auto iap_it = DICOM_data.image_data.begin();
    if(false){
    }else if(std::regex_match(ImageSelectionStr, regex_none)){ iap_it = DICOM_data.image_data.end();
    }else if(std::regex_match(ImageSelectionStr, regex_last)){
        if(!DICOM_data.image_data.empty()) iap_it = std::prev(DICOM_data.image_data.end());
    }
    while(iap_it != DICOM_data.image_data.end()){
        InPlaneImageBlurUserData ud;
        ud.gaussian_sigma = GaussianOpenSigma;

        if(false){
        }else if( std::regex_match(EstimatorStr, regex_box3x3) ){
            ud.estimator = BlurEstimator::box_3x3;
        }else if( std::regex_match(EstimatorStr, regex_box5x5) ){
            ud.estimator = BlurEstimator::box_5x5;
        }else if( std::regex_match(EstimatorStr, regex_gau3x3) ){
            ud.estimator = BlurEstimator::gaussian_3x3;
        }else if( std::regex_match(EstimatorStr, regex_gau5x5) ){
            ud.estimator = BlurEstimator::gaussian_5x5;
        }else if( std::regex_match(EstimatorStr, regex_gauopn) ){
            ud.estimator = BlurEstimator::gaussian_open;
        }else{
            throw std::invalid_argument("Estimator argument '"_s + EstimatorStr + "' is not valid");
        }

        if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                          InPlaneImageBlur,
                                                          {}, {}, &ud )){
            throw std::runtime_error("Unable to compute specified blur.");
        }
        ++iap_it;
        if(std::regex_match(ImageSelectionStr, regex_first)) break;
    }

    return DICOM_data;
}
