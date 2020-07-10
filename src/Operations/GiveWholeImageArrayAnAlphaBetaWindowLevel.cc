//GiveWholeImageArrayAnAlphaBetaWindowLevel.cc - A part of DICOMautomaton 2020. Written by hal clark.

#include <any>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Structs.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/CT_Reasonable_HU_Window.h"
#include "GiveWholeImageArrayAnAlphaBetaWindowLevel.h"
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


OperationDoc OpArgDocGiveWholeImageArrayAnAlphaBetaWindowLevel(){
    OperationDoc out;
    out.name = "GiveWholeImageArrayAnAlphaBetaWindowLevel";

    out.desc = 
        "This operation runs the images in an image array through a uniform window-and-leveler instead of per-slice"
        " window-and-level or no window-and-level at all. Data is modified and no copy is made!";

    return out;
}

Drover GiveWholeImageArrayAnAlphaBetaWindowLevel(Drover DICOM_data, const OperationArgPkg& /*OptArgs*/, const std::map<std::string,std::string>& /*InvocationMetadata*/, const std::string& /*FilenameLex*/){
    for(auto & img_arr : DICOM_data.image_data){
        if(!img_arr->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                               StandardAlphaBetaWindow,
                                               {},{} )){
            FUNCERR("Unable to force window to cover a reasonable alpha/beta range");
        }
    }

    return DICOM_data;
}
