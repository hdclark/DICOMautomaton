//CT_Liver_Perfusion_Pharmaco_1C2I_5Param.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <array>
#include <cmath>
#include <cstdlib>           //quick_exit(), EXIT_SUCCESS.
#include <any>
#include <optional>
#include <fstream>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>
#include <cstdint>

#include "YgorAlgorithms.h"   //Needed for For_Each_In_Parallel<..>(...)
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathBSpline.h"   //Needed for basis_spline class.
#include "YgorMathChebyshev.h" //Needed for cheby_approx class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Common_Plotting.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Compute/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/CT_Reasonable_HU_Window.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Pixel_Decimate.h"
#include "../YgorImages_Functors/Processing/Liver_Kinetic_1Compartment2Input_5Param_Chebyshev_Common.h"
#include "../YgorImages_Functors/Processing/Liver_Kinetic_1Compartment2Input_5Param_Chebyshev_LevenbergMarquardt.h"
#include "../YgorImages_Functors/Processing/Liver_Kinetic_1Compartment2Input_5Param_LinearInterp_Common.h"
#include "../YgorImages_Functors/Processing/Liver_Kinetic_1Compartment2Input_5Param_LinearInterp_LevenbergMarquardt.h"
#include "../YgorImages_Functors/Processing/Liver_Kinetic_Common.h"
#include "../YgorImages_Functors/Processing/Min_Pixel_Value.h"
#include "../YgorImages_Functors/Transform/CT_Perfusion_Signal_Diff.h"
#include "CT_Liver_Perfusion_Pharmaco_1Compartment2Input_5Param.h"


