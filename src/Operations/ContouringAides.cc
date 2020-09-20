//ContouringAides.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <any>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/Logarithmic_Pixel_Scale.h"
#include "ContouringAides.h"
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


OperationDoc OpArgDocContouringAides(){
    OperationDoc out;
    out.name = "ContouringAides";
    out.desc = "This operation attempts to prepare an image for easier contouring.";

    out.notes.emplace_back(
        "At the moment, only logarithmic scaling is applied."
    );

    return out;
}

Drover ContouringAides(Drover DICOM_data,
                       const OperationArgPkg& /*OptArgs*/,
                       const std::map<std::string, std::string>&
                       /*InvocationMetadata*/,
                       const std::string& /*FilenameLex*/){

    //Get handles for each of the original image arrays so we can easily refer to them later.
    std::vector<std::shared_ptr<Image_Array>> orig_img_arrays;
    for(auto & img_arr : DICOM_data.image_data) orig_img_arrays.push_back(img_arr);

    //Scale the pixel intensities on a logarithmic scale. (For viewing purposes only!)
    std::vector<std::shared_ptr<Image_Array>> log_scaled_img_arrays;
    if(true) for(auto & img_arr : orig_img_arrays){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        log_scaled_img_arrays.emplace_back( DICOM_data.image_data.back() );

        if(!log_scaled_img_arrays.back()->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                                    LogScalePixels,
                                                                    {}, {} )){
            FUNCERR("Unable to perform logarithmic pixel scaling");
        }
    }

    return DICOM_data;
}
