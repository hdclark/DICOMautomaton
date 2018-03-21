//ModifyContourMetadata.cc - A part of DICOMautomaton 2018. Written by hal clark.

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
#include "ModifyContourMetadata.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)



std::list<OperationArgDoc> OpArgDocModifyContourMetadata(void){
    std::list<OperationArgDoc> out;

    // This operation injects metadata into contours.

    out.emplace_back();
    out.back().name = "NormalizedROILabelRegex";
    out.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.back().default_val = ".*";
    out.back().expected = true;
    out.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };

    out.emplace_back();
    out.back().name = "ROILabelRegex";
    out.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.back().default_val = ".*";
    out.back().expected = true;
    out.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                            R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                            R"***(left_parotid|right_parotid)***" };

    out.emplace_back();
    out.back().name = "KeyValues";
    out.back().desc = "Key-value pairs in the form of 'key1@value1;key2@value2' that will be injected into the"
                      " selected images. Existing metadata will be overwritten. Both keys and values are"
                      " case-sensitive. Note that a semi-colon separates key-value pairs, not a colon."
                      " Note that quotation marks are not stripped internally, but may have to be"
                      " provided for the shell to properly interpret the argument.";
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "Description@'some description'",
                            "'Description@some description'", 
                            "MinimumSeparation@1.23", 
                            "'Description@some description;MinimumSeparation@1.23'" };

    return out;
}



Drover ModifyContourMetadata(Drover DICOM_data, 
                           OperationArgPkg OptArgs, 
                           std::map<std::string,std::string> /*InvocationMetadata*/, 
                           std::string /*FilenameLex*/ ){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto KeyValues = OptArgs.getValueStr("KeyValues").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto theregex = std::regex(ROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto thenormalizedregex = std::regex(NormalizedROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    std::list<std::reference_wrapper<contour_collection<double>>> cc_all;
    for(auto & cc : DICOM_data.contour_data->ccs){
        auto base_ptr = reinterpret_cast<contour_collection<double> *>(&cc);
        cc_all.push_back( std::ref(*base_ptr) );
    }

    //Whitelist contours using the provided regex.
    auto cc_ROIs = cc_all;
    cc_ROIs.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("ROIName");
                   const auto ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,theregex));
    });
    cc_ROIs.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("NormalizedROIName");
                   const auto ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,thenormalizedregex));
    });
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }


    std::map<std::string,std::string> key_values;
    for(auto a : SplitStringToVector(KeyValues, ';', 'd')){
        auto b = SplitStringToVector(a, '@', 'd');
        if(b.size() != 2) throw std::runtime_error("Cannot parse subexpression: "_s + a);

        key_values[b.front()] = b.back();
    }
    //if(key_values.empty()) return std::move(DICOM_data);


    //Attach the metadata.
    for(auto & cc : cc_ROIs){
        for(auto & cop : cc.get().contours){
            for(const auto &kv_pair : key_values){
                cop.metadata[ kv_pair.first ] = kv_pair.second;
            }
        }
    }

    return DICOM_data;
}
