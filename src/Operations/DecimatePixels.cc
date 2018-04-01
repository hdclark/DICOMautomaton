//DecimatePixels.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <experimental/any>
#include <experimental/optional>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Pixel_Decimate.h"
#include "DecimatePixels.h"
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


std::list<OperationArgDoc> OpArgDocDecimatePixels(void){
    std::list<OperationArgDoc> out;

    out.emplace_back();
    out.back().name = "OutSizeR";
    out.back().desc = "The number of pixels along the row unit vector to group into an outgoing pixel."
                      " Must be a multiplicative factor of the incoming image's row count."
                      " No decimation occurs if either this or 'OutSizeC' is zero or negative.";
    out.back().default_val = "8";
    out.back().expected = true;
    out.back().examples = { "0", "2", "4", "8", "16", "32", "64", "128", "256", "512" };

    out.emplace_back();
    out.back().name = "OutSizeC";
    out.back().desc = "The number of pixels along the column unit vector to group into an outgoing pixel."
                      " Must be a multiplicative factor of the incoming image's column count."
                      " No decimation occurs if either this or 'OutSizeR' is zero or negative.";
    out.back().default_val = "8";
    out.back().expected = true;
    out.back().examples = { "0", "2", "4", "8", "16", "32", "64", "128", "256", "512" };

    return out;
}

Drover DecimatePixels(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //This operation spatially aggregates blocks of pixels, thereby decimating them and making the images consume
    // far less memory. The precise size reduction and spatial aggregate can be set in the source. 

    //---------------------------------------------- User Parameters --------------------------------------------------
    const long int DecimateR = std::stol( OptArgs.getValueStr("OutSizeR").value() );
    const long int DecimateC = std::stol( OptArgs.getValueStr("OutSizeC").value() );
    //-----------------------------------------------------------------------------------------------------------------

    //Decimate the number of pixels for modeling purposes.
    if((DecimateR > 0) && (DecimateC > 0)){
        auto DecimateRC = std::bind(InImagePlanePixelDecimate, 
                                    std::placeholders::_1, std::placeholders::_2, 
                                    std::placeholders::_3, std::placeholders::_4,
                                    DecimateR, DecimateC,
                                    std::experimental::any());


        for(auto & img_arr : DICOM_data.image_data){
            if(!img_arr->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                   DecimateRC,
                                                   {}, {} )){
                FUNCERR("Unable to decimate pixels");
            }
        }
    }

    return DICOM_data;
}