OperationDoc OpArgDocCT_Liver_Perfusion_Pharmaco_1C2I_5Param(){
    OperationDoc out;
    out.name = "CT_Liver_Perfusion_Pharmaco_1C2I_5Param";

    out.tags.emplace_back("category: image processing");
    out.tags.emplace_back("category: perfusion");

    out.desc = 
        "This operation performed dynamic contrast-enhanced CT perfusion image modeling on a time series image volume.";

    out.args.emplace_back();
    out.args.back().name = "AIFROINameRegex";
    out.args.back().desc = "Regex for the name of the ROI to use as the AIF. It should generally be a"
                      " major artery near the trunk or near the tissue of interest.";
    out.args.back().default_val = "Abdominal_Aorta";
    out.args.back().expected = true;
    out.args.back().examples = { "Abdominal_Aorta",
                            ".*Aorta.*",
                            "Major_Artery" };


    out.args.emplace_back();
    out.args.back().name = "ExponentialKernelCoeffTruncation";
    out.args.back().desc = "Control the number of Chebyshev coefficients used to approximate the exponential"
                      " kernel. Usually ~10 will suffice. ~20 is probably overkill, and ~5 is probably"
                      " too few. It is probably better to err on the side of caution and enlarge this"
                      " number if you're worried about loss of precision -- this will slow the computation"
                      " somewhat. (You might be able to offset by retaining fewer coefficients in"
                      " Chebyshev multiplication; see 'FastChebyshevMultiplication' parameter.)";
    out.args.back().default_val = "10";
    out.args.back().expected = true;
    out.args.back().examples = { "20",
                            "15",
                            "10",
                            "5"};

    out.args.emplace_back();
    out.args.back().name = "FastChebyshevMultiplication";
    out.args.back().desc = "Control coefficient truncation/pruning to speed up Chebyshev polynomial multiplication."
                      " (This setting does nothing if the Chebyshev method is not being used.)"
                      " The choice of this number depends on how much precision you are willing to forgo."
                      " It also strongly depends on the number of datum in the AIF, VIF, and the number"
                      " of coefficients used to approximate the exponential kernel (usually ~10 suffices)."
                      " Numbers are specified relative to max(N,M), where N and M are the number of"
                      " coefficients in the two Chebyshev expansions taking part in the multiplication."
                      " If too many coefficients are requested (i.e., more than (N+M-2)) then the full"
                      " non-approximate multiplication is carried out.";
    out.args.back().default_val = "*10000000.0";
    out.args.back().expected = true;
    out.args.back().examples = { "*2.0",
                            "*1.5",
                            "*1.0",
                            "*0.5",
                            "*0.3"};

    out.args.emplace_back();
    out.args.back().name = "PlotAIFVIF";
    out.args.back().desc = "Control whether the AIF and VIF should be shown prior to modeling.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true",
                            "false" };

    out.args.emplace_back();
    out.args.back().name = "PlotPixelModel";
    out.args.back().desc = "Show a plot of the fitted model for a specified pixel. Plotting happens "
                      " immediately after the pixel is processed. You can supply arbitrary"
                      " metadata, but must also supply Row and Column numbers. Note that numerical "
                      " comparisons are performed lexically, so you have to be exact. Also note the"
                      " sub-separation token is a semi-colon, not a colon.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "Row@12;Column@4;Description@.*k1A.*",
                            "Row@256;Column@500;SliceLocation@23;SliceThickness@0.5",
                            "Row@256;Column@500;Some@thing#Row@256;Column@501;Another@thing",
                            "Row@0;Column@5#Row@4;Column@5#Row@8;Column@5#Row@12;Column@5"};

    out.args.emplace_back();
    out.args.back().name = "PreDecimateOutSizeR";
    out.args.back().desc = "The number of pixels along the row unit vector to group into an outgoing pixel."
                      " This optional step can reduce computation effort by downsampling (decimating)"
                      " images before computing fitted parameter maps (but *after* computing AIF and"
                      " VIF time courses)."
                      " Must be a multiplicative factor of the incoming image's row count."
                      " No decimation occurs if either this or 'PreDecimateOutSizeC' is zero or negative.";
    out.args.back().default_val = "8";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "2", "4", "8", "16", "32", "64", "128", "256", "512" };

    out.args.emplace_back();
    out.args.back().name = "PreDecimateOutSizeC";
    out.args.back().desc = "The number of pixels along the column unit vector to group into an outgoing pixel."
                      " This optional step can reduce computation effort by downsampling (decimating)"
                      " images before computing fitted parameter maps (but *after* computing AIF and"
                      " VIF time courses)."
                      " Must be a multiplicative factor of the incoming image's column count."
                      " No decimation occurs if either this or 'PreDecimateOutSizeR' is zero or negative.";
    out.args.back().default_val = "8";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "2", "4", "8", "16", "32", "64", "128", "256", "512" };

    out.args.emplace_back();
    out.args.back().name = "TargetROINameRegex";
    out.args.back().desc = "Regex for the name of the ROI to perform modeling within. The largest contour is"
                      " usually what you want, but you can also be more focused.";
    out.args.back().default_val = ".*Body.*";
    out.args.back().expected = true;
    out.args.back().examples = { "Liver_Patches_For_Testing_Smaller",
                            "Liver_Patches_For_Testing",
                            "Suspected_Liver_Rough",
                            "Rough_Body",
                            ".*body.*",
                            ".*something.*\\|.*another.*thing.*" };

    out.args.emplace_back();
    out.args.back().name = "UseBasisSplineInterpolation";
    out.args.back().desc = "Control whether the AIF and VIF should use basis spline interpolation in"
                      " conjunction with the Chebyshev polynomial method. If this option is not"
                      " set, linear interpolation is used instead. Linear interpolation may"
                      " result in a less-smooth AIF and VIF (and therefore possibly slower "
                      " optimizer convergence), but is safer if you cannot verify"
                      " the AIF and VIF plots are reasonable. This option currently produces an effect"
                      " only if the Chebyshev polynomial method is being used.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true",
                            "false" };

    out.args.emplace_back();
    out.args.back().name = "BasisSplineCoefficients";
    out.args.back().desc = "Control the number of basis spline coefficients to use, if applicable."
                      " (This setting does nothing when basis splines are not being used.)"
                      " Valid options for this setting depend on the amount of data and b-spline order."
                      " This number controls the number of coefficients that are fitted (via least-squares)."
                      " You must verify that overfitting is not happening. If in doubt, use fewer coefficients."
                      " There are two ways to specify the number: relative and absolute."
                      " Relative means relative to the number of datum."
                      " For example, if the AIF and VIF have ~40 datum then generally '*0.5' is safe."
                      " ('*0.5' means there are half the number of coefficients as datum.)"
                      " Inspect for overfitting and poor fit."
                      " Because this routine happens once and is fast, do not tweak to optimize for speed;"
                      " the aim of this method is to produce a smooth and accurate AIF and VIF."
                      " Because an integer number of coefficients are needed, so rounding is used."
                      " You can also specify the absolute number of coefficients to use like '20'."
                      " It often makes more sense to use relative specification."
                      " Be aware that not all inputs can be honoured due to limits on b-spline knots and breaks,"
                      " and may cause unpredictable behaviour or internal failure.";
    out.args.back().default_val = "*0.5";
    out.args.back().expected = true;
    out.args.back().examples = { "*0.8",
                            "*0.5",
                            "*0.3",
                            "20.0",
                            "10.0"};

    out.args.emplace_back();
    out.args.back().name = "BasisSplineOrder";
    out.args.back().desc = "Control the polynomial order of basis spline interpolation to use, if applicable."
                      " (This setting does nothing when basis splines are not being used.)"
                      " This parameter controls the order of polynomial used for b-spline interpolation,"
                      " and therefore has ramifications for the computability and numerical stability of"
                      " AIF and VIF derivatives. Stick with '4' or '5' if you're unsure.";
    out.args.back().default_val = "4";
    out.args.back().expected = true;
    out.args.back().examples = { "1",
                            "2",
                            "3",
                            "4",
                            "5",
                            "6",
                            "7",
                            "8",
                            "9",
                            "10"};

    out.args.emplace_back();
    out.args.back().name = "UseChebyshevPolyMethod";
    out.args.back().desc = "Control whether the AIF and VIF should be approximated by Chebyshev polynomials."
                      " If this option is not set, a inear interpolation approach is used instead.";
    out.args.back().default_val = "true";
    out.args.back().expected = true;
    out.args.back().examples = { "true",
                            "false" };


    out.args.emplace_back();
    out.args.back().name = "ChebyshevPolyCoefficients";
    out.args.back().desc = "Control the number of Chebyshev polynomial coefficients to use, if applicable."
                      " (This setting does nothing when the Chebyshev polynomial method is not being used.)"
                      " This number controls the number of coefficients that are computed."
                      " There are two ways to specify the number: relative and absolute."
                      " Relative means relative to the number of datum."
                      " For example, if the AIF and VIF have ~40 datum then generally '*2' is safe."
                      " ('*2' means there are 2x the number of coefficients as datum; usually overkill.)"
                      " A good middle-ground is '*1' which is faster but should produce similar results."
                      " For speed '/2' is even faster, but can produce bad results in some cases."
                      " Because an integer number of coefficients are needed, rounding is used."
                      " You can also specify the absolute number of coefficients to use like '20'."
                      " It often makes more sense to use relative specification."
                      " Be aware that not all inputs can be honoured (i.e., too large, too small, or negative),"
                      " and may cause unpredictable behaviour or internal failure.";
    out.args.back().default_val = "*2.0";
    out.args.back().expected = true;
    out.args.back().examples = { "*10.0",
                            "*5.0",
                            "*2.0",
                            "*1.23",
                            "*1.0",
                            "/1.0",
                            "/2.0",
                            "/3.0",
                            "/5.0",
                            "100.0",
                            "50.0",
                            "20",
                            "10.01"};


    out.args.emplace_back();
    out.args.back().name = "VIFROINameRegex";
    out.args.back().desc = "Regex for the name of the ROI to use as the VIF. It should generally be a"
                      " major vein near the trunk or near the tissue of interest.";
    out.args.back().default_val = "Hepatic_Portal_Vein";
    out.args.back().expected = true;
    out.args.back().examples = { "Hepatic_Portal_Vein",
                            ".*Portal.*Vein.*",
                            "Major_Vein" };

    return out;
}



