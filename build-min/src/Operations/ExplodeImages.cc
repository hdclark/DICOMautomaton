//ExplodeImages.cc - A part of DICOMautomaton 2023. Written by hal clark.

#include <optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>
#include <vector>

#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorImages.h"
#include "YgorString.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Metadata.h"
#include "../YgorImages_Functors/Compute/Joint_Pixel_Sampler.h"

#include "ExplodeImages.h"



OperationDoc OpArgDocExplodeImages(){
    OperationDoc out;
    out.name = "ExplodeImages";

    out.tags.emplace_back("category: image processing");

    out.desc = 
        "This operation takes an image array containing multiple images and 'explodes' it,"
        " creating one new image array for each individual image.";

    out.notes.emplace_back(
        "The original image array is removed and each image is appended as a separate image array."
    );


    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "first";

    return out;
}



bool ExplodeImages(Drover &DICOM_data,
                    const OperationArgPkg& OptArgs,
                    std::map<std::string, std::string>& /*InvocationMetadata*/,
                    const std::string& /*FilenameLex*/){


    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    decltype(DICOM_data.image_data) shtl;

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        auto imgarr_ptr = &((*iap_it)->imagecoll);

        while(!imgarr_ptr->images.empty()){
            auto ia_ptr = std::make_shared<Image_Array>();
            ia_ptr->imagecoll.images.splice( std::end(ia_ptr->imagecoll.images), 
                                             imgarr_ptr->images,
                                             std::begin(imgarr_ptr->images) );
            shtl.emplace_back( ia_ptr );
        }
    }

    for(auto & iap_it : IAs){
        DICOM_data.image_data.erase( iap_it );
    }
    DICOM_data.image_data.splice( std::end(DICOM_data.image_data), shtl );

    return true;
}
