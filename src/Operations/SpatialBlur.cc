//SpatialBlur.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

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
#include "../YgorImages_Functors/Processing/In_Image_Plane_Blur.h"
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

#include "SpatialBlur.h"


std::list<OperationArgDoc> OpArgDocSpatialBlur(void){
    std::list<OperationArgDoc> out;

    // This operation blurs pixels (within the plane of the image only) using the specified estimator.

    out.emplace_back();
    out.back().name = "ImageSelection";
    out.back().desc = "Images to operate on. Either 'none', 'last', 'first', or 'all'.";
    out.back().default_val = "all";
    out.back().expected = true;
    out.back().examples = { "none", "last", "first", "all" };
    
    out.emplace_back();
    out.back().name = "Estimator";
    out.back().desc = "Controls the (in-plane) blur estimator to use."
                      " Options are currently: box_3x3, box_5x5, gaussian_3x3, gaussian_5x5, and gaussian_open."
                      " The latter (gaussian_open) is adaptive and requires a supplementary parameter that controls"
                      " the number of adjacent pixels to consider. The former ('...3x3' and '...5x5') are 'fixed'"
                      " estimators that use a convolution kernel with a fixed size (3x3 or 5x5 pixel neighbourhoods)."
                      " All estimators operate in 'pixel-space' and are ignorant about the image spatial extent."
                      " All estimators are normalized, and thus won't significantly affect the pixel magnitude scale.";
    out.back().default_val = "gaussian_open";
    out.back().expected = true;
    out.back().examples = { "box_3x3",
                            "box_5x5",
                            "gaussian_3x3",
                            "gaussian_5x5",
                            "gaussian_open" };

    out.emplace_back();
    out.back().name = "GaussianOpenSigma";
    out.back().desc = "Controls the number of neighbours to consider (only) when using the gaussian_open estimator."
                      " The number of pixels is computed automatically to accommodate the specified sigma"
                      " (currently ignored pixels have 3*sigma or less weighting). Be aware this operation can take"
                      " an enormous amount of time, since the pixel neighbourhoods quickly grow large.";
    out.back().default_val = "1.5";
    out.back().expected = true;
    out.back().examples = { "0.5",
                            "1.0",
                            "1.5",
                            "2.5",
                            "5.0" };

    return out;
}

Drover SpatialBlur(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> InvocationMetadata, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto EstimatorStr = OptArgs.getValueStr("Estimator").value();
    const auto GaussianOpenSigma = std::stod( OptArgs.getValueStr("GaussianOpenSigma").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_none  = std::regex("^no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_first = std::regex("^fi?r?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last  = std::regex("^la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all   = std::regex("^al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto regex_box3x3 = std::regex("^bo?x?_?3x?3?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_box5x5 = std::regex("^bo?x?_?5x?5?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_gau3x3 = std::regex("^ga?u?s?s?i?a?n?_?3x?3?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_gau5x5 = std::regex("^ga?u?s?s?i?a?n?_?5x?5?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_gauopn = std::regex("^ga?u?s?s?i?a?n?_?op?e?n?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    if( !std::regex_match(ImageSelectionStr, regex_none)
    &&  !std::regex_match(ImageSelectionStr, regex_first)
    &&  !std::regex_match(ImageSelectionStr, regex_last)
    &&  !std::regex_match(ImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Image selection is not valid. Cannot continue.");
    }

    // --- Cycle over all images, performing the blur ---

    //Image data.
    auto iap_it = DICOM_data.image_data.begin();
    if(false){
    }else if(std::regex_match(ImageSelectionStr, regex_none)){ iap_it = DICOM_data.image_data.end();
    }else if(std::regex_match(ImageSelectionStr, regex_last)){
        if(!DICOM_data.image_data.empty()) iap_it = std::prev(DICOM_data.image_data.end());
    }
    while(iap_it != DICOM_data.image_data.end()){
        InPlaneImageBlurUserData ud;
        ud.gaussian_sigma = GaussianOpenSigma;

        if(false){
        }else if( std::regex_match(EstimatorStr, regex_box3x3) ){
            ud.estimator = BlurEstimator::box_3x3;
        }else if( std::regex_match(EstimatorStr, regex_box5x5) ){
            ud.estimator = BlurEstimator::box_5x5;
        }else if( std::regex_match(EstimatorStr, regex_gau3x3) ){
            ud.estimator = BlurEstimator::gaussian_3x3;
        }else if( std::regex_match(EstimatorStr, regex_gau5x5) ){
            ud.estimator = BlurEstimator::gaussian_5x5;
        }else if( std::regex_match(EstimatorStr, regex_gauopn) ){
            ud.estimator = BlurEstimator::gaussian_open;
        }else{
            throw std::invalid_argument("Estimator argument '"_s + EstimatorStr + "' is not valid");
        }

        if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                          InPlaneImageBlur,
                                                          {}, {}, &ud )){
            throw std::runtime_error("Unable to compute specified blur.");
        }
        ++iap_it;
        if(std::regex_match(ImageSelectionStr, regex_first)) break;
    }

    return std::move(DICOM_data);
}