bool CT_Liver_Perfusion_Pharmaco_1C2I_5Param(Drover &DICOM_data,
                                               const OperationArgPkg& OptArgs,
                                               std::map<std::string, std::string>& InvocationMetadata,
                                               const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto AIFROIName = OptArgs.getValueStr("AIFROINameRegex").value();
    const int64_t ExponentialKernelCoeffTruncation = std::stol( OptArgs.getValueStr("ExponentialKernelCoeffTruncation").value() );
    const auto FastChebyshevMultiplicationStr = OptArgs.getValueStr("FastChebyshevMultiplication").value();
    const auto PlotAIFVIF = OptArgs.getValueStr("PlotAIFVIF").value();
    const auto PlotPixelModel = OptArgs.getValueStr("PlotPixelModel").value();
    const int64_t PreDecimateR = std::stol( OptArgs.getValueStr("PreDecimateOutSizeR").value() );
    const int64_t PreDecimateC = std::stol( OptArgs.getValueStr("PreDecimateOutSizeC").value() );
    const auto TargetROIName = OptArgs.getValueStr("TargetROINameRegex").value();
    const auto UseBasisSplineInterpolationStr = OptArgs.getValueStr("UseBasisSplineInterpolation").value();
    const auto BasisSplineCoefficientsStr = OptArgs.getValueStr("BasisSplineCoefficients").value();
    const auto BasisSplineOrder = std::stol( OptArgs.getValueStr("BasisSplineOrder").value() );

    const auto UseChebyshevPolyMethodStr = OptArgs.getValueStr("UseChebyshevPolyMethod").value();
    const auto ChebyshevPolyCoefficientsStr = OptArgs.getValueStr("ChebyshevPolyCoefficients").value();

    const auto VIFROIName = OptArgs.getValueStr("VIFROINameRegex").value();
    //-----------------------------------------------------------------------------------------------------------------
    const auto AIFROINameRegex = Compile_Regex(AIFROIName);
    const auto VIFROINameRegex = Compile_Regex(VIFROIName);
    const auto TargetROINameRegex = Compile_Regex(TargetROIName);
    const auto TrueRegex = Compile_Regex("^tr?u?e?$");
    const auto IsPositiveInteger = Compile_Regex("^[0-9]*$");
    const auto IsPositiveFloat = Compile_Regex("^[0-9.]*$");
    const auto IsRelativePosFloat = Compile_Regex("^[*][0-9.]*$");


    //Figure out how many coefficients to use.
    double BasisSplineCoefficients;
    bool BasisSplineCoefficientsRelative;
    if(std::regex_match(BasisSplineCoefficientsStr, IsPositiveFloat)){
        BasisSplineCoefficients = std::stod( BasisSplineCoefficientsStr );
        BasisSplineCoefficientsRelative = false;
    }else if(std::regex_match(BasisSplineCoefficientsStr, IsRelativePosFloat)){
        BasisSplineCoefficients = std::stod( BasisSplineCoefficientsStr.substr(1,std::string::npos) );
        BasisSplineCoefficientsRelative = true;
    }else{
        throw std::invalid_argument("Unable to handle '" + BasisSplineCoefficientsStr + "'");
    }
    if(BasisSplineCoefficients <= 0){
        throw std::invalid_argument("Option invalid: '" + BasisSplineCoefficientsStr + "'");
    }

    double ChebyshevPolyCoefficients;
    bool ChebyshevPolyCoefficientsRelative;
    if(std::regex_match(ChebyshevPolyCoefficientsStr, IsPositiveFloat)){
        ChebyshevPolyCoefficients = std::stod( ChebyshevPolyCoefficientsStr );
        ChebyshevPolyCoefficientsRelative = false;
    }else if(std::regex_match(ChebyshevPolyCoefficientsStr, IsRelativePosFloat)){
        ChebyshevPolyCoefficients = std::stod( ChebyshevPolyCoefficientsStr.substr(1,std::string::npos) );
        ChebyshevPolyCoefficientsRelative = true;
    }else{
        throw std::invalid_argument("Unable to handle '" + ChebyshevPolyCoefficientsStr + "'");
    }
    if(ChebyshevPolyCoefficients <= 0){
        throw std::invalid_argument("Option invalid: '" + ChebyshevPolyCoefficientsStr + "'");
    }

    double FastChebyshevMultiplication;
    if(std::regex_match(FastChebyshevMultiplicationStr, IsRelativePosFloat)){
        FastChebyshevMultiplication = std::stod( FastChebyshevMultiplicationStr.substr(1,std::string::npos) );
    }else{
        throw std::invalid_argument("Unable to handle '" + FastChebyshevMultiplicationStr + "'");
    }



    //Boolean options.
    const auto ShouldPlotAIFVIF = std::regex_match(PlotAIFVIF, TrueRegex);
    const auto UseBasisSplineInterpolation = std::regex_match(UseBasisSplineInterpolationStr, TrueRegex);
    const auto UseChebyshevPolyMethod = std::regex_match(UseChebyshevPolyMethodStr, TrueRegex);


    //Tokenize the plotting criteria.
    std::list<KineticModel_PixelSelectionCriteria> pixels_to_plot;
    const auto RowRegex = Compile_Regex("row");
    const auto ColRegex = Compile_Regex("column");
    for(const auto& a : SplitStringToVector(PlotPixelModel, '#', 'd')){
        pixels_to_plot.emplace_back();
        pixels_to_plot.back().row = -1;
        pixels_to_plot.back().column = -1;
        for(const auto& b : SplitStringToVector(a, ';', 'd')){
            auto c = SplitStringToVector(b, '@', 'd');
            if(c.size() != 2) throw std::runtime_error("Cannot parse subexpression: "_s + b);

            if(std::regex_match(c.front(),RowRegex)){
                pixels_to_plot.back().row = std::stol(c.back());
            }else if(std::regex_match(c.front(),ColRegex)){
                pixels_to_plot.back().column = std::stol(c.back());
            }else{
                pixels_to_plot.back().metadata_criteria.insert( { c.front(), 
                    Compile_Regex(c.back())
                } );
            }
        }
    }

    // ---------------------------------------------

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    std::list<std::reference_wrapper<contour_collection<double>>> cc_all;
    DICOM_data.Ensure_Contour_Data_Allocated();
    for(auto & cc : DICOM_data.contour_data->ccs){
        auto base_ptr = reinterpret_cast<contour_collection<double> *>(&cc);
        cc_all.push_back( std::ref(*base_ptr) );
    }

    //Get handles for each of the original image arrays so we can easily refer to them later.
    std::vector<std::shared_ptr<Image_Array>> orig_img_arrays;
    for(auto & img_arr : DICOM_data.image_data) orig_img_arrays.push_back(img_arr);


    //Force the window to something reasonable to be uniform and cover normal tissue HU range.
    if(true) for(auto & img_arr : orig_img_arrays){
        if(!img_arr->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                               StandardAbdominalHUWindow,
                                               {}, {} )){
            throw std::runtime_error("Unable to force window to cover reasonable HU range");
        }
    }

    std::map<std::string,std::string> l_InvocationMetadata(InvocationMetadata);

    //Look for relevant invocation metadata.
    double ContrastInjectionLeadTime = 6.0; //Seconds. 
    if(l_InvocationMetadata.count("ContrastInjectionLeadTime") == 0){
        YLOGWARN("Unable to locate 'ContrastInjectionLeadTime' invocation metadata key. Assuming the default lead time "
                 << ContrastInjectionLeadTime << "s is appropriate");
    }else{
        ContrastInjectionLeadTime = std::stod( l_InvocationMetadata["ContrastInjectionLeadTime"] );
        if(ContrastInjectionLeadTime < 0.0) throw std::runtime_error("Non-sensical 'ContrastInjectionLeadTime' found.");
        YLOGINFO("Found 'ContrastInjectionLeadTime' invocation metadata key. Using value " << ContrastInjectionLeadTime << "s");
    }

    double ContrastInjectionWashoutTime = 60.0; //Seconds.
    if(l_InvocationMetadata.count("ContrastInjectionWashoutTime") == 0){
        YLOGWARN("Unable to locate 'ContrastInjectionWashoutTime' invocation metadata key. Assuming the default lead time "
                 << ContrastInjectionWashoutTime << "s is appropriate");
    }else{
        ContrastInjectionWashoutTime = std::stod( l_InvocationMetadata["ContrastInjectionWashoutTime"] );
        if(ContrastInjectionWashoutTime < 0.0) throw std::runtime_error("Non-sensical 'ContrastInjectionWashoutTime' found.");
        YLOGINFO("Found 'ContrastInjectionWashoutTime' invocation metadata key. Using value " << ContrastInjectionWashoutTime << "s");
    }

    //Whitelist contours. Also rename the remaining into either "AIF" or "VIF".
    auto cc_AIF_VIF = cc_all;
    cc_AIF_VIF.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("ROIName");
                   if(!ROINameOpt) return true; //Remove those without names.
                   const auto& ROIName = ROINameOpt.value();
                   const auto MatchesAIF = std::regex_match(ROIName,AIFROINameRegex);
                   const auto MatchesVIF = std::regex_match(ROIName,VIFROINameRegex);
                   if(!MatchesAIF && !MatchesVIF) return true;

                   //Keep them, but rename them all.
                   for(auto &acontour : cc.get().contours){
                       acontour.metadata["ROIName"] = MatchesAIF ? "AIF" : "VIF";
                   }
                   return false;
    });


    //Compute a baseline with which we can use later to compute signal enhancement.
    std::vector<std::shared_ptr<Image_Array>> baseline_img_arrays;
    if(true){
        //Baseline = temporally averaged pre-contrast-injection signal.

        auto PurgeAboveNSeconds = std::bind(PurgeAboveTemporalThreshold, std::placeholders::_1, ContrastInjectionLeadTime);

        for(auto & img_arr : orig_img_arrays){
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
            baseline_img_arrays.emplace_back( DICOM_data.image_data.back() );
    
            baseline_img_arrays.back()->imagecoll.Prune_Images_Satisfying(PurgeAboveNSeconds);
    
            if(!baseline_img_arrays.back()->imagecoll.Condense_Average_Images(GroupSpatiallyOverlappingImages)){
                throw std::runtime_error("Cannot temporally average data set. Is it able to be averaged?");
            }
        }
    
    }else{
        //Baseline = minimum of signal over whole time course (minimum is usually pre-contrast, but noise
        // can affect the result).

        for(auto & img_arr : orig_img_arrays){
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
            baseline_img_arrays.emplace_back( DICOM_data.image_data.back() );
    
            if(!baseline_img_arrays.back()->imagecoll.Process_Images_Parallel( GroupSpatiallyOverlappingImages,
                                                                      CondenseMinPixel,
                                                                      {}, {} )){
                throw std::runtime_error("Unable to generate min(pixel) images over the time course");
            }
        }
    }


    //Deep-copy the original long image array and use the baseline map to work out 
    // approximate contrast enhancement in each voxel.
    std::vector<std::shared_ptr<Image_Array>> C_enhancement_img_arrays;
    {
        auto img_arr = orig_img_arrays.front();
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        C_enhancement_img_arrays.emplace_back( DICOM_data.image_data.back() );

        if(!C_enhancement_img_arrays.back()->imagecoll.Transform_Images( CTPerfusionSigDiffC,
                                                                         { baseline_img_arrays.front()->imagecoll },
                                                                         { } )){
            throw std::runtime_error("Unable to transform image array to make poor-man's C map");
        }
    }
       
    //Eliminate some images to relieve some memory pressure.
    if(true){
        for(auto & img_arr : orig_img_arrays){
            img_arr->imagecoll.images.clear();
        }
        for(auto & img_arr : baseline_img_arrays){
            img_arr->imagecoll.images.clear();
        }
    }
 
    //Compute some aggregate C(t) curves from the available ROIs. We especially want the 
    // portal vein and ascending aorta curves.
    ComputePerROITimeCoursesUserData ud; // User Data.
    if(true) for(auto & img_arr : C_enhancement_img_arrays){
        if(!img_arr->imagecoll.Compute_Images( ComputePerROICourses,   //Non-modifying function, can use in-place.
                                               { },
                                               cc_AIF_VIF,
                                               &ud )){
            throw std::runtime_error("Unable to compute per-ROI time courses");
        }
    }
    //For perfusion purposes, we always want to scale down the ROIs per-atomos (i.e., per-voxel).
    for(auto & tcs : ud.time_courses){
        const auto lROIname = tcs.first;
        const auto lVoxelCount = ud.voxel_count[lROIname];
        tcs.second = tcs.second.Multiply_With(1.0/static_cast<double>(lVoxelCount));
    }

    //Scale the contrast agent to account for the fact that contrast agent does not enter the RBCs.
    //
    // NOTE: "Because the contrast agent does not enter the RBCs, the time series Caorta(t) and Cportal(t)
    //        were divided by one minus the hematocrit." (From Van Beers et al. 2000.)
    for(auto & theROI : ud.time_courses){
        theROI.second = theROI.second.Multiply_With(1.0/(1.0 - 0.42));
    }


    //Decimate the number of pixels for modeling purposes.
    if((PreDecimateR > 0) && (PreDecimateC > 0)){
        auto DecimateRC = std::bind(InImagePlanePixelDecimate, 
                                    std::placeholders::_1, std::placeholders::_2, 
                                    std::placeholders::_3, std::placeholders::_4,
                                    PreDecimateR, PreDecimateC,
                                    std::placeholders::_5);


        //for(auto & img_arr : DICOM_data.image_data){
        for(auto & img_arr : C_enhancement_img_arrays){
            if(!img_arr->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                   DecimateRC,
                                                   {}, {} )){
                throw std::runtime_error("Unable to decimate pixels");
            }
        }
    }

    //Using the ROI time curves, compute a pharmacokinetic model and produce an image map 
    // with some model parameter(s).
    std::vector<std::shared_ptr<Image_Array>> pharmaco_model_dummy; //This gets destroyed ASAP after computation.
    std::vector<std::shared_ptr<Image_Array>> pharmaco_model_kA;
    std::vector<std::shared_ptr<Image_Array>> pharmaco_model_tauA;
    std::vector<std::shared_ptr<Image_Array>> pharmaco_model_kV;
    std::vector<std::shared_ptr<Image_Array>> pharmaco_model_tauV;
    std::vector<std::shared_ptr<Image_Array>> pharmaco_model_k2;


    //Prune images, to reduce the computational effort needed.
    if(false){
        for(auto & img_arr : C_enhancement_img_arrays){
            const auto centre = img_arr->imagecoll.center();
            img_arr->imagecoll.Retain_Images_Satisfying(
                                  [=](const planar_image<float,double> &animg)->bool{
                                      return animg.encompasses_point(centre);
                                  });
        }
    }

    //Use a Chebyshev model.
    if(UseChebyshevPolyMethod){
        decltype(ud.time_courses) orig_time_courses;
        for(const auto & tc : ud.time_courses) orig_time_courses["Original " + tc.first] = tc.second;

        //Pre-process the AIF and VIF time courses.
        KineticModel_Liver_1C2I_5Param_Chebyshev_UserData ud_cheby; 

        ud_cheby.pixels_to_plot = pixels_to_plot;
        ud_cheby.TargetROIs = TargetROINameRegex;
        ud_cheby.ContrastInjectionLeadTime = ContrastInjectionLeadTime;
        ud_cheby.ExpApproxTrunc = ExponentialKernelCoeffTruncation;
        ud_cheby.MultiplicationCoeffTrunc = FastChebyshevMultiplication;
        {
            //Correct any unaccounted-for contrast enhancement shifts. 
            if(true) for(auto & theROI : ud.time_courses){
                //Subtract the minimum over the full time course.
                if(false){
                    const auto Cmin = theROI.second.Get_Extreme_Datum_y().first;
                    theROI.second = theROI.second.Sum_With(0.0-Cmin[2]);

                //Subtract the mean from the pre-injection period.
                }else{
                    const auto preinject = theROI.second.Select_Those_Within_Inc(-1E99,ContrastInjectionLeadTime);
                    const auto themean = preinject.Mean_y()[0];
                    theROI.second = theROI.second.Sum_With(0.0-themean);
                }
            }
        
            //Insert some virtual points before the first sample (assumed to be at t=0).
            //
            // Note: If B-splines are used you need to have good coverage. If linear interpolation is used you only
            //       need two (one the the far left and one near t=0).
            const std::vector<double> ExtrapolationDts = { 5.0, 9.0, 12.5, 17.0, 21.3, 25.0 };
            if(true) for(auto & theROI : ud.time_courses){
                const auto tmin = theROI.second.Get_Extreme_Datum_x().first[0];
                for(auto &dt : ExtrapolationDts) theROI.second.push_back( tmin - dt, 0.0, 0.0, 0.0 );
            }
        
            //Perform smoothing on the AIF and VIF to help reduce optimizer bounce.
            if(false) for(auto & theROI : ud.time_courses){
                theROI.second = theROI.second.Resample_Equal_Spacing(200);
                theROI.second = theROI.second.Moving_Median_Filter_Two_Sided_Equal_Weighting(2);
            }
       
            //Extrapolate beyond the data collection limit (to stop the optimizer getting snagged
            // on any sharp drop-offs when shifting tauA and tauV).
            if(true) for(auto & theROI : ud.time_courses){
                const auto washout = theROI.second.Select_Those_Within_Inc(ContrastInjectionWashoutTime,1E99);
                const auto leastsquares =  washout.Linear_Least_Squares_Regression();
                const auto tmax = theROI.second.Get_Extreme_Datum_x().second[0];
                for(auto &dt : ExtrapolationDts){
                    const auto virtdatum_t = tmax + dt;
                    const auto virtdatum_f = leastsquares.evaluate_simple(virtdatum_t);
                    theROI.second.push_back(virtdatum_t, 0.0, virtdatum_f, 0.0 );
                }
            }

            //Perform smoothing on the AIF and VIF using NPLLR.
            if(false) for(auto & theROI : ud.time_courses){
                bool lOK;
                orig_time_courses["NPLLR: " + theROI.first] = NPRLL::Attempt_Auto_Smooth(theROI.second, &lOK);
                theROI.second = NPRLL::Attempt_Auto_Smooth(theROI.second, &lOK);
                if(!lOK) throw std::runtime_error("Unable to smooth AIF or VIF.");
            }

            //Approximate the VIF and VIF with a Chebyshev polynomial approximation.
            for(auto & theROI : ud.time_courses){
                const auto ROIN = theROI.second.size();
                size_t num_ca_coeffs;
                if(ChebyshevPolyCoefficientsRelative){
                    num_ca_coeffs = static_cast<size_t>( std::round( ROIN * ChebyshevPolyCoefficients ) );
                }else{
                    num_ca_coeffs = static_cast<size_t>( std::round( ChebyshevPolyCoefficients ) );
                }

                size_t num_bs_coeffs;
                if(BasisSplineCoefficientsRelative){
                    num_bs_coeffs = static_cast<size_t>( std::round( ROIN * BasisSplineCoefficients ) );
                }else{
                    num_bs_coeffs = static_cast<size_t>( std::round( BasisSplineCoefficients ) );
                }

                const auto tmin = theROI.second.Get_Extreme_Datum_x().first[0];
                const auto tmax = theROI.second.Get_Extreme_Datum_x().second[0];
                const auto pinf = std::numeric_limits<double>::infinity(); //use automatic (maximal) endpoint determination.

                cheby_approx<double> ca;

                //Use basis spline interpolation when constructing the Chebyshev approximation.
                if(UseBasisSplineInterpolation){
                    theROI.second = theROI.second.Strip_Uncertainties_in_y();
                    //basis_spline bs(theROI.second, pinf, pinf, BasisSplineOrder, num_bs_coeffs, basis_spline_breakpoints::equal_spacing);
                    basis_spline bs(theROI.second, pinf, pinf, BasisSplineOrder, num_bs_coeffs, basis_spline_breakpoints::adaptive_datum_density);
                    auto interp = [&bs](double t) -> double {
                            const auto interpolated_f = bs.Sample(t)[2];
                            return interpolated_f;
                    };
                    ca.Prepare(interp, num_ca_coeffs, tmin + 5.0, tmax - 5.0);

                //Use (default) linear interpolation when constructing the Chebyshev approximation.
                }else{
                    ca.Prepare(theROI.second, num_ca_coeffs, tmin + 5.0, tmax - 5.0);
                }

                ud_cheby.time_courses[theROI.first] = ca;
                ud_cheby.time_course_derivatives[theROI.first] = ca.Chebyshev_Derivative();
            }

            if(ShouldPlotAIFVIF){
                PlotTimeCourses("Processed AIF and VIF", orig_time_courses, ud_cheby.time_courses);
            }
        }

        for(auto & img_arr : C_enhancement_img_arrays){
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
            pharmaco_model_dummy.emplace_back( DICOM_data.image_data.back() );

            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_kA.emplace_back( DICOM_data.image_data.back() );
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_tauA.emplace_back( DICOM_data.image_data.back() );
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_kV.emplace_back( DICOM_data.image_data.back() );
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_tauV.emplace_back( DICOM_data.image_data.back() );
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_k2.emplace_back( DICOM_data.image_data.back() );

            if(!pharmaco_model_dummy.back()->imagecoll.Process_Images_Parallel( 
                              GroupSpatiallyOverlappingImages,
                              KineticModel_Liver_1C2I_5Param_Chebyshev_LevenbergMarquardt,
                              { std::ref(pharmaco_model_kA.back()->imagecoll),
                                std::ref(pharmaco_model_tauA.back()->imagecoll),
                                std::ref(pharmaco_model_kV.back()->imagecoll),
                                std::ref(pharmaco_model_tauV.back()->imagecoll),
                                std::ref(pharmaco_model_k2.back()->imagecoll) }, 
                              cc_all,
                              &ud_cheby )){
                throw std::runtime_error("Unable to pharmacokinetically model liver!");
            }else{
                pharmaco_model_dummy.back()->imagecoll.images.clear();
            }
        }
        pharmaco_model_dummy.clear();

    //Use a linear model.
    }else{
        decltype(ud.time_courses) toplot_time_courses;
        for(const auto & tc : ud.time_courses) toplot_time_courses["Original " + tc.first] = tc.second;

        //Pre-process the AIF and VIF time courses.
        KineticModel_Liver_1C2I_5Param_LinearInterp_UserData ud_linear; 

        ud_linear.pixels_to_plot = pixels_to_plot;
        ud_linear.TargetROIs = TargetROINameRegex;
        ud_linear.ContrastInjectionLeadTime = ContrastInjectionLeadTime;
        {
            //Correct any unaccounted-for contrast enhancement shifts. 
            if(true) for(auto & theROI : ud.time_courses){
                //Subtract the minimum over the full time course.
                if(false){
                    const auto Cmin = theROI.second.Get_Extreme_Datum_y().first;
                    theROI.second = theROI.second.Sum_With(0.0-Cmin[2]);

                //Subtract the mean from the pre-injection period.
                }else{
                    const auto preinject = theROI.second.Select_Those_Within_Inc(-1E99,ContrastInjectionLeadTime);
                    const auto themean = preinject.Mean_y()[0];
                    theROI.second = theROI.second.Sum_With(0.0-themean);
                }
            }
        
            //Insert some virtual points before the first sample (assumed to be at t=0).
            //
            // Note: If B-splines are used you need to have good coverage. If linear interpolation is used you only
            //       need two (one the the far left and one near t=0).
            const std::vector<double> ExtrapolationDts = { 5.0, 9.0, 12.5, 17.0, 21.3, 25.0 };
            if(true) for(auto & theROI : ud.time_courses){
                const auto tmin = theROI.second.Get_Extreme_Datum_x().first[0];
                for(auto &dt : ExtrapolationDts) theROI.second.push_back( tmin - dt, 0.0, 0.0, 0.0 );
            }
        
            //Perform smoothing on the AIF and VIF to help reduce optimizer bounce.
            if(false) for(auto & theROI : ud.time_courses){
                theROI.second = theROI.second.Resample_Equal_Spacing(200);
                theROI.second = theROI.second.Moving_Median_Filter_Two_Sided_Equal_Weighting(2);
            }
       
            //Extrapolate beyond the data collection limit (to stop the optimizer getting snagged
            // on any sharp drop-offs when shifting tauA and tauV).
            if(true) for(auto & theROI : ud.time_courses){
                const auto washout = theROI.second.Select_Those_Within_Inc(ContrastInjectionWashoutTime,1E99);
                const auto leastsquares =  washout.Linear_Least_Squares_Regression();
                const auto tmax = theROI.second.Get_Extreme_Datum_x().second[0];
                for(auto &dt : ExtrapolationDts){
                    const auto virtdatum_t = tmax + dt;
                    const auto virtdatum_f = leastsquares.evaluate_simple(virtdatum_t);
                    theROI.second.push_back(virtdatum_t, 0.0, virtdatum_f, 0.0 );
                }
            }

            //Perform smoothing on the AIF and VIF using NPLLR.
            if(false) for(auto & theROI : ud.time_courses){
                bool lOK;
                toplot_time_courses["NPLLR: " + theROI.first] = NPRLL::Attempt_Auto_Smooth(theROI.second, &lOK);
                theROI.second = NPRLL::Attempt_Auto_Smooth(theROI.second, &lOK);
                if(!lOK) throw std::runtime_error("Unable to smooth AIF or VIF.");
            }

            //Pack the AIF and VIF into the user_data parameter pack.
            for(auto & theROI : ud.time_courses){
                ud_linear.time_courses[theROI.first] = theROI.second;

                //Also place them into a plotting buffer.
                toplot_time_courses[theROI.first] = theROI.second;
            }

            if(ShouldPlotAIFVIF){
                PlotTimeCourses("Processed AIF and VIF", toplot_time_courses, {});
            }
        }

        for(auto & img_arr : C_enhancement_img_arrays){
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
            pharmaco_model_dummy.emplace_back( DICOM_data.image_data.back() );

            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_kA.emplace_back( DICOM_data.image_data.back() );
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_tauA.emplace_back( DICOM_data.image_data.back() );
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_kV.emplace_back( DICOM_data.image_data.back() );
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_tauV.emplace_back( DICOM_data.image_data.back() );
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
            pharmaco_model_k2.emplace_back( DICOM_data.image_data.back() );

            if(!pharmaco_model_dummy.back()->imagecoll.Process_Images_Parallel( 
                              GroupSpatiallyOverlappingImages,
                              KineticModel_Liver_1C2I_5Param_LinearInterp,
                              { std::ref(pharmaco_model_kA.back()->imagecoll),
                                std::ref(pharmaco_model_tauA.back()->imagecoll),
                                std::ref(pharmaco_model_kV.back()->imagecoll),
                                std::ref(pharmaco_model_tauV.back()->imagecoll),
                                std::ref(pharmaco_model_k2.back()->imagecoll) }, 
                              cc_all,
                              &ud_linear )){
                throw std::runtime_error("Unable to pharmacokinetically model liver!");
            }else{
                pharmaco_model_dummy.back()->imagecoll.images.clear();
            }
        }
        pharmaco_model_dummy.clear();
    }

    //Ensure the images are properly spatially ordered.
    if(true){
        for(auto & img_array : DICOM_data.image_data){
            //img_array->imagecoll.Stable_Sort_on_Metadata_Keys_Value_Numeric<int64_t>("InstanceNumber");
            img_array->imagecoll.Stable_Sort_on_Metadata_Keys_Value_Numeric<double>("SliceLocation");
            img_array->imagecoll.Stable_Sort_on_Metadata_Keys_Value_Numeric<double>("dt");
        }
    }
    
    return true;
}
