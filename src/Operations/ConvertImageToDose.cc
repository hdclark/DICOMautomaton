//ConvertImageToDose.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "ConvertImageToDose.h"


OperationDoc OpArgDocConvertImageToDose(){
    OperationDoc out;
    out.name = "ConvertImageToDose";
    out.tags.emplace_back("category: image processing");
    out.tags.emplace_back("category: radiation dose");

    out.desc = 
        "This operation converts all loaded image modalities into RTDOSE. Image contents will not change,"
        " but the intent to treat as an image or dose matrix will of course change.";

    return out;
}

bool ConvertImageToDose(Drover &DICOM_data,
                          const OperationArgPkg&,
                          std::map<std::string, std::string>& /*InvocationMetadata*/,
                          const std::string&){

    for(auto &ia_ptr : DICOM_data.image_data){
        for(auto &img : ia_ptr->imagecoll.images){
            if( (img.metadata.count("Modality") != 0)
            &&  (img.metadata["Modality"] != "RTDOSE") ) img.metadata["Modality"] = "RTDOSE";
        }
    }

    return true;
}
