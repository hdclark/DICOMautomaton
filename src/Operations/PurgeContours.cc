//PurgeContours.cc - A part of DICOMautomaton 2018. Written by hal clark.

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
#include "PurgeContours.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocPurgeContours(){
    OperationDoc out;
    out.name = "PurgeContours";
    out.desc = 
        " This routine purges contours if they satisfy various criteria.";
        
    out.notes.emplace_back(
        "This operation considers individual contours only at the moment. It could be extended to operate on whole"
        " ROIs (i.e., contour_collections), or to perform a separate vote within each ROI. The individual contour"
        " approach was taken since filtering out small contour 'islands' is the primary use-case."
    );


    out.args.emplace_back();
    out.args.back().name = "ROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                            R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                            R"***(left_parotid|right_parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };


    out.args.emplace_back();
    out.args.back().name = "Invert";
    out.args.back().desc = "If false, matching contours will be purged. If true, matching contours"
                           " will be retained and non-matching contours will be purged."
                           " Enabling this option is equivalent to a 'Keep if and only if' operation.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };


    out.args.emplace_back();
    out.args.back().name = "AreaAbove";
    out.args.back().desc = "If this option is provided with a valid positive number, contour(s) with an area"
                      " greater than the specified value are purged. Note that the DICOM coordinate"
                      " space is used. (Supplying the default, inf, will disable this option.)";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "inf", "100.0", "1000", "10.23E8" };

    out.args.emplace_back();
    out.args.back().name = "AreaBelow";
    out.args.back().desc = "If this option is provided with a valid positive number, contour(s) with an area"
                      " less than the specified value are purged. Note that the DICOM coordinate"
                      " space is used. (Supplying the default, -inf, will disable this option.)";
    out.args.back().default_val = "-inf";
    out.args.back().expected = true;
    out.args.back().examples = { "-inf", "100.0", "1000", "10.23E8" };


    out.args.emplace_back();
    out.args.back().name = "PerimeterAbove";
    out.args.back().desc = "If this option is provided with a valid positive number, contour(s) with a perimeter"
                      " greater than the specified value are purged. Note that the DICOM coordinate"
                      " space is used. (Supplying the default, inf, will disable this option.)";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "inf", "10.0", "100", "10.23E4" };

    out.args.emplace_back();
    out.args.back().name = "PerimeterBelow";
    out.args.back().desc = "If this option is provided with a valid positive number, contour(s) with a perimeter"
                      " less than the specified value are purged. Note that the DICOM coordinate"
                      " space is used. (Supplying the default, -inf, will disable this option.)";
    out.args.back().default_val = "-inf";
    out.args.back().expected = true;
    out.args.back().examples = { "-inf", "10.0", "100", "10.23E4" };

    return out;
}



Drover PurgeContours(Drover DICOM_data, const OperationArgPkg& OptArgs, const std::map<std::string,std::string>&, const std::string& ){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();

    const auto InvertStr = OptArgs.getValueStr("Invert").value();

    const auto AreaAbove = std::stod( OptArgs.getValueStr("AreaAbove").value() );
    const auto AreaBelow = std::stod( OptArgs.getValueStr("AreaBelow").value() );
    const auto PerimeterAbove = std::stod( OptArgs.getValueStr("PerimeterAbove").value() );
    const auto PerimeterBelow = std::stod( OptArgs.getValueStr("PerimeterBelow").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = Compile_Regex("^tr?u?e?$");

    const auto roiregex = Compile_Regex(ROILabelRegex);
    const auto roinormalizedregex = Compile_Regex(NormalizedROILabelRegex);

    const auto invert_logic = (std::regex_match(InvertStr, regex_true)) ?
                      [](bool in){ return !in; }
                    : [](bool in){ return in; };

    auto matches_ROIName = [=](const contour_of_points<double> &cop) -> bool {
                   const auto ROINameOpt = cop.GetMetadataValueAs<std::string>("ROIName");
                   const auto ROIName = ROINameOpt.value_or("");
                   return (std::regex_match(ROIName,roiregex));
    };
    auto matches_NormalizedROIName = [=](const contour_of_points<double> &cop) -> bool {
                   const auto ROINameOpt = cop.GetMetadataValueAs<std::string>("NormalizedROIName");
                   const auto ROIName = ROINameOpt.value_or("");
                   return (std::regex_match(ROIName,roiregex));
    };
    auto area_criteria = [=](const contour_of_points<double> &cop) -> bool {
        //This closure returns true when a contour should be removed.
        if( !std::isnan( AreaAbove ) ){
            const auto area = std::abs(cop.Get_Signed_Area());
            if(area >= AreaAbove) return true;
        }
        if( !std::isnan( AreaBelow ) ){
            const auto area = std::abs(cop.Get_Signed_Area());
            if(area <= AreaBelow) return true;
        }
        return false;
    };
    auto perimeter_criteria = [=](const contour_of_points<double> &cop) -> bool {
        //This closure returns true when a contour should be removed.
        if( !std::isnan( PerimeterAbove )){
            const auto perimeter = cop.Perimeter();
            if(perimeter >= PerimeterAbove) return true;
        }
        if( !std::isnan( PerimeterBelow )){
            const auto perimeter = cop.Perimeter();
            if(perimeter <= PerimeterBelow) return true;
        }
        return false;
    };
    auto remove_all_criteria = [&](const contour_of_points<double> &cop) -> bool {
        if(!matches_ROIName(cop) && !matches_NormalizedROIName(cop)) return invert_logic(false); //Ignore.
        return invert_logic( area_criteria(cop) || perimeter_criteria(cop) );
    };

    //Walk the contours, testing each contour iff selected.
    for(auto & cc : DICOM_data.contour_data->ccs){
        cc.contours.remove_if( remove_all_criteria );
    }

    //Note: should I remove empty ccs here? It can cause issues either way ... TODO.

    return DICOM_data;
}
