//MaxMinPixels.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <any>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Structs.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/Max-Min_Pixel_Value.h"
#include "MaxMinPixels.h"
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


OperationDoc OpArgDocMaxMinPixels(){
    OperationDoc out;
    out.name = "MaxMinPixels";
    out.desc = 
        "This operation replaces pixels with the pixel-wise difference (max)-(min).";

    return out;
}

Drover MaxMinPixels(Drover DICOM_data,
                    const OperationArgPkg& /*OptArgs*/,
                    const std::map<std::string, std::string>&
                    /*InvocationMetadata*/,
                    const std::string& /*FilenameLex*/){

    auto img_arr = DICOM_data.image_data.back();
    if(!img_arr->imagecoll.Process_Images_Parallel( GroupSpatiallyOverlappingImages,
                                           CondenseMaxMinPixel,
                                           {}, {} )){
        FUNCERR("Unable to max-min pixel values");
    }

    return DICOM_data;
}
