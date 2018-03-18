//PurgeContours.cc - A part of DICOMautomaton 2018. Written by hal clark.

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

#include "PurgeContours.h"



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



Drover PurgeContours(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){
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
