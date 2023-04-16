//ContourVote.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <cstdint>
#include <optional>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "ContourVote.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"



OperationDoc OpArgDocContourVote(){
    OperationDoc out;
    out.name = "ContourVote";

    out.desc = 
        " This routine pits contours against one another using various criteria. A number of 'closest' or 'best' or"
        " 'winning' contours are copied into a new contour collection with the specified ROILabel. The original ROIs are"
        " not altered, even the winning ROIs.";
        
    out.notes.emplace_back(
        "This operation considers individual contours only at the moment. It could be extended to operate on whole"
        " ROIs (i.e., contour_collections), or to perform a separate vote within each ROI. The individual contour"
        " approach was taken for relevance in 2D image (e.g., RTIMAGE) analysis."
    );
        
    out.notes.emplace_back(
        "This operation currently cannot perform voting on multiple criteria. Several criteria could be specified,"
        " but an awkward weighting system would also be needed."
    );
        

    out.args.emplace_back();
    out.args.back().name = "WinnerROILabel";
    out.args.back().desc = "The ROI label to attach to the winning contour(s)."
                      " All other metadata remains the same.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "closest", "best", "winners", "best-matches" };

    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back().name = "Area";
    out.args.back().desc = "If this option is provided with a valid positive number, the contour(s) with an area"
                      " closest to the specified value is/are retained. Note that the DICOM coordinate"
                      " space is used. (Supplying the default, NaN, will disable this option.)"
                      " Note: if several criteria are specified, it is not specified in which order they"
                      " are considered.";
    out.args.back().default_val = "nan";
    out.args.back().expected = true;
    out.args.back().examples = { "nan", "100.0", "1000", "10.23E8" };

    out.args.emplace_back();
    out.args.back().name = "Perimeter";
    out.args.back().desc = "If this option is provided with a valid positive number, the contour(s) with a perimeter"
                      " closest to the specified value is/are retained. Note that the DICOM coordinate"
                      " space is used. (Supplying the default, NaN, will disable this option.)"
                      " Note: if several criteria are specified, it is not specified in which order they"
                      " are considered.";
    out.args.back().default_val = "nan";
    out.args.back().expected = true;
    out.args.back().examples = { "nan", "0.0", "123.456", "1E6" };

    out.args.emplace_back();
    out.args.back().name = "CentroidX";
    out.args.back().desc = "If this option is provided with a valid positive number, the contour(s) with a centroid"
                      " closest to the specified value is/are retained. Note that the DICOM coordinate"
                      " space is used. (Supplying the default, NaN, will disable this option.)"
                      " Note: if several criteria are specified, it is not specified in which order they"
                      " are considered.";
    out.args.back().default_val = "nan";
    out.args.back().expected = true;
    out.args.back().examples = { "nan", "0.0", "123.456", "-1E6" };

    out.args.emplace_back();
    out.args.back().name = "CentroidY";
    out.args.back().desc = "If this option is provided with a valid positive number, the contour(s) with a centroid"
                      " closest to the specified value is/are retained. Note that the DICOM coordinate"
                      " space is used. (Supplying the default, NaN, will disable this option.)"
                      " Note: if several criteria are specified, it is not specified in which order they"
                      " are considered.";
    out.args.back().default_val = "nan";
    out.args.back().expected = true;
    out.args.back().examples = { "nan", "0.0", "123.456", "-1E6" };

    out.args.emplace_back();
    out.args.back().name = "CentroidZ";
    out.args.back().desc = "If this option is provided with a valid positive number, the contour(s) with a centroid"
                      " closest to the specified value is/are retained. Note that the DICOM coordinate"
                      " space is used. (Supplying the default, NaN, will disable this option.)"
                      " Note: if several criteria are specified, it is not specified in which order they"
                      " are considered.";
    out.args.back().default_val = "nan";
    out.args.back().expected = true;
    out.args.back().examples = { "nan", "0.0", "123.456", "-1E6" };

    out.args.emplace_back();
    out.args.back().name = "WinnerCount";
    out.args.back().desc = "Retain this number of 'best' or 'winning' contours.";
    out.args.back().default_val = "1";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "1", "3", "10000" };

    return out;
}



bool ContourVote(Drover &DICOM_data,
                   const OperationArgPkg& OptArgs,
                   std::map<std::string, std::string>& /*InvocationMetadata*/,
                   const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto WinnerROILabel = OptArgs.getValueStr("WinnerROILabel").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();

    const auto Perimeter = std::stod( OptArgs.getValueStr("Perimeter").value() );
    const auto Area = std::stod( OptArgs.getValueStr("Area").value() );
    const auto WinnerCount = std::stol( OptArgs.getValueStr("WinnerCount").value() );
    const auto CentroidX = std::stod( OptArgs.getValueStr("CentroidX").value() );
    const auto CentroidY = std::stod( OptArgs.getValueStr("CentroidY").value() );
    const auto CentroidZ = std::stod( OptArgs.getValueStr("CentroidZ").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto roiregex = Compile_Regex(ROILabelRegex);

    const auto roinormalizedregex = Compile_Regex(NormalizedROILabelRegex);

    if(!std::isfinite(WinnerCount) || (WinnerCount < 0)){
        throw std::invalid_argument("The number of winners to retain has to be [0,inf).");
    }

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    std::list<std::reference_wrapper<contour_of_points<double>>> cop_all;
    DICOM_data.Ensure_Contour_Data_Allocated();
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
        YLOGWARN("No contours participated, so no contours won");
    }
        
    if(!std::isnan( Area )){
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
    }else if(!std::isnan( CentroidX )){
        cop_ROIs.sort( [&](const std::reference_wrapper<contour_of_points<double>> A,
                           const std::reference_wrapper<contour_of_points<double>> B ){
                               const auto AX = A.get().Centroid().x;
                               const auto BX = B.get().Centroid().x;
                               return std::abs(CentroidX - AX) < std::abs(CentroidX - BX);
                      });
    }else if(!std::isnan( CentroidY )){
        cop_ROIs.sort( [&](const std::reference_wrapper<contour_of_points<double>> A,
                           const std::reference_wrapper<contour_of_points<double>> B ){
                               const auto AY = A.get().Centroid().y;
                               const auto BY = B.get().Centroid().y;
                               return std::abs(CentroidY - AY) < std::abs(CentroidY - BY);
                      });
    }else if(!std::isnan( CentroidZ )){
        cop_ROIs.sort( [&](const std::reference_wrapper<contour_of_points<double>> A,
                           const std::reference_wrapper<contour_of_points<double>> B ){
                               const auto AZ = A.get().Centroid().z;
                               const auto BZ = B.get().Centroid().z;
                               return std::abs(CentroidZ - AZ) < std::abs(CentroidZ - BZ);
                      });
    }

    //Create a new contour collection from the winning contours.
    contour_collection<double> cc_new;
    for(auto & cop : cop_ROIs){
        if(static_cast<int64_t>(cc_new.contours.size()) >= WinnerCount) break;
        cc_new.contours.emplace_back(cop.get());
    }

    //Attach the requested metadata.
    cc_new.Insert_Metadata("ROIName", WinnerROILabel);
    cc_new.Insert_Metadata("NormalizedROIName", X(WinnerROILabel));
    cc_new.Insert_Metadata("ROINumber", "999"); // TODO: fix this.
    cc_new.Insert_Metadata("MinimumSeparation", "1.0"); // TODO: fix this.

    if(!cc_new.contours.empty()){
        DICOM_data.Ensure_Contour_Data_Allocated();
        DICOM_data.contour_data->ccs.emplace_back(cc_new);
    }

    return true;
}
