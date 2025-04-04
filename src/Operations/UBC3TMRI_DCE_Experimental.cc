//UBC3TMRI_DCE_Experimental.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <any>
#include <fstream>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    
#include <vector>
#include <cstdint>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/DCEMRI_AUC_Map.h"
#include "../YgorImages_Functors/Processing/Partitioned_Image_Voxel_Visitor_Mutator.h"
#include "../YgorImages_Functors/Transform/DCEMRI_C_Map.h"
#include "../YgorImages_Functors/Transform/DCEMRI_S0_Map_v2.h"
#include "../YgorImages_Functors/Transform/DCEMRI_Signal_Difference_C.h"
#include "../YgorImages_Functors/Transform/DCEMRI_T1_Map_v2.h"
#include "../YgorImages_Functors/Transform/Pixel_Value_Histogram.h"
#include "UBC3TMRI_DCE_Experimental.h"
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"

template <class T> class contour_collection;


OperationDoc OpArgDocUBC3TMRI_DCE_Experimental(){
    OperationDoc out;
    out.name = "UBC3TMRI_DCE_Experimental";

    out.tags.emplace_back("category: image processing");
    out.tags.emplace_back("category: modeling");
    out.tags.emplace_back("category: perfusion");
    out.tags.emplace_back("category: needs refresh");

    out.desc = 
        "This operation is an experimental operation for processing dynamic contrast-enhanced MR images.";

    return out;
}

