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
        "This operation converts all loaded Image_Arrays to Dose_Arrays. Neither image contents nor metadata should change,"
        " but the intent to treat as an image or dose matrix will of course change. A deep copy may be performed.";

    return out;
}

Drover ConvertImageToDose(Drover DICOM_data, OperationArgPkg, std::map<std::string,std::string>, std::string ){

    for(const auto &ia : DICOM_data.image_data){
        DICOM_data.dose_data.emplace_back();
        DICOM_data.dose_data.back() = std::make_shared<Dose_Array>(*ia);
    }
    DICOM_data.image_data.clear();

    return DICOM_data;
}
