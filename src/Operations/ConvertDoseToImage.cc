//ConvertDoseToImage.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "ConvertDoseToImage.h"


OperationDoc OpArgDocConvertDoseToImage(void){
    OperationDoc out;
    out.name = "ConvertDoseToImage";

    out.desc = 
        "This operation converts all loaded Dose_Arrays to Image_Arrays. Neither image contents nor metadata should change,"
        " but the intent to treat as an image or dose matrix will of course change. A deep copy may be performed.";

    return out;
}

Drover ConvertDoseToImage(Drover DICOM_data, OperationArgPkg, std::map<std::string,std::string>, std::string ){

    for(const auto &da : DICOM_data.dose_data){
        DICOM_data.image_data.emplace_back();
        DICOM_data.image_data.back() = std::make_shared<Image_Array>(*da);
    }
    DICOM_data.dose_data.clear();

    return DICOM_data;
}
