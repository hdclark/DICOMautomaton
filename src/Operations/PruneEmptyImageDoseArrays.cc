//PruneEmptyImageDoseArrays.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <algorithm>
#include <fstream>
#include <list>
#include <map>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "PruneEmptyImageDoseArrays.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.

OperationDoc OpArgDocPruneEmptyImageDoseArrays(void){
    OperationDoc out;
    out.name = "PruneEmptyImageDoseArrays";
    out.desc = "This operation deletes Image_Arrays that do not contain any images.";

    return out;
}

Drover PruneEmptyImageDoseArrays(Drover DICOM_data, OperationArgPkg, std::map<std::string,std::string> , std::string){

    FUNCINFO("Pre-prune: there are " << DICOM_data.image_data.size() << " image_arrays");
/*
    for(auto it = DICOM_data.image_data.begin(); it != DICOM_data.image_data.end();  ){
        if((*it)->imagecoll.images.empty()){
            it = DICOM_data.image_data.erase(it);
        }else{
            ++it;
        }
    }
*/

    DICOM_data.image_data.erase(
        std::remove_if(DICOM_data.image_data.begin(), DICOM_data.image_data.end(), 
            [](auto img_arr_ptr) -> bool {
                return img_arr_ptr->imagecoll.images.empty();
            }),
        DICOM_data.image_data.end());

    DICOM_data.dose_data.erase(
        std::remove_if(DICOM_data.dose_data.begin(), DICOM_data.dose_data.end(), 
            [](auto dose_arr_ptr) -> bool {
                return dose_arr_ptr->imagecoll.images.empty();
            }),
        DICOM_data.dose_data.end());

    FUNCINFO("Post-prune: " << DICOM_data.image_data.size() << " image_arrays remain");

    return DICOM_data;
}
