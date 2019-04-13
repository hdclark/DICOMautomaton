//SubtractImages.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <experimental/optional>
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



OperationDoc OpArgDocSubtractImages(void){
    OperationDoc out;
    out.name = "SubtractImages";
    out.desc = 
        " This routine subtracts images that spatially overlap.";

    out.notes.emplace_back(
        "This operation currently performs a subtraction necessarily using the first image volume."
    );
    out.notes.emplace_back(
        "This operation is currently extremely limited in that the selected images must be selected by position."
        " A more flexible approach will be eventually be implemented when the image selection mechanism is overhauled."
    );

    out.args.emplace_back();
    out.args.back().name = "ImageSelection";
    out.args.back().desc = "Images to operate on. Either 'none', 'last', 'first', or 'all'.";
    out.args.back().default_val = "all";
    out.args.back().expected = true;
    out.args.back().examples = { "none", "last", "first", "all" };

    return out;
}



Drover SubtractImages(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string>, std::string ){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_none  = Compile_Regex("^no?n?e?$");
    const auto regex_first = Compile_Regex("^fi?r?s?t?$");
    const auto regex_last  = Compile_Regex("^la?s?t?$");
    const auto regex_all   = Compile_Regex("^al?l?$");

    if( !std::regex_match(ImageSelectionStr, regex_none)
    &&  !std::regex_match(ImageSelectionStr, regex_first)
    &&  !std::regex_match(ImageSelectionStr, regex_last)
    &&  !std::regex_match(ImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Image selection is not valid. Cannot continue.");
    }

    auto iapA_it = DICOM_data.image_data.begin();
    std::list<std::shared_ptr<Image_Array>> diffs;

    auto iapB_it = DICOM_data.image_data.begin();
    if(false){
    }else if(std::regex_match(ImageSelectionStr, regex_none)){ iapB_it = DICOM_data.image_data.end();
    }else if(std::regex_match(ImageSelectionStr, regex_last)){
        if(!DICOM_data.image_data.empty()) iapB_it = std::prev(DICOM_data.image_data.end());
    }
    while(iapB_it != DICOM_data.image_data.end()){

        std::shared_ptr<Image_Array> difference = std::make_shared<Image_Array>(*(*iapA_it));
        {
          std::list<std::reference_wrapper<planar_image_collection<float,double>>> external_imgs;
          external_imgs.push_back( std::ref((*iapB_it)->imagecoll) );

          if(!difference->imagecoll.Transform_Images( SubtractSpatiallyOverlappingImages, std::move(external_imgs), {} )){
              FUNCERR("Unable to subtract the pixel maps");
          }
        }
        diffs.emplace_back( difference );

        ++iapB_it;
        if(std::regex_match(ImageSelectionStr, regex_first)) break;
    }

    for(auto &iap : diffs){
        DICOM_data.image_data.emplace_back(iap);
    }

    return DICOM_data;
}
