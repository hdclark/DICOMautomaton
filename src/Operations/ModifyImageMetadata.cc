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
    out.desc = "";

    out.notes.emplace_back("");


    // This operation injects metadata into images.

    out.args.emplace_back();
    out.args.back().name = "ImageSelection";
    out.args.back().desc = "Images to operate on. Either 'none', 'last', or 'all'.";
    out.args.back().default_val = "last";
    out.args.back().expected = true;
    out.args.back().examples = { "none", "last", "all" };

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
    const auto regex_none = std::regex("no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last = std::regex("la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all  = std::regex("al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    if( !std::regex_match(ImageSelectionStr, regex_none)
    &&  !std::regex_match(ImageSelectionStr, regex_last)
    &&  !std::regex_match(ImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Image selection is not valid. Cannot continue.");
    }


    std::map<std::string,std::string> key_values;
    for(auto a : SplitStringToVector(KeyValues, ';', 'd')){
        auto b = SplitStringToVector(a, '@', 'd');
        if(b.size() != 2) throw std::runtime_error("Cannot parse subexpression: "_s + a);

        key_values[b.front()] = b.back();
    }
    //if(key_values.empty()) return std::move(DICOM_data);


    //Image data.
    auto iap_it = DICOM_data.image_data.begin();
    if(false){
    }else if(std::regex_match(ImageSelectionStr, regex_none)){
        iap_it = DICOM_data.image_data.end();
    }else if(std::regex_match(ImageSelectionStr, regex_last)){
        if(!DICOM_data.image_data.empty()) iap_it = std::prev(DICOM_data.image_data.end());
    }
    while(iap_it != DICOM_data.image_data.end()){
        for(auto &animg : (*iap_it)->imagecoll.images){
            for(const auto &kv_pair : key_values){
                animg.metadata[ kv_pair.first ] = kv_pair.second;
            }
        }
        ++iap_it;
    }

    return DICOM_data;
}
