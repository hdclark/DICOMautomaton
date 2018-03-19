//CT_Liver_Perfusion.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <YgorPlot.h>
#include <algorithm>
#include <experimental/any>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "../Structs.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/CT_Perfusion_Clip_Search.h"
#include "../YgorImages_Functors/Processing/CT_Reasonable_HU_Window.h"
#include "../YgorImages_Functors/Processing/DBSCAN_Time_Courses.h"
#include "../YgorImages_Functors/Processing/Highlight_ROI_Voxels.h"
#include "../YgorImages_Functors/Processing/Logarithmic_Pixel_Scale.h"
#include "../YgorImages_Functors/Processing/Max_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/Min_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Transform/CT_Perfusion_Signal_Diff.h"
#include "../YgorImages_Functors/Transform/Subtract_Spatially_Overlapping_Images.h"
#include "CT_Liver_Perfusion.h"
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.

template <class T> class contour_collection;

std::list<OperationArgDoc> OpArgDocCT_Liver_Perfusion(void){
    return std::list<OperationArgDoc>();
}

Drover CT_Liver_Perfusion(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string,std::string> InvocationMetadata, std::string /*FilenameLex*/){

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    std::list<std::reference_wrapper<contour_collection<double>>> cc_all;
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
            FUNCERR("Unable to force window to cover reasonable HU range");
        }
    }


    //Compute a baseline with which we can use later to compute signal enhancement.
    std::vector<std::shared_ptr<Image_Array>> baseline_img_arrays;

    if(false){
        //Baseline = temporally averaged pre-contrast-injection signal.

        double ContrastInjectionLeadTime = 10.0; //Seconds. 
        if(InvocationMetadata.count("ContrastInjectionLeadTime") == 0){
            FUNCWARN("Unable to locate 'ContrastInjectionLeadTime' invocation metadata key. Assuming the default lead time "
                     << ContrastInjectionLeadTime << "s is appropriate");
        }else{
            ContrastInjectionLeadTime = std::stod( InvocationMetadata["ContrastInjectionLeadTime"] );
            if(ContrastInjectionLeadTime < 0.0) throw std::runtime_error("Non-sensical 'ContrastInjectionLeadTime' found.");
            FUNCINFO("Found 'ContrastInjectionLeadTime' invocation metadata key. Using value " << ContrastInjectionLeadTime << "s");
        }
        auto PurgeAboveNSeconds = std::bind(PurgeAboveTemporalThreshold, std::placeholders::_1, ContrastInjectionLeadTime);

        for(auto & img_arr : orig_img_arrays){
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
            baseline_img_arrays.emplace_back( DICOM_data.image_data.back() );
    
            baseline_img_arrays.back()->imagecoll.Prune_Images_Satisfying(PurgeAboveNSeconds);
    
            if(!baseline_img_arrays.back()->imagecoll.Condense_Average_Images(GroupSpatiallyOverlappingImages)){
                FUNCERR("Cannot temporally average data set. Is it able to be averaged?");
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
                FUNCERR("Unable to generate min(pixel) images over the time course");
            }
        }
    }


    //Deep-copy the original long image array and use the temporally-averaged, pre-contrast map to work out the approximate contrast in each voxel.
    std::vector<std::shared_ptr<Image_Array>> C_enhancement_img_arrays;
    {
        auto img_arr = orig_img_arrays.front();
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        C_enhancement_img_arrays.emplace_back( DICOM_data.image_data.back() );

        if(!C_enhancement_img_arrays.back()->imagecoll.Transform_Images( CTPerfusionSigDiffC,
                                                                         { baseline_img_arrays.front()->imagecoll },
                                                                         { } )){
            FUNCERR("Unable to transform image array to make poor-man's C map");
        }
    }


    //Temporally average the whole series, to convert motion to blur.
    std::vector<std::shared_ptr<Image_Array>> temporal_avg_img_arrays;
    if(false) for(auto & img_arr : orig_img_arrays){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        temporal_avg_img_arrays.emplace_back( DICOM_data.image_data.back() );

        if(!temporal_avg_img_arrays.back()->imagecoll.Condense_Average_Images(GroupSpatiallyOverlappingImages)){
            FUNCERR("Cannot temporally average large-pixel-censored data set. Is it able to be averaged?");
        }
    }

    // TODO: Temporally average in chunks of 3-5 overlappping images. This will give a cleaner time course
    //       but will not be totally independent of time.


    //Temporally average the C(t) map, to help assess whether it seems to conform to structures.
    std::vector<std::shared_ptr<Image_Array>> temp_avg_C_img_arrays;
    if(false) for(auto & img_arr : C_enhancement_img_arrays){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        temp_avg_C_img_arrays.emplace_back( DICOM_data.image_data.back() );

        if(!temp_avg_C_img_arrays.back()->imagecoll.Condense_Average_Images(GroupSpatiallyOverlappingImages)){
            FUNCERR("Cannot temporally average C map data set. Is it able to be averaged?");
        }
    }


    //Perform cluster analysis on the time contrast agent time courses.
    std::vector<std::shared_ptr<Image_Array>> clustered_img_arrays;
    if(false) for(auto & img_arr : C_enhancement_img_arrays){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        clustered_img_arrays.emplace_back( DICOM_data.image_data.back() );

        DBSCANTimeCoursesUserData UD;
        UD.MinPts = 10.0; //Used?
        UD.Eps = -1.0; //Used?
        if(!clustered_img_arrays.back()->imagecoll.Process_Images( GroupSpatiallyOverlappingImages,
                                                                   DBSCANTimeCourses,
                                                                   {}, cc_all,
                                                                   UD )){
            FUNCERR("Unable to perform DBSCAN clustering");
        }
    }


    //Deep-copy images at a single temporal point and highlight the ROIs. 
    if(false) if(!cc_all.empty()){
        std::vector<std::shared_ptr<Image_Array>> roi_highlighted_img_arrays;
        for(auto & img_arr : temporal_avg_img_arrays){
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
            roi_highlighted_img_arrays.emplace_back( DICOM_data.image_data.back() );
 
            HighlightROIVoxelsUserData ud;
            if(!roi_highlighted_img_arrays.back()->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                                             HighlightROIVoxels,
                                                                             {}, cc_all,
                                                                             &ud )){
                FUNCERR("Unable to highlight ROIs");
            }
        }
    }


    //Copy the contrast agent images and generate contrast time courses for each ROI.
    if(false) if(!cc_all.empty()){

        std::vector<std::shared_ptr<Image_Array>> temp_img_arrays;
        //for(auto & img_arr : orig_img_arrays){
        for(auto & img_arr : C_enhancement_img_arrays){
            temp_img_arrays.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        }

        PerROITimeCoursesUserData ud;
        for(auto & img_arr : temp_img_arrays){
            if(!img_arr->imagecoll.Process_Images( GroupSpatiallyOverlappingImages,
                                                   PerROITimeCourses,
                                                   {}, cc_all,
                                                   &ud )){
                FUNCERR("Unable to generate per-ROI time courses");
            }
        }
 
        //Plot the time courses.
        Plotter2 toplot;
        for(const auto &tc_pair : ud.time_courses){
            toplot.Set_Global_Title("Contrast agent time courses");
            toplot.Insert_samples_1D(tc_pair.second, tc_pair.first, "points");
            toplot.Insert_samples_1D(tc_pair.second, "", "linespoints");
        }
        toplot.Plot();
        toplot.Plot_as_PDF(Get_Unique_Sequential_Filename("/tmp/time_course_",4,".pdf"));
        WriteStringToFile(toplot.Dump_as_String(),
                          Get_Unique_Sequential_Filename("/tmp/time_course_gnuplot_",4,".dat"));
    }


    //Deep-copy and compute the max pixel intensity over the time course. This will help find the liver marker clips.
    std::vector<std::shared_ptr<Image_Array>> max_pixel_img_arrays;
    if(true) for(auto & img_arr : orig_img_arrays){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        max_pixel_img_arrays.emplace_back( DICOM_data.image_data.back() );

        if(!max_pixel_img_arrays.back()->imagecoll.Process_Images_Parallel( GroupSpatiallyOverlappingImages,
                                                                   CondenseMaxPixel,
                                                                   {}, {} )){
            FUNCERR("Unable to generate max(pixel) images over the time course");
        }
    }

    //Scale the pixel intensities on a logarithmic scale. (For viewing purposes only!)
    std::vector<std::shared_ptr<Image_Array>> log_scaled_img_arrays;
    if(true) for(auto & img_arr : max_pixel_img_arrays){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        log_scaled_img_arrays.emplace_back( DICOM_data.image_data.back() );

        if(!log_scaled_img_arrays.back()->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                                    LogScalePixels,
                                                                    {}, {} )){
            FUNCERR("Unable to perform logarithmic pixel scaling");
        }
    }

    ////Temporally average the images with logarithmically scaled pixels.
    //std::vector<std::shared_ptr<Image_Array>> temporalavgd_logscaled_img_arrays;
    //if(false) for(auto & img_arr : log_scaled_img_arrays){
    //    DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
    //    temporalavgd_logscaled_img_arrays.emplace_back( DICOM_data.image_data.back() );
    //
    //    if(!temporalavgd_logscaled_img_arrays.back()->imagecoll.Condense_Average_Images(GroupSpatiallyOverlappingImages)){
    //        FUNCERR("Cannot temporally average data set. Is it able to be averaged?");
    //    }
    //}


    // IDEA: 1. Compute the MIN pixel value over the time course.
    //       2. Grow the bright areas of the MIN by N pixels in all directions. If the neighbouring
    //          pixel is lower, grow the pixel value out (but use the original MIN image as the growth
    //          map so the brightest pixel doesn't spread everywhere.)
    //       3. Take the full, original image series and subtract off the GROWN MIN.
    // This ought to help get rid of ribs, couch, anything consistently bright in every image.
    // Since the liver clips and liver move around quite a bit, they should be 'hidden' in the MIN.
    // Subtracting off the bright areas should really help ensure static structures do not remain.


    //Deep-copy and compute the min pixel intensity over the time course. This will help find the liver marker clips.
    std::vector<std::shared_ptr<Image_Array>> min_pixel_img_arrays;
    if(false) for(auto & img_arr : orig_img_arrays){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        min_pixel_img_arrays.emplace_back( DICOM_data.image_data.back() );

        if(!min_pixel_img_arrays.back()->imagecoll.Process_Images_Parallel( GroupSpatiallyOverlappingImages,
                                                                   CondenseMinPixel,
                                                                   {}, {} )){
            FUNCERR("Unable to generate min(pixel) images over the time course");
        }
    }


    //Deep-copy and subtract the min pixel intensity over the time course from each image. This will help find the liver marker clips.
    std::vector<std::shared_ptr<Image_Array>> sub_min_pixel_img_arrays;
    if(false) for(auto & img_arr : orig_img_arrays){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        sub_min_pixel_img_arrays.emplace_back( DICOM_data.image_data.back() );

        std::list<std::reference_wrapper<planar_image_collection<float,double>>> external_imgs;
        for(auto & img_arr2 : min_pixel_img_arrays){
            external_imgs.push_back( std::ref(img_arr2->imagecoll) );
        }
        if(!sub_min_pixel_img_arrays.back()->imagecoll.Transform_Images( SubtractSpatiallyOverlappingImages, std::move(external_imgs), {} )){
            FUNCERR("Unable to subtract the min(pixel) map from the time course");
        }
    }


    //TODO: Grow bright areas to N nearby pixels.


    //TODO: Alter this routine to use the GROWN MIN instead of the MIN directly.


    //Generate a map which will help in the identification of liver marker clips.
    std::vector<std::shared_ptr<Image_Array>> clip_likelihood_map_img_arrays;
    if(false) for(auto & img_arr : orig_img_arrays){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        clip_likelihood_map_img_arrays.emplace_back( DICOM_data.image_data.back() );

        if(!clip_likelihood_map_img_arrays.back()->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                                             CTPerfusionSearchForLiverClips,
                                                                             {}, {} )){
            FUNCERR("Unable to perform search for liver clip markers");
        }
    }

    //Deep-copy and temporally-average the clip likelihood maps. This is hopefully average out clip motions
    // over the time course (clip motion should be somewhat).   
    std::vector<std::shared_ptr<Image_Array>> tavgd_clip_likelihood_map_img_arrays;
    if(false) for(auto & img_arr : clip_likelihood_map_img_arrays){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        tavgd_clip_likelihood_map_img_arrays.emplace_back( DICOM_data.image_data.back() );

        if(!tavgd_clip_likelihood_map_img_arrays.back()->imagecoll.Condense_Average_Images(
                                                                 GroupSpatiallyOverlappingImages)){
            FUNCERR("Unable to time-average clip likelihood maps");
        }
    }
 


    return DICOM_data;
}
