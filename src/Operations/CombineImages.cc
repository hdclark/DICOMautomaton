//CombineImages.cc - A part of DICOMautomaton 2023. Written by hal clark.

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

#include "CombineImages.h"



OperationDoc OpArgDocCombineImages(){
    OperationDoc out;
    out.name = "CombineImages";
    out.tags.emplace_back("category: image processing");

    out.desc = 
        "This operation combines the images in two or more image arrays, creating a single image array"
        " containing all images.";

    out.notes.emplace_back(
        "The original image arrays are removed and all images are placed into a image array appended at the end."
    );
    out.notes.emplace_back(
        "Individual images (e.g., those that are spatially overlapping) are not merged together."
        " No voxel resampling or combination is performed."
    );


    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "first";

    return out;
}



bool CombineImages(Drover &DICOM_data,
                    const OperationArgPkg& OptArgs,
                    std::map<std::string, std::string>& /*InvocationMetadata*/,
                    const std::string& /*FilenameLex*/){


    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    auto ia_ptr = std::make_shared<Image_Array>();

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        auto imgarr_ptr = &((*iap_it)->imagecoll);

        ia_ptr->imagecoll.images.splice( std::end(ia_ptr->imagecoll.images),
                                         imgarr_ptr->images );
    }

    for(auto & iap_it : IAs){
        DICOM_data.image_data.erase( iap_it );
    }
    if(!ia_ptr->imagecoll.images.empty()){
        DICOM_data.image_data.emplace_back(ia_ptr);
    }

    return true;
}
