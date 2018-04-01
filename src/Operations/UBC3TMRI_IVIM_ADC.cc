//UBC3TMRI_IVIM_ADC.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <experimental/any>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/IVIMMRI_ADC_Map.h"
#include "../YgorImages_Functors/Processing/Time_Course_Slope_Map.h"
#include "UBC3TMRI_IVIM_ADC.h"
#include "YgorImages.h"


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

        if(!adc_map_img_arrays.back()->imagecoll.Process_Images( GroupSpatiallyTemporallyOverlappingImages, 
                                                                 IVIMMRIADCMap, 
                                                                 {}, {} )){
            throw std::runtime_error("Unable to generate ADC map");
        }
    }


    //Deep-copy the ADC map and compute a slope-sign map.
    if(false){
        std::vector<std::shared_ptr<Image_Array>> slope_sign_map_img_arrays;
        auto TimeCourseSlopeMapAllTime = std::bind(TimeCourseSlopeMap, 
                                                   std::placeholders::_1, std::placeholders::_2, 
                                                   std::placeholders::_3, std::placeholders::_4,
                                                   std::numeric_limits<double>::min(), std::numeric_limits<double>::max(),
                                                   std::experimental::any());
        for(auto & img_arr : adc_map_img_arrays){
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
            slope_sign_map_img_arrays.emplace_back( DICOM_data.image_data.back() );

            if(!slope_sign_map_img_arrays.back()->imagecoll.Process_Images( GroupSpatiallyOverlappingImages,
                                                                            TimeCourseSlopeMapAllTime,
                                                                            {}, {} )){
                throw std::runtime_error("Unable to compute time course slope map");
            }
        }
    }

    return DICOM_data;
}
