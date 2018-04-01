//CopyImages.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <deque>
#include <experimental/optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "CopyImages.h"

std::list<OperationArgDoc> OpArgDocCopyImages(void){
    std::list<OperationArgDoc> out;

    // This operation deep-copies the selected images.

    out.emplace_back();
    out.back().name = "ImageSelection";
    out.back().desc = "Images to operate on. Either 'none', 'last', 'first', or 'all'.";
    out.back().default_val = "last";
    out.back().expected = true;
    out.back().examples = { "none", "last", "first", "all" };
    
    return out;
}

Drover CopyImages(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> , std::string){

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

    //Gather a list of images to work on.
    std::deque<std::shared_ptr<Image_Array>> img_arrays_to_copy;

    auto iap_it = DICOM_data.image_data.begin();
    if(false){
    }else if(std::regex_match(ImageSelectionStr, regex_none)){ iap_it = DICOM_data.image_data.end();
    }else if(std::regex_match(ImageSelectionStr, regex_last)){
        if(!DICOM_data.image_data.empty()) iap_it = std::prev(DICOM_data.image_data.end());
    }
    while(iap_it != DICOM_data.image_data.end()){

        img_arrays_to_copy.push_back(*iap_it);

        ++iap_it;
        if(std::regex_match(ImageSelectionStr, regex_first)) break;
    }

    //Copy the images.
    for(auto & img_arr : img_arrays_to_copy){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
    }

    return DICOM_data;
}
