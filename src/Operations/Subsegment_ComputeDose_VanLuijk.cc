//Subsegment_ComputeDose_VanLuijk.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

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

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.
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
#include "../Dose_Meld.h"

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
#include "../YgorImages_Functors/Processing/Cross_Second_Derivative.h"
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

#include "Subsegment_ComputeDose_VanLuijk.h"



std::list<OperationArgDoc> OpArgDocSubsegment_ComputeDose_VanLuijk(void){
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

    return out;
}



Drover Subsegment_ComputeDose_VanLuijk(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    //-----------------------------------------------------------------------------------------------------------------
    const auto theregex = std::regex(ROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto thenormalizedregex = std::regex(NormalizedROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    //Merge the dose arrays if necessary.
    if(DICOM_data.dose_data.empty()){
        throw std::invalid_argument("This routine requires at least one dose image array. Cannot continue");
    }
    DICOM_data.dose_data = Meld_Dose_Data(DICOM_data.dose_data);
    if(DICOM_data.dose_data.size() != 1){
        throw std::invalid_argument("Unable to meld doses into a single dose array. Cannot continue.");
    }

    //auto img_arr_ptr = DICOM_data.image_data.front();
    auto img_arr_ptr = DICOM_data.dose_data.front();

    if(img_arr_ptr == nullptr){
        throw std::runtime_error("Encountered a nullptr when expecting a valid Image_Array or Dose_Array ptr.");
    }else if(img_arr_ptr->imagecoll.images.empty()){
        throw std::runtime_error("Encountered a Image_Array or Dose_Array with valid images -- no images found.");
    }


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

    //Get the plane in which the contours are defined on.
    //
    // This is done by taking the cross product of the first few points of the first the first contour with three or
    // more points. We assume that all contours are defined by the same plane. What we are after is the normal that
    // defines the plane. We use this contour-only approach to avoid having to load CT image data.
    cc_ROIs.front().get().contours.front().Reorient_Counter_Clockwise();
    const auto planar_normal = cc_ROIs.front().get().contours.front().Estimate_Planar_Normal();

    //Perform the sub-segmentation bisection, building up lists of the split contours for each ROI.
    const double desired_total_area_fraction_above_plane = 0.25; //Here 'above' means in the positive normal direction.
    const double acceptable_deviation = 0.01; //Deviation from desired_total_area_fraction_above_plane.
    const size_t max_iters = 20; //If the tolerance cannot be reached after this many iters, report the current plane as-is.

    std::list<contour_collection<double>> cc_above;
    std::list<contour_collection<double>> cc_below;
    for(const auto &cc_opt : cc_ROIs){
        plane<double> final_plane;
        size_t iters_taken = 0;
        double final_area_frac = std::numeric_limits<double>::quiet_NaN();

        auto splits = cc_opt.get().Total_Area_Bisection_Along_Plane(planar_normal,
                                                            desired_total_area_fraction_above_plane,
                                                            acceptable_deviation,
                                                            max_iters,
                                                            &final_plane,
                                                            &iters_taken,
                                                            &final_area_frac);
        FUNCINFO("Using bisection, the fraction of planar area above the final plane was " << final_area_frac);
        FUNCINFO(iters_taken << " iterations were taken");
        if(false) for(auto it = splits.begin(); it != splits.end(); ++it){ it->Plot(); }
        cc_below.push_back( splits.front() );
        cc_above.push_back( splits.back() );
    }

    //Generate lists of reference wrappers to the split contours.
    std::list<std::reference_wrapper<contour_collection<double>>> cc_above_refs;
    std::list<std::reference_wrapper<contour_collection<double>>> cc_below_refs;
    for(auto &cc : cc_above) cc_above_refs.push_back( std::ref(cc) );
    for(auto &cc : cc_below) cc_below_refs.push_back( std::ref(cc) );

    //Accumulate the voxel intensity distributions for each group.
    AccumulatePixelDistributionsUserData ud_above;
    if(!img_arr_ptr->imagecoll.Compute_Images( AccumulatePixelDistributions, { },
                                           cc_above_refs, &ud_above )){
        throw std::runtime_error("Unable to accumulate pixel distributions (1).");
    }

    AccumulatePixelDistributionsUserData ud_below;
    if(!img_arr_ptr->imagecoll.Compute_Images( AccumulatePixelDistributions, { },
                                           cc_below_refs, &ud_below )){
        throw std::runtime_error("Unable to accumulate pixel distributions (2).");
    }

    //Report the findings.
    std::cout << "=== Above the bisected plane ===" << std::endl;
    for(const auto &av : ud_above.accumulated_voxels){
        const auto lROIname = av.first;
        const auto MeanDose = Stats::Mean( av.second );
        const auto MedianDose = Stats::Median( av.second );

        std::cout << "ROIname  = " << lROIname << std::endl;
        std::cout << "\t" << "Mean Dose   = " << MeanDose << std::endl;
        std::cout << "\t" << "Median Dose = " << MedianDose << std::endl;
        std::cout << "\t" << "Voxel Count = " << av.second.size() << std::endl;
    }
    std::cout << std::endl;

    std::cout << "=== Below the bisected plane ===" << std::endl;
    for(const auto &av : ud_below.accumulated_voxels){
        const auto lROIname = av.first;
        const auto MeanDose = Stats::Mean( av.second );
        const auto MedianDose = Stats::Median( av.second );

        std::cout << "ROIname  = " << lROIname << std::endl;
        std::cout << "\t" << "Mean Dose   = " << MeanDose << std::endl;
        std::cout << "\t" << "Median Dose = " << MedianDose << std::endl;
        std::cout << "\t" << "Voxel Count = " << av.second.size() << std::endl;
    }
    std::cout << std::endl;
 
    return std::move(DICOM_data);
}
