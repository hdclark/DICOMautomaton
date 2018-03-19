//ContourVote.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <experimental/optional>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "ContourVote.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.



std::list<OperationArgDoc> OpArgDocContourVote(void){
    std::list<OperationArgDoc> out;


    out.emplace_back();
    out.back().name = "WinnerROILabel";
    out.back().desc = "The ROI label to attach to the winning contour(s)."
                      " All other metadata remains the same.";
    out.back().default_val = "unspecified";
    out.back().expected = true;
    out.back().examples = { "closest", "best", "winners", "best-matches" };

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
    out.back().name = "Area";
    out.back().desc = "If this option is provided with a valid positive number, the contour(s) with an area"
                      " closest to the specified value is/are retained. Note that the DICOM coordinate"
                      " space is used. (Supplying the default, NaN, will disable this option.)"
                      " Note: if several criteria are specified, it is not specified in which order they"
                      " are considered.";
    out.back().default_val = "nan";
    out.back().expected = true;
    out.back().examples = { "nan", "100.0", "1000", "10.23E8" };

    out.emplace_back();
    out.back().name = "Perimeter";
    out.back().desc = "If this option is provided with a valid positive number, the contour(s) with a perimeter"
                      " closest to the specified value is/are retained. Note that the DICOM coordinate"
                      " space is used. (Supplying the default, NaN, will disable this option.)"
                      " Note: if several criteria are specified, it is not specified in which order they"
                      " are considered.";
    out.back().default_val = "nan";
    out.back().expected = true;
    out.back().examples = { "nan", "0.0", "123.456", "1E6" };

    out.emplace_back();
    out.back().name = "WinnerCount";
    out.back().desc = "Retain this number of 'best' or 'winning' contours.";
    out.back().default_val = "1";
    out.back().expected = true;
    out.back().examples = { "0", "1", "3", "10000" };

    return out;
}



Drover ContourVote(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){
    // This routine pits contours against one another using various criteria. A number of 'closest' or 'best' or
    // 'winning' contours are copied into a new contour collection with the specified ROILabel. The original ROIs are
    // not altered, even the winning ROIs.
    //
    // Note: This operation considers individual contours only at the moment. It could be extended to operate on whole
    //       ROIs (i.e., contour_collections), or to perform a separate vote within each ROI. The individual contour
    //       approach was taken for relevance in 2D image (e.g., RTIMAGE) analysis.
    //
    // Note: This operation currently cannot perform voting on multiple criteria. Several criteria could be specified,
    //       but an awkward weighting system would also be needed.
    //

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto WinnerROILabel = OptArgs.getValueStr("WinnerROILabel").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();

    const auto Perimeter = std::stod( OptArgs.getValueStr("Perimeter").value() );
    const auto Area = std::stod( OptArgs.getValueStr("Area").value() );
    const auto WinnerCount = std::stol( OptArgs.getValueStr("WinnerCount").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto roiregex = std::regex(ROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto roinormalizedregex = std::regex(NormalizedROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    if(!std::isfinite(WinnerCount) || (WinnerCount < 0)){
        throw std::invalid_argument("The number of winners to retain has to be [0,inf).");
    }

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    std::list<std::reference_wrapper<contour_of_points<double>>> cop_all;
    for(auto & cc : DICOM_data.contour_data->ccs){
        for(auto & cop : cc.contours){
            cop_all.push_back( std::ref(cop) );
        }
    }

    //Whitelist contours using the provided regex.
    auto cop_ROIs = cop_all;
    cop_ROIs.remove_if([=](std::reference_wrapper<contour_of_points<double>> cop) -> bool {
                   const auto ROINameOpt = cop.get().GetMetadataValueAs<std::string>("ROIName");
                   const auto ROIName = ROINameOpt.value_or("");
                   return !(std::regex_match(ROIName,roiregex));
    });
    cop_ROIs.remove_if([=](std::reference_wrapper<contour_of_points<double>> cop) -> bool {
                   const auto ROINameOpt = cop.get().GetMetadataValueAs<std::string>("NormalizedROIName");
                   const auto ROIName = ROINameOpt.value_or("");
                   return !(std::regex_match(ROIName,roinormalizedregex));
    });
    if(cop_ROIs.empty()){
        FUNCWARN("No contours participated, so no contours won");
    }
        
    if(false){
    }else if(!std::isnan( Area )){
        cop_ROIs.sort( [&](const std::reference_wrapper<contour_of_points<double>> A,
                           const std::reference_wrapper<contour_of_points<double>> B ){
                               const auto AA = std::abs(A.get().Get_Signed_Area());
                               const auto BA = std::abs(B.get().Get_Signed_Area());
                               return std::abs(Area - AA) < std::abs(Area - BA);
                      });
    }else if(!std::isnan( Perimeter )){
        cop_ROIs.sort( [&](const std::reference_wrapper<contour_of_points<double>> A,
                           const std::reference_wrapper<contour_of_points<double>> B ){
                               const auto AP = A.get().Perimeter();
                               const auto BP = B.get().Perimeter();
                               return std::abs(Perimeter - AP) < std::abs(Perimeter - BP);
                      });
    }

    //Create a new contour collection from the winning contours.
    contour_collection<double> cc_new;
    for(auto & cop : cop_ROIs){
        if(cc_new.contours.size() >= WinnerCount) break;
        cc_new.contours.emplace_back(cop.get());
    }

    //Attach the requested metadata.
    cc_new.Insert_Metadata("ROIName", WinnerROILabel);
    cc_new.Insert_Metadata("NormalizedROIName", X(WinnerROILabel));
    cc_new.Insert_Metadata("ROINumber", "999");

    if(!cc_new.contours.empty()){
        DICOM_data.contour_data->ccs.emplace_back(cc_new);
        DICOM_data.contour_data->ccs.back().ROI_number = 999;
        DICOM_data.contour_data->ccs.back().Minimum_Separation = 1.0;
        DICOM_data.contour_data->ccs.back().Raw_ROI_name = WinnerROILabel;
    }

    return DICOM_data;
}
