//LogScale.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

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
#include "../YgorImages_Functors/Processing/Logarithmic_Pixel_Scale.h"
#include "LogScale.h"
#include "YgorImages.h"


OperationDoc OpArgDocLogScale(){
    OperationDoc out;
    out.name = "LogScale";

    out.desc = 
        "This operation log-scales pixels for all available image arrays. This functionality is often desired for viewing"
        " purposes, to make the pixel level changes appear more linear. Be weary of using for anything quantitative!";

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
    
    return out;
}

Drover LogScale(Drover DICOM_data,
                const OperationArgPkg& OptArgs,
                const std::map<std::string, std::string>&,
                const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                          LogScalePixels,
                                                          {}, {} )){
            throw std::runtime_error("Unable to log-scale image.");
        }
    }

    return DICOM_data;
}
