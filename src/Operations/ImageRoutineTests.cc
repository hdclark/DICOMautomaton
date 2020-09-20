//ImageRoutineTests.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <any>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/ImagePartialDerivative.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bicubic_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bilinear_Supersample.h"
#include "ImageRoutineTests.h"
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


OperationDoc OpArgDocImageRoutineTests(){
    OperationDoc out;
    out.name = "ImageRoutineTests";
    out.desc = "This operation performs a series of sub-operations that are generally useful when inspecting an image.";

    return out;
}

Drover ImageRoutineTests(Drover DICOM_data,
                         const OperationArgPkg& /*OptArgs*/,
                         const std::map<std::string, std::string>&
                         /*InvocationMetadata*/,
                         const std::string& /*FilenameLex*/){

    //Get handles for each of the original image arrays so we can easily refer to them later.
    std::vector<std::shared_ptr<Image_Array>> orig_img_arrays;
    for(auto & img_arr : DICOM_data.image_data) orig_img_arrays.push_back(img_arr);


    //Deep-copy, resample the original images using bilinear interpolation, for image viewing, contours, etc..
    InImagePlaneBilinearSupersampleUserData bilin_ud;

    std::vector<std::shared_ptr<Image_Array>> bilin_resampled_img_arrays;
    for(auto & img_arr : orig_img_arrays){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        bilin_resampled_img_arrays.emplace_back( DICOM_data.image_data.back() );

        if(!bilin_resampled_img_arrays.back()->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                                         InImagePlaneBilinearSupersample,
                                                                         {}, {}, &bilin_ud )){
            FUNCERR("Unable to bilinearly supersample images");
        }
    }

    //Deep-copy, resample the original images using bicubic interpolation, for image viewing, contours, etc..
    InImagePlaneBicubicSupersampleUserData bicub_ud;

    std::vector<std::shared_ptr<Image_Array>> bicub_resampled_img_arrays;
    if(false) for(auto & img_arr : orig_img_arrays){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        bicub_resampled_img_arrays.emplace_back( DICOM_data.image_data.back() );

        if(!bicub_resampled_img_arrays.back()->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                                         InImagePlaneBicubicSupersample,
                                                                         {}, {}, &bicub_ud )){
            FUNCERR("Unable to bicubically supersample images");
        }
    }


    //Deep-copy, convert the original images to their 'cross' second-order partial derivative (for edge-finding).
    ImagePartialDerivativeUserData csd_ud;
    csd_ud.order = PartialDerivativeEstimator::second;
    csd_ud.method = PartialDerivativeMethod::cross;

    std::vector<std::shared_ptr<Image_Array>> cross_second_deriv_img_arrays;
    for(auto & img_arr : orig_img_arrays){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        cross_second_deriv_img_arrays.emplace_back( DICOM_data.image_data.back() );

        if(!cross_second_deriv_img_arrays.back()->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                                            ImagePartialDerivative,
                                                                            {}, {}, &csd_ud )){
            FUNCERR("Unable to compute 'cross' second-order partial derivative");
        }
    }


    return DICOM_data;
}
