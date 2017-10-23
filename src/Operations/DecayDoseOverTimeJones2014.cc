//DecayDoseOverTimeJones2014.cc - A part of DICOMautomaton 2017. Written by hal clark.

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

#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorTime.h"         //Needed for time_mark class.
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
#include "../BED_Conversion.h"

#include "../YgorImages_Functors/Grouping/Misc_Functors.h"

#include "../YgorImages_Functors/Processing/CT_Convert_NaNs_to_Air.h"
#include "../YgorImages_Functors/Processing/CT_Perf_Pixel_Filter.h"
#include "../YgorImages_Functors/Processing/CT_Perfusion_Clip_Search.h"
#include "../YgorImages_Functors/Processing/CT_Reasonable_HU_Window.h"
#include "../YgorImages_Functors/Processing/Centralized_Moments.h"
#include "../YgorImages_Functors/Processing/Cross_Second_Derivative.h"
#include "../YgorImages_Functors/Processing/DBSCAN_Time_Courses.h"
#include "../YgorImages_Functors/Processing/DCEMRI_AUC_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_S0_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_T1_Map.h"
#include "../YgorImages_Functors/Processing/DecayDoseOverTime.h"
#include "../YgorImages_Functors/Processing/Highlight_ROI_Voxels.h"
#include "../YgorImages_Functors/Processing/IVIMMRI_ADC_Map.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bicubic_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bilinear_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Pixel_Decimate.h"
#include "../YgorImages_Functors/Processing/Kitchen_Sink_Analysis.h"
#include "../YgorImages_Functors/Processing/Logarithmic_Pixel_Scale.h"
#include "../YgorImages_Functors/Processing/Max_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/Min_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/Orthogonal_Slices.h"
#include "../YgorImages_Functors/Processing/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Processing/Slope_Difference.h"
#include "../YgorImages_Functors/Processing/Time_Course_Slope_Map.h"

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

#include "DecayDoseOverTimeJones2014.h"



std::list<OperationArgDoc> OpArgDocDecayDoseOverTimeJones2014(void){
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


    //out.emplace_back();
    //out.back().name = "Course1DosePerFraction";
    //out.back().desc = "The dose delivered per fraction (in Gray) for the first (i.e., previous) course."
    //                  " If several apply, you can provide a single effective fractionation scheme's 'd'.";
    //out.back().default_val = "2";
    //out.back().expected = true;
    //out.back().examples = { "1", "2", "2.33333", "10" };


    out.emplace_back();
    out.back().name = "Course1NumberOfFractions";
    out.back().desc = "The number of fractions delivered for the first (i.e., previous) course."
                      " If several apply, you can provide a single effective fractionation scheme's 'n'.";
    out.back().default_val = "35";
    out.back().expected = true;
    out.back().examples = { "15", "25", "30.001", "35.3" };


    out.emplace_back();
    out.back().name = "Course2NumberOfFractions";
    out.back().desc = "The number of fractions you want to deliver for the second (i.e., forthcoming) course."
                      " If several apply, you can provide a single effective fractionation scheme's 'n'.";
    out.back().default_val = "35";
    out.back().expected = true;
    out.back().examples = { "15", "25", "30.001", "35.3" };


    out.emplace_back();
    out.back().name = "ToleranceDosePerFraction";
    out.back().desc = "The dose delivered per fraction ('d', in Gray) for a hypothetical 'lifetime dose tolerance' course."
                      " This dose per fraction corresponds to a hypothetical radiation course that nominally"
                      " corresponds to the toxicity of interest. For CNS tissues, it will probably be myelopathy"
                      " or necrosis at some population-level onset risk (e.g., 5% risk of myelopathy)."
                      " The value provided will be converted to a BED_{a/b} so you can safely provide a 'nominal' value."
                      " Be aware that each voxel is treated independently, rather than treating OARs/ROIs as a whole."
                      " (Many dose limits reported in the literature use whole-ROI D_mean or D_max, and so may be"
                      " not be directly applicable to per-voxel risk estimation!) Note that the QUANTEC 2010 reports"
                      " almost all assume 2 Gy/fraction."
                      " If several 'd' apply, you can provide a single effective fractionation scheme's 'd'.";
    out.back().default_val = "2";
    out.back().expected = true;
    out.back().examples = { "1", "2", "2.33333", "10" };


    out.emplace_back();
    out.back().name = "ToleranceNumberOfFractions";
    out.back().desc = "The number of fractions ('n') the 'lifetime dose tolerance' toxicity you are interested in."
                      " Note that this is converted to a BED_{a/b} so you can safely provide a 'nominal' value."
                      " If several apply, you can provide a single effective fractionation scheme's 'n'.";
    out.back().default_val = "35";
    out.back().expected = true;
    out.back().examples = { "15", "25", "30.001", "35.3" };


    out.emplace_back();
    out.back().name = "TemporalGapOverride";
    out.back().desc = "The number of months between radiotherapy courses. Note that this is normally estimated by"
                      " (1) extracting study/series dates from the provided dose files and (2) using the current"
                      " date as the second course date. Use this parameter to override the autodetection gap."
                      " Note: if the provided value is negative, autodetection will be used.";
    out.back().default_val = "-1";
    out.back().expected = true;
    out.back().examples = { "0", "12", "38" };


    out.emplace_back();
    out.back().name = "AlphaBetaRatio";
    out.back().desc = "The ratio alpha/beta (in Gray) to use when converting to a biologically-equivalent"
                      " dose distribution for central nervous tissues. "
                      " Jones and Grant (2014) recommend alpha/beta = 2 Gy to be conservative. "
                      " It is more commonplace to use alpha/beta = 3 Gy, but this is less conservative and there "
                      " is some evidence that it may be erroneous to use 3 Gy.";
    out.back().default_val = "2";
    out.back().expected = true;
    out.back().examples = { "2", "3" };
    

    out.emplace_back();
    out.back().name = "UseMoreConservativeRecovery";
    out.back().desc = "Jones and Grant (2014) provide two ways to estimate the function 'r'. One is fitted to"
                      " experimental data, and one is a more conservative estimate of the fitted function."
                      " This parameter controls whether or not the more conservative function is used.";
    out.back().default_val = "true";
    out.back().expected = true;
    out.back().examples = { "true", "false" };

    
    return out;
}



