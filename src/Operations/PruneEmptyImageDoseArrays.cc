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

OperationDoc OpArgDocPruneEmptyImageDoseArrays(){
    OperationDoc out;
    out.name = "PruneEmptyImageDoseArrays";
    out.desc = "This operation deletes Image_Arrays that do not contain any images.";

    return out;
}

bool PruneEmptyImageDoseArrays(Drover &DICOM_data,
                                 const OperationArgPkg&,
                                 std::map<std::string, std::string>& /*InvocationMetadata*/,
                                 const std::string&){

    FUNCINFO("Pre-prune: there are " << DICOM_data.image_data.size() << " image_arrays");

    DICOM_data.image_data.erase(
        std::remove_if(DICOM_data.image_data.begin(), DICOM_data.image_data.end(), 
            [](auto img_arr_ptr) -> bool {
                return img_arr_ptr->imagecoll.images.empty();
            }),
        DICOM_data.image_data.end());

    FUNCINFO("Post-prune: " << DICOM_data.image_data.size() << " image_arrays remain");

    return true;
}
