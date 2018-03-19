//PreFilterEnormousCTValues.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <experimental/any>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Structs.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/CT_Perf_Pixel_Filter.h"
#include "PreFilterEnormousCTValues.h"
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


std::list<OperationArgDoc> OpArgDocPreFilterEnormousCTValues(void){
    return std::list<OperationArgDoc>();
}

Drover PreFilterEnormousCTValues(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //This operation runs the data through a per-pixel filter, censoring pixels which are too high to legitimately
    // show up in a clinical CT. Censored pixels are set to NaN. Data is modified and no copy is made!
    for(auto & img_arr : DICOM_data.image_data){
        if(!img_arr->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                               CTPerfEnormousPixelFilter,
                                               {}, {} )){
            FUNCERR("Unable to censor pixels with enormous values");
        }
    }

    return DICOM_data;
}
