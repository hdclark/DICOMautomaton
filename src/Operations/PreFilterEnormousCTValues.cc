//PreFilterEnormousCTValues.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <any>
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
#include "YgorLog.h"


OperationDoc OpArgDocPreFilterEnormousCTValues(){
    OperationDoc out;
    out.name = "PreFilterEnormousCTValues";
    out.desc = 
        " This operation runs the data through a per-pixel filter, censoring pixels which are too high to legitimately"
        " show up in a clinical CT. Censored pixels are set to NaN. Data is modified and no copy is made!";

    return out;
}

bool PreFilterEnormousCTValues(Drover &DICOM_data,
                                 const OperationArgPkg& /*OptArgs*/,
                                 std::map<std::string, std::string>& /*InvocationMetadata*/,
                                 const std::string& /*FilenameLex*/){

    for(auto & img_arr : DICOM_data.image_data){
        if(!img_arr->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                               CTPerfEnormousPixelFilter,
                                               {}, {} )){
            throw std::runtime_error("Unable to censor pixels with enormous values");
        }
    }

    return true;
}
