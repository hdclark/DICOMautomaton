//PurgeContours.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <experimental/optional>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <string>    

#include "../Structs.h"
#include "PurgeContours.h"
#include "YgorMath.h"         //Needed for vec3 class.



std::list<OperationArgDoc> OpArgDocPurgeContours(void){
    std::list<OperationArgDoc> out;

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
    out.back().name = "AreaAbove";
    out.back().desc = "If this option is provided with a valid positive number, contour(s) with an area"
                      " greater than the specified value are purged. Note that the DICOM coordinate"
                      " space is used. (Supplying the default, inf, will disable this option.)";
    out.back().default_val = "inf";
    out.back().expected = true;
    out.back().examples = { "inf", "100.0", "1000", "10.23E8" };

    out.emplace_back();
    out.back().name = "AreaBelow";
    out.back().desc = "If this option is provided with a valid positive number, contour(s) with an area"
                      " less than the specified value are purged. Note that the DICOM coordinate"
                      " space is used. (Supplying the default, -inf, will disable this option.)";
    out.back().default_val = "-inf";
    out.back().expected = true;
    out.back().examples = { "-inf", "100.0", "1000", "10.23E8" };


    out.emplace_back();
    out.back().name = "PerimeterAbove";
    out.back().desc = "If this option is provided with a valid positive number, contour(s) with a perimeter"
                      " greater than the specified value are purged. Note that the DICOM coordinate"
                      " space is used. (Supplying the default, inf, will disable this option.)";
    out.back().default_val = "inf";
    out.back().expected = true;
    out.back().examples = { "inf", "10.0", "100", "10.23E4" };

    out.emplace_back();
    out.back().name = "PerimeterBelow";
    out.back().desc = "If this option is provided with a valid positive number, contour(s) with a perimeter"
                      " less than the specified value are purged. Note that the DICOM coordinate"
                      " space is used. (Supplying the default, -inf, will disable this option.)";
    out.back().default_val = "-inf";
    out.back().expected = true;
    out.back().examples = { "-inf", "10.0", "100", "10.23E4" };

    return out;
}



Drover PurgeContours(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string>, std::string ){
    // This routine purges contours if they satisfy various criteria.
    //
    // Note: This operation considers individual contours only at the moment. It could be extended to operate on whole
    //       ROIs (i.e., contour_collections), or to perform a separate vote within each ROI. The individual contour
    //       approach was taken since filtering out small contour 'islands' is the primary use-case.
    //

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();

    const auto AreaAbove = std::stod( OptArgs.getValueStr("AreaAbove").value() );
    const auto AreaBelow = std::stod( OptArgs.getValueStr("AreaBelow").value() );
    const auto PerimeterAbove = std::stod( OptArgs.getValueStr("PerimeterAbove").value() );
    const auto PerimeterBelow = std::stod( OptArgs.getValueStr("PerimeterBelow").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto roiregex = std::regex(ROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto roinormalizedregex = std::regex(NormalizedROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);


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
        if(!matches_ROIName(cop) && !matches_NormalizedROIName(cop)) return false; //Ignore.
        return area_criteria(cop) || perimeter_criteria(cop);
    };

    //Walk the contours, testing each contour iff selected.
    for(auto & cc : DICOM_data.contour_data->ccs){
        cc.contours.remove_if( remove_all_criteria );
    }

    //Note: should I remove empty ccs here? It can cause issues either way ... TODO.

    return DICOM_data;
}
