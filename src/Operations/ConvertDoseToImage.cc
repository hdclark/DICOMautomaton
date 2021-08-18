//ConvertDoseToImage.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "ConvertDoseToImage.h"


OperationDoc OpArgDocConvertDoseToImage(){
    OperationDoc out;
    out.name = "ConvertDoseToImage";

    out.desc = 
        "This operation converts all loaded images from RTDOSE modality to CT modality. Image contents will not change,"
        " but the intent to treat as an image or dose matrix will of course change.";

    out.args.emplace_back();
    out.args.back().name = "Modality";
    out.args.back().desc = "The modality that will replace 'RTDOSE'.";
    out.args.back().default_val = "CT";
    out.args.back().expected = true;
    out.args.back().examples = { "CT",
                                 "MR",
                                 "UNKNOWN" };

    return out;
}

bool ConvertDoseToImage(Drover &DICOM_data,
                          const OperationArgPkg& OptArgs,
                          const std::map<std::string, std::string>&,
                          const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ModalityStr = OptArgs.getValueStr("Modality").value();

    //-----------------------------------------------------------------------------------------------------------------

    for(auto &ia_ptr : DICOM_data.image_data){
        for(auto &img : ia_ptr->imagecoll.images){
            if( (img.metadata.count("Modality") != 0)
            &&  (img.metadata["Modality"] == "RTDOSE") ) img.metadata["Modality"] = ModalityStr;
        }
    }

    return true;
}