bool UBC3TMRI_DCE_Experimental(Drover &DICOM_data,
                                 const OperationArgPkg& /*OptArgs*/,
                                 std::map<std::string, std::string>& InvocationMetadata,
                                 const std::string& /*FilenameLex*/){

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    std::list<std::reference_wrapper<contour_collection<double>>> cc_all;
    DICOM_data.Ensure_Contour_Data_Allocated();
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

    std::map<std::string,std::string> l_InvocationMetadata(InvocationMetadata);

    //Temporally average the long array for later S0 and T1 map creation.
    DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_orig_long_scan ) );
    std::shared_ptr<Image_Array> img_arr_copy_long_temporally_avgd( DICOM_data.image_data.back() );

    double ContrastInjectionLeadTime = 35.0; //Seconds. 
    if(l_InvocationMetadata.count("ContrastInjectionLeadTime") == 0){
        YLOGWARN("Unable to locate 'ContrastInjectionLeadTime' invocation metadata key. Assuming the default lead time "
                 << ContrastInjectionLeadTime << "s is appropriate");
    }else{
        ContrastInjectionLeadTime = std::stod( l_InvocationMetadata["ContrastInjectionLeadTime"] );
        if(ContrastInjectionLeadTime < 0.0) throw std::runtime_error("Non-sensical 'ContrastInjectionLeadTime' found.");
        YLOGINFO("Found 'ContrastInjectionLeadTime' invocation metadata key. Using value " << ContrastInjectionLeadTime << "s");
    }
    auto PurgeAboveNSeconds = std::bind(PurgeAboveTemporalThreshold, std::placeholders::_1, ContrastInjectionLeadTime);

    img_arr_copy_long_temporally_avgd->imagecoll.Prune_Images_Satisfying(PurgeAboveNSeconds);
    if(!img_arr_copy_long_temporally_avgd->imagecoll.Condense_Average_Images(GroupSpatiallyOverlappingImages)){
        throw std::runtime_error("Cannot temporally avg long img_arr");
    }
 
    //Temporally average the short arrays for later S0 and T1 map creation.
    std::vector<std::shared_ptr<Image_Array>> short_tavgd;
    for(const auto& img_ptr : short_scans){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_ptr ) );
        short_tavgd.push_back( DICOM_data.image_data.back() );

        if(!short_tavgd.back()->imagecoll.Condense_Average_Images(GroupSpatiallyOverlappingImages)){
            throw std::runtime_error("Cannot temporally avg short img_arr");
        }
    }


    //Gaussian blur in pixel space.
    std::shared_ptr<Image_Array> img_arr_long_tavgd_blurred(img_arr_copy_long_temporally_avgd);
    if(false){ //Blur the image.
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_long_tavgd_blurred ) );
        img_arr_long_tavgd_blurred = DICOM_data.image_data.back();

        if(!img_arr_long_tavgd_blurred->imagecoll.Gaussian_Pixel_Blur({ }, 1.5)){
            throw std::runtime_error("Unable to blur long temporally averaged images");
        }
    }

    std::vector<std::shared_ptr<Image_Array>> short_tavgd_blurred;
    if(false){
        for(const auto& img_ptr : short_tavgd){
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_ptr ) );
            short_tavgd_blurred.push_back( DICOM_data.image_data.back() );

            if(!short_tavgd_blurred.back()->imagecoll.Gaussian_Pixel_Blur({ }, 1.5)){
                throw std::runtime_error("Unable to blur short temporally averaged images");
            }
        }
    }else{
        for(const auto& img_ptr : short_tavgd) short_tavgd_blurred.push_back( img_ptr );
    }

    //Package the short and long images together as needed for the S0 and T1 calculations.
    std::list<std::reference_wrapper<planar_image_collection<float,double>>> tavgd_blurred;
    tavgd_blurred.emplace_back(img_arr_long_tavgd_blurred->imagecoll);
    for(const auto& img_ptr : short_tavgd_blurred) tavgd_blurred.emplace_back(img_ptr->imagecoll);

    //Deep-copy and process the (possibly blurred) collated image array, generating a T1 map in-situ.
    DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_long_tavgd_blurred ) );
    std::shared_ptr<Image_Array> img_arr_T1_map( DICOM_data.image_data.back() );

    if(!img_arr_T1_map->imagecoll.Transform_Images( DCEMRIT1MapV2, tavgd_blurred, { } )){
        throw std::runtime_error("Unable to transform image array to make T1 map");
    }

    //Produce an S0 map.
    DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_long_tavgd_blurred ) );
    std::shared_ptr<Image_Array> img_arr_S0_map( DICOM_data.image_data.back() );

    if(!img_arr_S0_map->imagecoll.Transform_Images( DCEMRIS0MapV2, tavgd_blurred, { } )){
        throw std::runtime_error("Unable to transform image array to make S0 map");
    }

    //Blur the S0 and T1 maps if needed.
    std::shared_ptr<Image_Array> img_arr_T1_map_blurred( img_arr_T1_map );
    if(false){ //Produce a blurred image.
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_T1_map ) );
        img_arr_T1_map_blurred = DICOM_data.image_data.back();
    
        if(!img_arr_T1_map_blurred->imagecoll.Gaussian_Pixel_Blur({ }, 1.5)){
            throw std::runtime_error("Unable to blur T1 map");
        }
    }

    std::shared_ptr<Image_Array> img_arr_S0_map_blurred( img_arr_S0_map );
    if(false){ //Produce a blurred image.
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_S0_map ) );
        img_arr_S0_map_blurred = DICOM_data.image_data.back();
        
        if(!img_arr_S0_map_blurred->imagecoll.Gaussian_Pixel_Blur({ }, 1.5)){
            throw std::runtime_error("Unable to blur S0 map");
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
            throw std::runtime_error("Unable to transform image array to make C map");
        }

    //Or compute it using the signal difference method (without S0 or T1 maps).
    }else{
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_orig_long_scan ) );
        img_arr_C_map = DICOM_data.image_data.back();

        if(!img_arr_C_map->imagecoll.Transform_Images( DCEMRISigDiffC,
                                                       { img_arr_copy_long_temporally_avgd->imagecoll }, 
                                                       { } )){
            throw std::runtime_error("Unable to transform image array to make poor-man's C map");
        }
    }

    //Compute an IAUC map from the C(t) map.
    if(false){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_C_map ) ); 
        std::shared_ptr<Image_Array> img_arr_iauc_map( DICOM_data.image_data.back() );
    
        if(!img_arr_iauc_map->imagecoll.Process_Images( GroupSpatiallyOverlappingImages, DCEMRIAUCMap, {}, {} )){
            throw std::runtime_error("Unable to process image array to make IAUC map");
        }
    }


    //Compute a histogram over pixel value intensities for each ROI using the original long time series.
    if(false){
        if(!img_arr_orig_long_scan->imagecoll.Transform_Images( PixelHistogramAnalysis,
                                                                { },
                                                                { cc_all } )){
                                                                //{ cc_r_parotid_int, cc_l_parotid_int } )){
                                                                //{ cc_r_parotid_int, cc_l_parotid_int, cc_r_masseter_int, cc_pharynx_int } )){
            throw std::runtime_error("Unable to compute pixel value intensity histograms");
        }else{
            DumpPixelHistogramResults();
        }
    }

    //Deep-copy images at a single temporal point and highlight the ROIs. 
    if(false){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr_copy_long_temporally_avgd ) );
        std::shared_ptr<Image_Array> img_arr_highlighted_rois( DICOM_data.image_data.back() );

        PartitionedImageVoxelVisitorMutatorUserData ud;
            ud.f_bounded = [&](int64_t /*row*/, int64_t /*col*/, int64_t /*channel*/,
                               std::reference_wrapper<planar_image<float,double>> /*img_refw*/,
                               std::reference_wrapper<planar_image<float,double>> /*mask_img_refw*/,
                               float &voxel_val) {
                    voxel_val = 2.0;
            };
            ud.f_unbounded = [&](int64_t /*row*/, int64_t /*col*/, int64_t /*channel*/,
                                 std::reference_wrapper<planar_image<float,double>> /*img_refw*/,
                                 std::reference_wrapper<planar_image<float,double>> /*mask_img_refw*/,
                                 float &voxel_val) {
                    voxel_val = 1.0;
            };
        if(!img_arr_highlighted_rois->imagecoll.Process_Images( GroupIndividualImages, 
                                                                PartitionedImageVoxelVisitorMutator, {},
                                                                { cc_all },
                                                                &ud )){
                                                                //{ cc_r_parotid_int, cc_l_parotid_int, cc_r_masseter_int, cc_pharynx_int } )){
            throw std::runtime_error("Unable to highlight ROIs");
        }
    }

    return true;
}
