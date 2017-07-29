//UBC3TMRI_DCE_Experimental.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

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

#include "UBC3TMRI_DCE_Experimental.h"


std::list<OperationArgDoc> OpArgDocUBC3TMRI_DCE_Experimental(void){
    return std::list<OperationArgDoc>();
}

Drover UBC3TMRI_DCE_Experimental(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string,std::string> InvocationMetadata, std::string /*FilenameLex*/){

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    std::list<std::reference_wrapper<contour_collection<double>>> cc_all;
    for(auto & cc : DICOM_data.contour_data->ccs){
        auto base_ptr = reinterpret_cast<contour_collection<double> *>(&cc);
        cc_all.push_back( std::ref(*base_ptr) );
    }


    //Get named handles for each image array so we can easily refer to them later.
    std::shared_ptr<Image_Array> dummy;
    std::shared_ptr<Image_Array> img_arr_orig_long_scan = *std::next(DICOM_data.image_data.begin(),0);  //SeriesNumber 901.
    std::vector<std::shared_ptr<Image_Array>> short_scans;
    for(auto it = std::next(DICOM_data.image_data.begin(),1);
             it != DICOM_data.image_data.end(); ++it) short_scans.push_back( *it );
//        std::shared_ptr<Image_Array> img_arr_orig_short_scan = *std::next(DICOM_data.image_data.begin(),1); //SeriesNumber 801.


    //Temporally average the long array for later S0 and T1 map creation.
    DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_orig_long_scan ) );
    std::shared_ptr<Image_Array> img_arr_copy_long_temporally_avgd( DICOM_data.image_data.back() );

    double ContrastInjectionLeadTime = 35.0; //Seconds. 
    if(InvocationMetadata.count("ContrastInjectionLeadTime") == 0){
        FUNCWARN("Unable to locate 'ContrastInjectionLeadTime' invocation metadata key. Assuming the default lead time "
                 << ContrastInjectionLeadTime << "s is appropriate");
    }else{
        ContrastInjectionLeadTime = std::stod( InvocationMetadata["ContrastInjectionLeadTime"] );
        if(ContrastInjectionLeadTime < 0.0) throw std::runtime_error("Non-sensical 'ContrastInjectionLeadTime' found.");
        FUNCINFO("Found 'ContrastInjectionLeadTime' invocation metadata key. Using value " << ContrastInjectionLeadTime << "s");
    }
    auto PurgeAboveNSeconds = std::bind(PurgeAboveTemporalThreshold, std::placeholders::_1, ContrastInjectionLeadTime);

    img_arr_copy_long_temporally_avgd->imagecoll.Prune_Images_Satisfying(PurgeAboveNSeconds);
    if(!img_arr_copy_long_temporally_avgd->imagecoll.Condense_Average_Images(GroupSpatiallyOverlappingImages)) FUNCERR("Cannot temporally avg long img_arr");

 
    //Temporally average the short arrays for later S0 and T1 map creation.
    std::vector<std::shared_ptr<Image_Array>> short_tavgd;
    for(auto img_ptr : short_scans){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_ptr ) );
        short_tavgd.push_back( DICOM_data.image_data.back() );

        if(!short_tavgd.back()->imagecoll.Condense_Average_Images(GroupSpatiallyOverlappingImages)) FUNCERR("Cannot temporally avg short img_arr");
    }


    //Gaussian blur in pixel space.
    std::shared_ptr<Image_Array> img_arr_long_tavgd_blurred(img_arr_copy_long_temporally_avgd);
    if(false){ //Blur the image.
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_long_tavgd_blurred ) );
        img_arr_long_tavgd_blurred = DICOM_data.image_data.back();

        if(!img_arr_long_tavgd_blurred->imagecoll.Gaussian_Pixel_Blur({ }, 1.5)){
            FUNCERR("Unable to blur long temporally averaged images");
        }
    }

    std::vector<std::shared_ptr<Image_Array>> short_tavgd_blurred;
    if(false){
        for(auto img_ptr : short_tavgd){
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_ptr ) );
            short_tavgd_blurred.push_back( DICOM_data.image_data.back() );

            if(!short_tavgd_blurred.back()->imagecoll.Gaussian_Pixel_Blur({ }, 1.5)){
                FUNCERR("Unable to blur short temporally averaged images");
            }
        }
    }else{
        for(auto img_ptr : short_tavgd) short_tavgd_blurred.push_back( img_ptr );
    }

    //Package the short and long images together as needed for the S0 and T1 calculations.
    std::list<std::reference_wrapper<planar_image_collection<float,double>>> tavgd_blurred;
    tavgd_blurred.push_back(img_arr_long_tavgd_blurred->imagecoll);
    for(auto img_ptr : short_tavgd_blurred) tavgd_blurred.push_back(img_ptr->imagecoll);

    //Deep-copy and process the (possibly blurred) collated image array, generating a T1 map in-situ.
    DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_long_tavgd_blurred ) );
    std::shared_ptr<Image_Array> img_arr_T1_map( DICOM_data.image_data.back() );

    if(!img_arr_T1_map->imagecoll.Transform_Images( DCEMRIT1MapV2, tavgd_blurred, { } )){
        FUNCERR("Unable to transform image array to make T1 map");
    }

    //Produce an S0 map.
    DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_long_tavgd_blurred ) );
    std::shared_ptr<Image_Array> img_arr_S0_map( DICOM_data.image_data.back() );

    if(!img_arr_S0_map->imagecoll.Transform_Images( DCEMRIS0MapV2, tavgd_blurred, { } )){
        FUNCERR("Unable to transform image array to make S0 map");
    }

    //Blur the S0 and T1 maps if needed.
    std::shared_ptr<Image_Array> img_arr_T1_map_blurred( img_arr_T1_map );
    if(false){ //Produce a blurred image.
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_T1_map ) );
        img_arr_T1_map_blurred = DICOM_data.image_data.back();
    
        if(!img_arr_T1_map_blurred->imagecoll.Gaussian_Pixel_Blur({ }, 1.5)){
            FUNCERR("Unable to blur T1 map");
        }
    }

    std::shared_ptr<Image_Array> img_arr_S0_map_blurred( img_arr_S0_map );
    if(false){ //Produce a blurred image.
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_S0_map ) );
        img_arr_S0_map_blurred = DICOM_data.image_data.back();
        
        if(!img_arr_S0_map_blurred->imagecoll.Gaussian_Pixel_Blur({ }, 1.5)){
            FUNCERR("Unable to blur S0 map");
        }
    }


    //Compute the contrast agent enhancement C(t) curves using S0 and T1 maps.
    std::shared_ptr<Image_Array> img_arr_C_map; //Can be either "proper" or "signal difference" C(t) maps.
    if(true){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_orig_long_scan ) );
        img_arr_C_map = DICOM_data.image_data.back();

        if(!img_arr_C_map->imagecoll.Transform_Images( DCEMRICMap,
                                                       { img_arr_S0_map_blurred->imagecoll, img_arr_T1_map_blurred->imagecoll }, 
                                                       { } )){
            FUNCERR("Unable to transform image array to make C map");
        }

    //Or compute it using the signal difference method (without S0 or T1 maps).
    }else{
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_orig_long_scan ) );
        img_arr_C_map = DICOM_data.image_data.back();

        if(!img_arr_C_map->imagecoll.Transform_Images( DCEMRISigDiffC,
                                                       { img_arr_copy_long_temporally_avgd->imagecoll }, 
                                                       { } )){
            FUNCERR("Unable to transform image array to make poor-man's C map");
        }
    }

    //Compute an IAUC map from the C(t) map.
    if(false){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_C_map ) ); 
        std::shared_ptr<Image_Array> img_arr_iauc_map( DICOM_data.image_data.back() );
    
        if(!img_arr_iauc_map->imagecoll.Process_Images( GroupSpatiallyOverlappingImages, DCEMRIAUCMap, {}, {} )){
            FUNCERR("Unable to process image array to make IAUC map");
        }
    }


    //Perform a "kitchen sink" analysis on the C(t) map.
    if(false){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_C_map ) ); 
        std::shared_ptr<Image_Array> img_arr_kitchen_sink_map( DICOM_data.image_data.back() );

        if(!img_arr_kitchen_sink_map->imagecoll.Process_Images( GroupSpatiallyOverlappingImages, 
                                                                KitchenSinkAnalysis, {},
                                                                { cc_all } )){
                                                                //{ cc_r_parotid_int, cc_l_parotid_int } )){
                                                                //{ cc_r_parotid_int, cc_l_parotid_int, cc_r_masseter_int, cc_pharynx_int } )){
            FUNCERR("Unable to process image array to perform kitchen sink analysis");
        }else{
            DumpKitchenSinkResults(InvocationMetadata);
        }
    }
 
    //Compute a histogram over pixel value intensities for each ROI using the original long time series.
    if(false){
        if(!img_arr_orig_long_scan->imagecoll.Transform_Images( PixelHistogramAnalysis,
                                                                { },
                                                                { cc_all } )){
                                                                //{ cc_r_parotid_int, cc_l_parotid_int } )){
                                                                //{ cc_r_parotid_int, cc_l_parotid_int, cc_r_masseter_int, cc_pharynx_int } )){
            FUNCERR("Unable to compute pixel value intensity histograms");
        }else{
            DumpPixelHistogramResults();
        }
    }

    //Deep-copy images at a single temporal point and highlight the ROIs. 
    if(false){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_copy_long_temporally_avgd ) );
        std::shared_ptr<Image_Array> img_arr_highlighted_rois( DICOM_data.image_data.back() );

        HighlightROIVoxelsUserData ud;
        if(!img_arr_highlighted_rois->imagecoll.Process_Images( GroupIndividualImages, 
                                                                HighlightROIVoxels, {},
                                                                { cc_all },
                                                                &ud )){
                                                                //{ cc_r_parotid_int, cc_l_parotid_int, cc_r_masseter_int, cc_pharynx_int } )){
            FUNCERR("Unable to highlight ROIs");
        }
    }

    return std::move(DICOM_data);
}