Drover DecayDoseOverTimeJones2014(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    // This operation transforms a dose map (assumed to be delivered some time in the past) to 'decay' or 'evaporate' or
    // 'forgive' some of the dose using the time-dependent model of Jones and Grant (2014;
    // doi:10.1016/j.clon.2014.04.027). This model is specific to reirradiation of central nervous tissues. See
    // the Jones and Grant paper or 'Nasopharyngeal Carcinoma' by Wai Tong Ng et al. (2016; doi:10.1007/174_2016_48) for
    // more information.
    //
    // Note: this routine uses image_arrays so convert dose_arrays beforehand.
    //
    // Note: this routine will combine spatially-overlapping images by summing voxel intensities. So if you have a time
    //       course it may be more sensible to aggregate images in some way (e.g., spatial averaging) prior to calling
    //       this routine.
    //

    DecayDoseOverTimeUserData ud;
    ud.model = DecayDoseOverTimeMethod::Jones_and_Grant_2014; 
    ud.channel = -1; // -1 ==> all channels.

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();

    //ud.Course1DosePerFraction = std::stod( OptArgs.getValueStr("Course1DosePerFraction").value() );
    ud.Course1NumberOfFractions = std::stod( OptArgs.getValueStr("Course1NumberOfFractions").value() );

    ud.Course2NumberOfFractions = std::stod( OptArgs.getValueStr("Course2NumberOfFractions").value() );

    ud.ToleranceDosePerFraction = std::stod( OptArgs.getValueStr("ToleranceDosePerFraction").value() );
    ud.ToleranceNumberOfFractions = std::stod( OptArgs.getValueStr("ToleranceNumberOfFractions").value() );

    const auto TemporalGapOverride_str = OptArgs.getValueStr("TemporalGapOverride").value();

    ud.AlphaBetaRatio = std::stod( OptArgs.getValueStr("AlphaBetaRatio").value() );

    const auto UseMoreConservativeRecovery_str = OptArgs.getValueStr("UseMoreConservativeRecovery").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto TrueRegex = std::regex("^tr?u?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto IsPositiveFloat = std::regex("^[0-9.]*$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto theregex = std::regex(ROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto thenormalizedregex = std::regex(NormalizedROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);


    ud.UseMoreConservativeRecovery = std::regex_match(UseMoreConservativeRecovery_str, TrueRegex);

    Explicator X(FilenameLex);

    //Merge the image arrays if necessary.
    if(DICOM_data.image_data.empty()){
        throw std::invalid_argument("This routine requires at least one image array. Cannot continue");
    }

    auto img_arr_ptr = DICOM_data.image_data.front();
    if(img_arr_ptr == nullptr){
        throw std::runtime_error("Encountered a nullptr when expecting a valid Image_Array ptr.");
    }else if(img_arr_ptr->imagecoll.images.empty()){
        throw std::runtime_error("Encountered a Image_Array with valid images -- no images found.");
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

    ud.TemporalGapMonths = 0.0;
    if(std::regex_match(TemporalGapOverride_str,IsPositiveFloat)){
        ud.TemporalGapMonths = std::stod(TemporalGapOverride_str);
        FUNCINFO("Overriding temporal gap with user-provided value of: " << ud.TemporalGapMonths);
    }else{
        auto study_dates = img_arr_ptr->imagecoll.get_all_values_for_key("StudyDate");
        for(const auto &adate : study_dates){
            // "Massage" the date to something we can easily process. Want ~"20171022-010000".
            const auto massaged = PurgeCharsFromString(adate, " -/_+,.") + "-010000";
            time_mark t2;
            if(t2.Read_from_string(massaged)){
                time_mark tnow;
                const auto dt = static_cast<double>(t2.Diff_in_Days(tnow));
                ud.TemporalGapMonths = dt / 30.4;
                FUNCINFO("Based on provided data and current date, assuming temporal gap is: " << ud.TemporalGapMonths);
                break;
            }
        }
    }

    // Clamp the temporal gap as per the Jones and Grant (2014) model: from 0y to 3y.
    if(ud.TemporalGapMonths <  0) ud.TemporalGapMonths =  0.0;
    if(ud.TemporalGapMonths > 36) ud.TemporalGapMonths = 36.0; 

    // Perform the dose modification.
    if(!img_arr_ptr->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                        DecayDoseOverTime,
                                                        {}, cc_ROIs, &ud )){
        throw std::runtime_error("Unable to decay dose (Jones and Grant 2014 model).");
    }

    return std::move(DICOM_data);
}
