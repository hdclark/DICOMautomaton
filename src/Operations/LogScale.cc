//LogScale.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

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
#include "../YgorImages_Functors/Processing/Logarithmic_Pixel_Scale.h"
#include "LogScale.h"
#include "YgorImages.h"


std::list<OperationArgDoc> OpArgDocLogScale(void){
    std::list<OperationArgDoc> out;

    //This operation log-scales pixels for all available image arrays. This functionality is often desired for viewing
    // purposes, to make the pixel level changes appear more linear. Be weary of using for anything quantitative!
    //

    out.emplace_back();
    out.back().name = "ImageSelection";
    out.back().desc = "Images to operate on. Either 'none', 'last', 'first', or 'all'.";
    out.back().default_val = "last";
    out.back().expected = true;
    out.back().examples = { "none", "last", "first", "all" };
    
    return out;
}

Drover LogScale(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> , std::string){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_none  = std::regex("^no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_first = std::regex("^fi?r?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last  = std::regex("^la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all   = std::regex("^al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    if( !std::regex_match(ImageSelectionStr, regex_none)
    &&  !std::regex_match(ImageSelectionStr, regex_first)
    &&  !std::regex_match(ImageSelectionStr, regex_last)
    &&  !std::regex_match(ImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Image selection is not valid. Cannot continue.");
    }


    // --- Cycle over all images, performing the detection ---

    auto iap_it = DICOM_data.image_data.begin();
    if(false){
    }else if(std::regex_match(ImageSelectionStr, regex_none)){ iap_it = DICOM_data.image_data.end();
    }else if(std::regex_match(ImageSelectionStr, regex_last)){
        if(!DICOM_data.image_data.empty()) iap_it = std::prev(DICOM_data.image_data.end());
    }
    while(iap_it != DICOM_data.image_data.end()){
        if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                          LogScalePixels,
                                                          {}, {} )){
            throw std::runtime_error("Unable to log-scale image.");
        }
        ++iap_it;
        if(std::regex_match(ImageSelectionStr, regex_first)) break;
    }

    return DICOM_data;
}
