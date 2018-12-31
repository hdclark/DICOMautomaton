//ModifyImageMetadata.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <experimental/optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "ModifyImageMetadata.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)



OperationDoc OpArgDocModifyImageMetadata(void){
    OperationDoc out;
    out.name = "ModifyImageMetadata";
    out.desc = 
        "This operation injects metadata into images.";

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "KeyValues";
    out.args.back().desc = "Key-value pairs in the form of 'key1@value1;key2@value2' that will be injected into the"
                      " selected images. Existing metadata will be overwritten. Both keys and values are"
                      " case-sensitive. Note that a semi-colon separates key-value pairs, not a colon."
                      " Note that quotation marks are not stripped internally, but may have to be"
                      " provided for the shell to properly interpret the argument.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "Description@'some description'",
                            "'Description@some description'", 
                            "MinimumSeparation@1.23", 
                            "'Description@some description;MinimumSeparation@1.23'" };

    return out;
}



Drover ModifyImageMetadata(Drover DICOM_data, 
                           OperationArgPkg OptArgs, 
                           std::map<std::string,std::string> /*InvocationMetadata*/, 
                           std::string /*FilenameLex*/ ){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto KeyValues = OptArgs.getValueStr("KeyValues").value();

    //-----------------------------------------------------------------------------------------------------------------

    std::map<std::string,std::string> key_values;
    for(auto a : SplitStringToVector(KeyValues, ';', 'd')){
        auto b = SplitStringToVector(a, '@', 'd');
        if(b.size() != 2) throw std::runtime_error("Cannot parse subexpression: "_s + a);

        key_values[b.front()] = b.back();
    }
    //if(key_values.empty()) return std::move(DICOM_data);


    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        for(auto &animg : (*iap_it)->imagecoll.images){
            for(const auto &kv_pair : key_values){
                animg.metadata[ kv_pair.first ] = kv_pair.second;
            }
        }
    }

    return DICOM_data;
}
