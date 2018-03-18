//ContourVote.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>    
#include <vector>
#include <set> 
#include <map>
#include <unordered_map>
#include <list>
#include <functional>
#include <thread>
#include <array>
#include <mutex>
#include <limits>
#include <cmath>
#include <regex>

#include <getopt.h>           //Needed for 'getopts' argument parsing.
#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <experimental/optional>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathIOOFF.h"    //Needed for WritePointsToOFF(...)
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMathChebyshev.h" //Needed for cheby_approx class.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorContainers.h"   //Needed for bimap class.
#include "YgorPerformance.h"  //Needed for YgorPerformance_dt_from_last().
#include "YgorAlgorithms.h"   //Needed for For_Each_In_Parallel<..>(...)
#include "YgorArguments.h"    //Needed for ArgumentHandler class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorImagesPlotting.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Thread_Pool.h"
#include "../Dose_Meld.h"
#include "../Contour_Boolean_Operations.h"

#include "../YgorImages_Functors/Grouping/Misc_Functors.h"

#include "../YgorImages_Functors/Processing/DCEMRI_AUC_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_S0_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_T1_Map.h"
#include "../YgorImages_Functors/Processing/Highlight_ROI_Voxels.h"
#include "../YgorImages_Functors/Processing/Kitchen_Sink_Analysis.h"
#include "../YgorImages_Functors/Processing/IVIMMRI_ADC_Map.h"
#include "../YgorImages_Functors/Processing/Time_Course_Slope_Map.h"
#include "../YgorImages_Functors/Processing/CT_Perfusion_Clip_Search.h"
#include "../YgorImages_Functors/Processing/CT_Perf_Pixel_Filter.h"
#include "../YgorImages_Functors/Processing/CT_Convert_NaNs_to_Air.h"
#include "../YgorImages_Functors/Processing/Min_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/Max_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/CT_Reasonable_HU_Window.h"
#include "../YgorImages_Functors/Processing/Slope_Difference.h"
#include "../YgorImages_Functors/Processing/Centralized_Moments.h"
#include "../YgorImages_Functors/Processing/Logarithmic_Pixel_Scale.h"
#include "../YgorImages_Functors/Processing/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Processing/DBSCAN_Time_Courses.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bilinear_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bicubic_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Pixel_Decimate.h"
#include "../YgorImages_Functors/Processing/ImagePartialDerivative.h"
#include "../YgorImages_Functors/Processing/Orthogonal_Slices.h"

#include "../YgorImages_Functors/Transform/DCEMRI_C_Map.h"
#include "../YgorImages_Functors/Transform/DCEMRI_Signal_Difference_C.h"
#include "../YgorImages_Functors/Transform/CT_Perfusion_Signal_Diff.h"
#include "../YgorImages_Functors/Transform/DCEMRI_S0_Map_v2.h"
#include "../YgorImages_Functors/Transform/DCEMRI_T1_Map_v2.h"
#include "../YgorImages_Functors/Transform/Pixel_Value_Histogram.h"
#include "../YgorImages_Functors/Transform/Subtract_Spatially_Overlapping_Images.h"

#include "../YgorImages_Functors/Compute/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Compute/Contour_Similarity.h"
#include "../YgorImages_Functors/Compute/AccumulatePixelDistributions.h"
#include "../YgorImages_Functors/Compute/GenerateSurfaceMask.h"

#include "ContourVote.h"



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
