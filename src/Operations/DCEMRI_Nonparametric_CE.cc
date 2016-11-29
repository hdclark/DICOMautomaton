//DCEMRI_Nonparametric_CE.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

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

#include "DCEMRI_Nonparametric_CE.h"


std::list<OperationArgDoc> OpArgDocDCEMRI_Nonparametric_CE(void){
    return std::list<OperationArgDoc>();
}

Drover DCEMRI_Nonparametric_CE(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string,std::string> InvocationMetadata, std::string /*FilenameLex*/){

    //This operation takes a single DCE-MRI scan ('measurement') and generates a "poor-mans's" contrast enhancement
    // signal. This is accomplished by subtracting the pre-contrast injection images average ('baseline') from later
    // images (and then possibly/optionally averaging relative to the baseline).
    //
    // Only a single image volume is required. It is expected to have temporal sampling beyond the contrast injection
    // timepoint (or some default value -- currently around ~30s). The resulting images retain the baseline portion, so
    // you'll need to trim yourself if needed.
    //
    // Be aware that this method of deriving contrast enhancement is not valid! It ignores nuances due to differing T1
    // or T2 values due to the presence of contrast agent. It should only be used for exploratory purposes or cases
    // where the distinction with reality is irrelevant.
    //

    //Verify there is data to work on.
    if( DICOM_data.image_data.empty() ){
        throw std::invalid_argument("No data to work on. Unable to estimate contrast enhancement.");
    }

    //Get handles for each of the original image arrays so we can easily refer to them later.
    std::vector<std::shared_ptr<Image_Array>> orig_img_arrays;
    for(auto & img_arr : DICOM_data.image_data) orig_img_arrays.push_back(img_arr);

    //Complain if there are several images, but continue on using only the first volume.
    if(orig_img_arrays.size() != 1){
        FUNCWARN("Several image volumes detected."
                 " Proceeding to generate non-parametric DCE contrast enhancement with the first only.");
    }
    orig_img_arrays.resize(1); // NOTE: Later assumptions are made about image ordering!

    //Figure out how much time elapsed before contrast injection began.
    double ContrastInjectionLeadTime = 35.0; //Seconds. 
    if(InvocationMetadata.count("ContrastInjectionLeadTime") == 0){
        FUNCWARN("Unable to locate 'ContrastInjectionLeadTime' invocation metadata key. Assuming the default lead time " 
                 << ContrastInjectionLeadTime << "s is appropriate");
    }else{
        ContrastInjectionLeadTime = std::stod( InvocationMetadata["ContrastInjectionLeadTime"] );
        if(ContrastInjectionLeadTime < 0.0) throw std::runtime_error("Non-sensical 'ContrastInjectionLeadTime' found.");
        FUNCINFO("Found 'ContrastInjectionLeadTime' invocation metadata key. Using value " << ContrastInjectionLeadTime << "s"); 
    }

    //Spatially blur the images. This may help if the measurements are noisy.
    //
    // NOTE: Blurring the baseline but not the rest of the data can result in odd results. It's best to uniformly blur
    //       all images before trying to derive contrast enhancement (i.e., low-pass filtering).
    if(false){
        for(auto img_ptr : orig_img_arrays){
            if(!img_ptr->imagecoll.Gaussian_Pixel_Blur({ }, 1.5)){
                FUNCERR("Unable to blur temporally averaged images");
            }
        }
    }
    
    //Compute a temporally-averaged baseline.
    auto PurgeAboveNSeconds = std::bind(PurgeAboveTemporalThreshold, std::placeholders::_1, ContrastInjectionLeadTime);
    std::vector<std::shared_ptr<Image_Array>> baseline_img_arrays;
    for(auto & img_arr : orig_img_arrays){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        baseline_img_arrays.emplace_back( DICOM_data.image_data.back() );

        baseline_img_arrays.back()->imagecoll.Prune_Images_Satisfying(PurgeAboveNSeconds);

        if(!baseline_img_arrays.back()->imagecoll.Condense_Average_Images(GroupSpatiallyOverlappingImages)){
            throw std::runtime_error("Cannot temporally average data set. Is it able to be averaged?");
        }
    }

    //Subtract the baseline from the measurement images.
    bool Normalize_to_Baseline = true; //Whether to normalize
    auto SigDiffC = (Normalize_to_Baseline) ? DCEMRISigDiffC : CTPerfusionSigDiffC;
    {
        auto baseline_img_arr_it = baseline_img_arrays.begin();
        for(auto & img_arr : orig_img_arrays){
            auto baseline_img_ptr = *(baseline_img_arr_it++);
            if(!img_arr->imagecoll.Transform_Images( SigDiffC,
                                                     { baseline_img_ptr->imagecoll },
                                                     { } )){
                throw std::runtime_error("Unable to subtract baseline from measurement images.");
            }
            /*
            if(Normalize_to_Baseline)
                if(!img_arr->imagecoll.Transform_Images( DCEMRISigDiffC,
                                                         { baseline_img_ptr->imagecoll },
                                                         { } )){
                    throw std::runtime_error("Unable to subtract baseline from measurement images.");
                }
            }else{
                if(!img_arr->imagecoll.Transform_Images( CTPerfusionSigDiffC,
                                                         { baseline_img_ptr->imagecoll },
                                                         { } )){
                    throw std::runtime_error("Unable to subtract baseline from measurement images.");
                }
            }
            */
        }
    }

    //Erase the baseline images.
    for(auto & img_arr : baseline_img_arrays){
        //"Erase-remove" idiom for vectors.
        auto ddid_beg = DICOM_data.image_data.begin();
        auto ddid_end = DICOM_data.image_data.end();
        DICOM_data.image_data.erase(std::remove(ddid_beg, ddid_end, img_arr), ddid_end);
    }

    return std::move(DICOM_data);
}
