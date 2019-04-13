//ConvertImageToDose.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "ConvertImageToDose.h"


OperationDoc OpArgDocConvertImageToDose(void){
    OperationDoc out;
    out.name = "ConvertImageToDose";

    out.desc = 
        "This operation converts all loaded image modalities into RTDOSE. Image contents will not change,"
        " but the intent to treat as an image or dose matrix will of course change.";

    return out;
}

Drover ConvertImageToDose(Drover DICOM_data, OperationArgPkg, std::map<std::string,std::string>, std::string ){

    for(auto &ia_ptr : DICOM_data.image_data){
        for(auto &img : ia_ptr->imagecoll.images){
            if( (img.metadata.count("Modality") != 0)
            &&  (img.metadata["Modality"] != "RTDOSE") ) img.metadata["Modality"] = "RTDOSE";
        }
    }

    return DICOM_data;
}
