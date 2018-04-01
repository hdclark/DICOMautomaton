//DCEMRI_IAUC.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <experimental/any>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/DCEMRI_AUC_Map.h"
#include "DCEMRI_IAUC.h"
#include "YgorImages.h"


std::list<OperationArgDoc> OpArgDocDCEMRI_IAUC(void){
    return std::list<OperationArgDoc>();
}

Drover DCEMRI_IAUC(Drover DICOM_data, OperationArgPkg, std::map<std::string,std::string> , std::string){

    //This operation will compute the Integrated Area Under the Curve (IAUC) for any images present.
    // It can be fairly slow, especially if the image volume is huge, so it is best to crop images if possible.
    //

    std::vector<std::shared_ptr<Image_Array>> orig_img_arrays;
    for(auto & img_arr : DICOM_data.image_data) orig_img_arrays.push_back(img_arr);


    //Compute the IAUC for each image array.
    //
    // NOTE: Takes a LONG time. You need to modify the IAUC code's Ygor... integration routine.
    //       I think it might be using a generic integration routine which samples the integrand 100 times
    //       between each datum(!). Surely you can improve this for a run-of-the-mill linear integrand.
    for(auto & img_arr : orig_img_arrays){
        if(!img_arr->imagecoll.Process_Images( GroupSpatiallyOverlappingImages,
                                               DCEMRIAUCMap,
                                               {}, {} )){
            throw std::runtime_error("Unable to process image array to make IAUC map.");
        }
    }
    
    return DICOM_data;
}
