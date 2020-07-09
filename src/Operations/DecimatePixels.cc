//DecimatePixels.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <optional>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Structs.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Pixel_Decimate.h"
#include "DecimatePixels.h"
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


OperationDoc OpArgDocDecimatePixels(){
    OperationDoc out;
    out.name = "DecimatePixels";

    out.desc = 
        " This operation spatially aggregates blocks of pixels, thereby decimating them and making the images consume"
        " far less memory. The precise size reduction and spatial aggregate can be set in the source.";

    out.args.emplace_back();
    out.args.back().name = "OutSizeR";
    out.args.back().desc = "The number of pixels along the row unit vector to group into an outgoing pixel."
                      " Must be a multiplicative factor of the incoming image's row count."
                      " No decimation occurs if either this or 'OutSizeC' is zero or negative.";
    out.args.back().default_val = "8";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "2", "4", "8", "16", "32", "64", "128", "256", "512" };

    out.args.emplace_back();
    out.args.back().name = "OutSizeC";
    out.args.back().desc = "The number of pixels along the column unit vector to group into an outgoing pixel."
                      " Must be a multiplicative factor of the incoming image's column count."
                      " No decimation occurs if either this or 'OutSizeR' is zero or negative.";
    out.args.back().default_val = "8";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "2", "4", "8", "16", "32", "64", "128", "256", "512" };

    return out;
}

Drover DecimatePixels(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

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
                                    std::placeholders::_5);

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
