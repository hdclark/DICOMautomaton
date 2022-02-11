//DeleteImages.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <optional>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "DeleteImages.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocDeleteImages(){
    OperationDoc out;
    out.name = "DeleteImages";
    out.desc = 
        " This routine deletes images from memory."
        " It is most useful when working with positional operations in stages.";

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    return out;
}



bool DeleteImages(Drover &DICOM_data,
                    const OperationArgPkg& OptArgs,
                    std::map<std::string, std::string>& /*InvocationMetadata*/,
                    const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        DICOM_data.image_data.erase( iap_it );
    }

    return true;
}
