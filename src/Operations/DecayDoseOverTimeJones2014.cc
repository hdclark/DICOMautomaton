//DecayDoseOverTimeJones2014.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <experimental/any>
#include <experimental/optional>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/DecayDoseOverTime.h"
#include "DecayDoseOverTimeJones2014.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorTime.h"         //Needed for time_mark class.



OperationDoc OpArgDocDecayDoseOverTimeJones2014(void){
    OperationDoc out;
    out.name = "DecayDoseOverTimeJones2014";

    out.desc = 
        "This operation transforms a dose map (assumed to be delivered some time in the past) to 'decay' or 'evaporate' or"
        " 'forgive' some of the dose using the time-dependent model of Jones and Grant (2014;"
        " doi:10.1016/j.clon.2014.04.027). This model is specific to reirradiation of central nervous tissues. See"
        " the Jones and Grant paper or 'Nasopharyngeal Carcinoma' by Wai Tong Ng et al. (2016; doi:10.1007/174_2016_48) for"
        " more information.";
        
    out.notes.emplace_back(
        "This routine uses image_arrays so convert dose_arrays beforehand."
    );
        
    out.notes.emplace_back(
        "This routine will combine spatially-overlapping images by summing voxel intensities. So if you have a time"
        " course it may be more sensible to aggregate images in some way (e.g., spatial averaging) prior to calling"
        " this routine."
    );


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
    out.args.back().name = "Course1NumberOfFractions";
    out.args.back().desc = "The number of fractions delivered for the first (i.e., previous) course."
                      " If several apply, you can provide a single effective fractionation scheme's 'n'.";
    out.args.back().default_val = "35";
    out.args.back().expected = true;
    out.args.back().examples = { "15", "25", "30.001", "35.3" };


    out.args.emplace_back();
    out.args.back().name = "ToleranceTotalDose";
    out.args.back().desc = "The dose delivered (in Gray) for a hypothetical 'lifetime dose tolerance' course."
                      " This dose corresponds to a hypothetical radiation course that nominally"
                      " corresponds to the toxicity of interest. For CNS tissues, it will probably be myelopathy"
                      " or necrosis at some population-level onset risk (e.g., 5% risk of myelopathy)."
                      " The value provided will be converted to a BED_{a/b} so you can safely provide a 'nominal' value."
                      " Be aware that each voxel is treated independently, rather than treating OARs/ROIs as a whole."
                      " (Many dose limits reported in the literature use whole-ROI D_mean or D_max, and so may be"
                      " not be directly applicable to per-voxel risk estimation!) Note that the QUANTEC 2010 reports"
                      " almost all assume 2 Gy/fraction."
                      " If several fractionation schemes were used, you should provide a cumulative BED-derived dose here.";
    out.args.back().default_val = "52";
    out.args.back().expected = true;
    out.args.back().examples = { "15", "20", "25", "50", "83.2" };


    out.args.emplace_back();
    out.args.back().name = "ToleranceNumberOfFractions";
    out.args.back().desc = "The number of fractions ('n') the 'lifetime dose tolerance' toxicity you are interested in."
                      " Note that this is converted to a BED_{a/b} so you can safely provide a 'nominal' value."
                      " If several apply, you can provide a single effective fractionation scheme's 'n'.";
    out.args.back().default_val = "35";
    out.args.back().expected = true;
    out.args.back().examples = { "15", "25", "30.001", "35.3" };


    out.args.emplace_back();
    out.args.back().name = "TimeGap";
    out.args.back().desc = "The number of years between radiotherapy courses. Note that this is normally estimated by"
                      " (1) extracting study/series dates from the provided dose files and (2) using the current"
                      " date as the second course date. Use this parameter to override the autodetected gap time."
                      " Note: if the provided value is negative, autodetection will be used.";
    out.args.back().default_val = "-1";
    out.args.back().expected = true;
    out.args.back().examples = { "0.91", "2.6", "5" };


    out.args.emplace_back();
    out.args.back().name = "AlphaBetaRatio";
    out.args.back().desc = "The ratio alpha/beta (in Gray) to use when converting to a biologically-equivalent"
                      " dose distribution for central nervous tissues. "
                      " Jones and Grant (2014) recommend alpha/beta = 2 Gy to be conservative. "
                      " It is more commonplace to use alpha/beta = 3 Gy, but this is less conservative and there "
                      " is some evidence that it may be erroneous to use 3 Gy.";
    out.args.back().default_val = "2";
    out.args.back().expected = true;
    out.args.back().examples = { "2", "3" };
    

    out.args.emplace_back();
    out.args.back().name = "UseMoreConservativeRecovery";
    out.args.back().desc = "Jones and Grant (2014) provide two ways to estimate the function 'r'. One is fitted to"
                      " experimental data, and one is a more conservative estimate of the fitted function."
                      " This parameter controls whether or not the more conservative function is used.";
    out.args.back().default_val = "true";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };

    
    return out;
}



Drover DecayDoseOverTimeJones2014(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    DecayDoseOverTimeUserData ud;
    ud.model = DecayDoseOverTimeMethod::Jones_and_Grant_2014; 
    ud.channel = -1; // -1 ==> all channels.

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();

    //ud.Course1DosePerFraction = std::stod( OptArgs.getValueStr("Course1DosePerFraction").value() );
    ud.Course1NumberOfFractions = std::stod( OptArgs.getValueStr("Course1NumberOfFractions").value() );

    ud.ToleranceTotalDose = std::stod( OptArgs.getValueStr("ToleranceTotalDose").value() );
    ud.ToleranceNumberOfFractions = std::stod( OptArgs.getValueStr("ToleranceNumberOfFractions").value() );

    const auto TemporalGapOverride_str = OptArgs.getValueStr("TimeGap").value();

    ud.AlphaBetaRatio = std::stod( OptArgs.getValueStr("AlphaBetaRatio").value() );

    const auto UseMoreConservativeRecovery_str = OptArgs.getValueStr("UseMoreConservativeRecovery").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto TrueRegex = std::regex("^tr?u?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto IsPositiveFloat = std::regex("^[0-9.]*$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);



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
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    ud.TemporalGapMonths = 0.0;
    if(std::regex_match(TemporalGapOverride_str,IsPositiveFloat)){
        ud.TemporalGapMonths = std::stod(TemporalGapOverride_str) * 12.0;
        FUNCINFO("Overriding temporal gap with user-provided value of: " << ud.TemporalGapMonths << " months");
    }else{
        auto study_dates = img_arr_ptr->imagecoll.get_all_values_for_key("StudyDate");
        for(const auto &adate : study_dates){
            // "Massage" the date to something we can easily process. Want ~"20171022-010000".
            const auto massaged = PurgeCharsFromString(adate, " -/_+,.") + "-010000";
            time_mark t2;
            if(t2.Read_from_string(massaged)){
                time_mark tnow;
                const auto dt = static_cast<double>(t2.Diff_in_Days(tnow));
                ud.TemporalGapMonths = dt / 30.4375;
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

    return DICOM_data;
}
