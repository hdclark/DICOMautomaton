//GrowContours.cc - A part of DICOMautomaton 2017. Written by hal clark.

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

#include "GrowContours.h"



std::list<OperationArgDoc> OpArgDocGrowContours(void){
    std::list<OperationArgDoc> out;

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
    out.back().name = "Distance";
    out.back().desc = "The distance to translate contour vertices. (The direction is outward.)";
    out.back().default_val = "0.00354165798657632";
    out.back().expected = true;
    out.back().examples = { "1E-5", "0.321", "1.1", "15.3" };



    return out;
}



Drover GrowContours(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string>, std::string ){
    // This routine will grow (or shrink) 2D contours in their plane by the specified amount. 
    // Growth is accomplish by translating vertices away from the interior by the specified amount.
    // The direction is chosen to be the direction opposite of the in-plane normal produced by averaging the line
    // segments connecting the contours.

    if(DICOM_data.contour_data == nullptr) return DICOM_data;

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();

    const auto dR = std::stod( OptArgs.getValueStr("Distance").value() );

    //-----------------------------------------------------------------------------------------------------------------

    const auto theregex = std::regex(ROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto thenormalizedregex = std::regex(NormalizedROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    for(auto &cc : DICOM_data.contour_data->ccs){
        for(auto &cop : cc.contours){
            if(cop.points.size() < 3) continue;

            const auto ROINameOpt = cop.GetMetadataValueAs<std::string>("ROIName");
            const auto ROIName = ROINameOpt.value_or("");
            const auto NROINameOpt = cop.GetMetadataValueAs<std::string>("NormalizedROIName");
            const auto NROIName = NROINameOpt.value_or("");
//            if(!( std::regex_match(ROIName,theregex) || std::regex_match(NROIName,thenormalizedregex))) continue;
            if(!std::regex_match(ROIName,theregex)) continue;

            const auto N = cop.Estimate_Planar_Normal();
            const auto aplane = cop.Least_Squares_Best_Fit_Plane(N);

            auto cop_orig = cop;
            auto cop_ref = cop;
            //Simplify the implementation by duplicating the first and last vertices.

            cop_ref.points.push_front(cop.points.back());
            cop_ref.points.push_back(cop.points.front());

            auto itA = cop_ref.points.begin();
            auto itB = cop.points.begin();

            const auto R_cent = cop.Centroid();

            for( ; (itB != cop.points.end()) ; ++itA, ++itB ){
                auto itC = itA;

                const auto R_prev = *(itC++);
                const auto R_this = *(itC++);
                const auto R_next = *itC;

                const auto R_fwd = (R_this - R_next);
                const auto R_bak = (R_this - R_prev);
                const auto R_avg = (R_fwd + R_bak)*0.5;
                auto R_dir = R_avg.unit();

                R_dir = (R_this - R_cent).unit();
/*                

                //if(R_dir.length() < 0.5){ // Edge case: straight lines...
                { // Edge case: straight lines...
                    //Figure out which side is inside the contour and which side is outside. Move away from the inside.

                    const auto R_l = R_fwd.unit().rotate_around_z( M_PI)*0.05;
                    const auto R_r = R_fwd.unit().rotate_around_z(-M_PI)*0.05;
                    const auto PA = *itB + R_l;
                    const auto PB = *itB + R_r;
                    if(cop_orig.Is_Point_In_Polygon_Projected_Orthogonally(aplane, PA)){
                        R_dir = PA.unit();
                    }else{
                        R_dir = PB.unit();
                    }
                }
*/

                *itB += R_dir * dR;
            }
        }
    }

    return DICOM_data;
}
