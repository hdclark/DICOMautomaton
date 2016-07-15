//UBC3TMRI_IVIM_ADC.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

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
#include "../YgorImages_Functors/Processing/Liver_Pharmacokinetic_Model_5Param_Structs.h"
#include "../YgorImages_Functors/Processing/Liver_Pharmacokinetic_Model_5Param_Linear.h"
#include "../YgorImages_Functors/Processing/Liver_Pharmacokinetic_Model_5Param_Cheby.h"
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

#include "UBC3TMRI_IVIM_ADC.h"


std::list<OperationArgDoc> OpArgDocUBC3TMRI_IVIM_ADC(void){
    return std::list<OperationArgDoc>();
}

Drover UBC3TMRI_IVIM_ADC(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //Get handles for each of the original image arrays so we can easily refer to them later.
    std::vector<std::shared_ptr<Image_Array>> orig_img_arrays;
    for(auto & img_arr : DICOM_data.image_data) orig_img_arrays.push_back(img_arr);

    //std::shared_ptr<Image_Array> img_arr_orig_series_501 = *std::next(DICOM_data.image_data.begin(),0);
    //std::shared_ptr<Image_Array> img_arr_orig_series_601 = *std::next(DICOM_data.image_data.begin(),1);
   
    //Deep-copy and compute an ADC map using the various images with varying diffusion b-values.
    // This will leave us with a time series of ADC parameters (the 1DYN series will have a single time point,
    // but the 5DYN series will have five time points).
    std::vector<std::shared_ptr<Image_Array>> adc_map_img_arrays;
    for(auto & img_arr : orig_img_arrays){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        adc_map_img_arrays.emplace_back( DICOM_data.image_data.back() );
        //std::shared_ptr<Image_Array> img_arr_series_501_ADC_map( DICOM_data.image_data.back() );

        if(!adc_map_img_arrays.back()->imagecoll.Process_Images( GroupSpatiallyTemporallyOverlappingImages, 
                                                                 IVIMMRIADCMap, 
                                                                 {}, {} )){
            FUNCERR("Unable to generate ADC map");
        }
    }



    //Deep-copy the ADC map and compute a slope-sign map.
    std::vector<std::shared_ptr<Image_Array>> slope_sign_map_img_arrays;
    auto TimeCourseSlopeMapAllTime = std::bind(TimeCourseSlopeMap, 
                                               std::placeholders::_1, std::placeholders::_2, 
                                               std::placeholders::_3, std::placeholders::_4,
                                               std::numeric_limits<double>::min(), std::numeric_limits<double>::max(),
                                               std::experimental::any());
    for(auto & img_arr : adc_map_img_arrays){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        slope_sign_map_img_arrays.emplace_back( DICOM_data.image_data.back() );
        //std::shared_ptr<Image_Array> img_arr_time_course_slope_map( DICOM_data.image_data.back() );

        if(!slope_sign_map_img_arrays.back()->imagecoll.Process_Images( GroupSpatiallyOverlappingImages,
                                                                        TimeCourseSlopeMapAllTime,
                                                                        {}, {} )){
            FUNCERR("Unable to compute time course slope map");
        }
    }


    return std::move(DICOM_data);
}
