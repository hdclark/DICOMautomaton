//ConvertNaNsToZeros.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <any>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Structs.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/Convert_NaNs_to_Zero.h"
#include "ConvertNaNsToZeros.h"
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


OperationDoc OpArgDocConvertNaNsToZeros(){
    OperationDoc out;
    out.name = "ConvertNaNsToZeros";

    out.desc = 
        "This operation runs the data through a per-pixel filter, converting NaN's to zeros.";

    return out;
}

Drover ConvertNaNsToZeros(Drover DICOM_data, const OperationArgPkg& /*OptArgs*/, const std::map<std::string,std::string>& /*InvocationMetadata*/, const std::string& /*FilenameLex*/){

    for(auto & img_arr : DICOM_data.image_data){
        if(!img_arr->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                               NaNsToZeros,
                                               {}, {} )){
            FUNCERR("Unable to censor NaN pixels");
        }
    }

    return DICOM_data;
}
