//ConvertNaNsToAir.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <any>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Structs.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/CT_Convert_NaNs_to_Air.h"
#include "ConvertNaNsToAir.h"
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


OperationDoc OpArgDocConvertNaNsToAir(){
    OperationDoc out;
    out.name = "ConvertNaNsToAir";
    out.desc = 
        "This operation runs the data through a per-pixel filter, converting NaN's to air in Hounsfield units (-1024).";

    return out;
}

Drover ConvertNaNsToAir(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    for(auto & img_arr : DICOM_data.image_data){
        if(!img_arr->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                               CTNaNsToAir,
                                               {}, {} )){
            FUNCERR("Unable to censor pixels with enormous values");
        }
    }

    return DICOM_data;
}
