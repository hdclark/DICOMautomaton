//UBC3TMRI_DCE.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <experimental/any>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    
#include <vector>

#include "../Structs.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/DCEMRI_AUC_Map.h"
#include "../YgorImages_Functors/Processing/Partitioned_Image_Voxel_Visitor_Mutator.h"
#include "../YgorImages_Functors/Processing/Kitchen_Sink_Analysis.h"
#include "../YgorImages_Functors/Transform/DCEMRI_Signal_Difference_C.h"
#include "UBC3TMRI_DCE.h"
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.

template <class T> class contour_collection;


std::list<OperationArgDoc> OpArgDocUBC3TMRI_DCE(void){
    return std::list<OperationArgDoc>();
}

Drover UBC3TMRI_DCE(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string,std::string> InvocationMetadata, std::string /*FilenameLex*/){

    //============================================= UBC3TMRI Vol01 DCE ================================================
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

    //Get named handles for each image array so we can easily refer to them later.
    //std::shared_ptr<Image_Array> img_arr_orig_long_scan = *std::next(DICOM_data.image_data.begin(),0);
    //std::shared_ptr<Image_Array> img_arr_orig_short_scan = *std::next(DICOM_data.image_data.begin(),1);

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

    //Deep-copy, trim the post-contrast injection signal, and temporally-average the image arrays.
    auto PurgeAboveNSeconds = std::bind(PurgeAboveTemporalThreshold, std::placeholders::_1, ContrastInjectionLeadTime);
    std::vector<std::shared_ptr<Image_Array>> temporal_avg_img_arrays;
    for(auto & img_arr : orig_img_arrays){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        temporal_avg_img_arrays.emplace_back( DICOM_data.image_data.back() );

        temporal_avg_img_arrays.back()->imagecoll.Prune_Images_Satisfying(PurgeAboveNSeconds);

        if(!temporal_avg_img_arrays.back()->imagecoll.Condense_Average_Images(GroupSpatiallyOverlappingImages)){
            FUNCERR("Cannot temporally average data set. Is it able to be averaged?");
        }
    }

    //Deep-copy images at a single temporal point and highlight the ROIs. 
    if(false && !cc_all.empty()){
        std::vector<std::shared_ptr<Image_Array>> roi_highlighted_img_arrays;
        for(auto & img_arr : temporal_avg_img_arrays){
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
            roi_highlighted_img_arrays.emplace_back( DICOM_data.image_data.back() );

            PartitionedImageVoxelVisitorMutatorUserData ud;
            ud.f_bounded = [&](long int /*row*/, long int /*col*/, long int /*channel*/, float &voxel_val) {
                    voxel_val = 2.0;
            };
            ud.f_unbounded = [&](long int /*row*/, long int /*col*/, long int /*channel*/, float &voxel_val) {
                    voxel_val = 1.0;
            };
            if(!roi_highlighted_img_arrays.back()->imagecoll.Process_Images( GroupIndividualImages,
                                                                             PartitionedImageVoxelVisitorMutator,
                                                                             {}, cc_all,
                                                                             &ud )){
                FUNCERR("Unable to highlight ROIs");
            }
        }
    }

    //Deep-copy temporally-averaged images and blur them.
    std::vector<std::shared_ptr<Image_Array>> tavgd_blurred;
    if(false){
        for(auto img_ptr : temporal_avg_img_arrays){
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_ptr ) );
            tavgd_blurred.push_back( DICOM_data.image_data.back() );

            if(!tavgd_blurred.back()->imagecoll.Gaussian_Pixel_Blur({ }, 1.5)){
                FUNCERR("Unable to blur temporally averaged images");
            }
        }
    }else{
        for(auto img_ptr : temporal_avg_img_arrays) tavgd_blurred.push_back( img_ptr );
    }



    //Deep-copy the original long image array and use the temporally-averaged, pre-contrast map to work out the poor-man's Gad C in each voxel.
    std::vector<std::shared_ptr<Image_Array>> poormans_C_map_img_arrays;
    {
        auto img_arr = orig_img_arrays.front();
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        poormans_C_map_img_arrays.emplace_back( DICOM_data.image_data.back() );

        if(!poormans_C_map_img_arrays.back()->imagecoll.Transform_Images( DCEMRISigDiffC,
                                                                          { tavgd_blurred.front()->imagecoll },
                                                                          { } )){
            FUNCERR("Unable to transform image array to make poor-man's C map");
        }
    }

    //Deep-copy the poor-man's C(t) map and use the images to compute an IAUC map.
    //
    // NOTE: Takes a LONG time. You need to modify the IAUC code's Ygor... integration routine.
    //       I think it might be using a generic integration routine which samples the integrand 100 times
    //       between each datum(!). Surely you can improve this for a run-of-the-mill linear integrand.
    if(false){
        std::vector<std::shared_ptr<Image_Array>> iauc_c_map_img_arrays;
        for(auto & img_arr : poormans_C_map_img_arrays){
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
            iauc_c_map_img_arrays.emplace_back( DICOM_data.image_data.back() );

            if(!iauc_c_map_img_arrays.back()->imagecoll.Process_Images( GroupSpatiallyOverlappingImages,
                                                                        DCEMRIAUCMap,
                                                                        {}, {} )){
                FUNCERR("Unable to process image array to make IAUC map");
            }
        }
    }

    //Deep-copy the poor-man's C(t) map and use the images to perform a "kitchen sink" analysis.
    if(false){
        std::vector<std::shared_ptr<Image_Array>> kitchen_sink_map_img_arrays;
        if(poormans_C_map_img_arrays.size() == 1){
            for(auto & img_arr : poormans_C_map_img_arrays){
                DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
                kitchen_sink_map_img_arrays.emplace_back( DICOM_data.image_data.back() );
                
                if(!kitchen_sink_map_img_arrays.back()->imagecoll.Process_Images( GroupSpatiallyOverlappingImages,
                                                                                  KitchenSinkAnalysis,
                                                                                  {}, cc_all )){
                    FUNCERR("Unable to process image array to perform kitchen sink analysis");
                }else{
                    DumpKitchenSinkResults(InvocationMetadata);
                }
            }
        }else{
            FUNCWARN("Skipping kitchen sink analysis. This routine uses static storage and assumes it will "
                     "be run over a single image array.");
        }
    }
    
    return DICOM_data;
}
