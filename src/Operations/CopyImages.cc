//CopyImages.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <deque>
#include <optional>
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

OperationDoc OpArgDocCopyImages(){
    OperationDoc out;
    out.name = "CopyImages";

    out.desc = 
        " This operation deep-copies the selected image arrays.";

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
    
    return out;
}

bool CopyImages(Drover &DICOM_data,
                  const OperationArgPkg& OptArgs,
                  const std::map<std::string, std::string>&,
                  const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    //Gather a list of images to work on.
    std::deque<std::shared_ptr<Image_Array>> img_arrays_to_copy;

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        img_arrays_to_copy.push_back(*iap_it);
    }

    //Copy the images.
    for(auto & img_arr : img_arrays_to_copy){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
    }

    return true;
}
