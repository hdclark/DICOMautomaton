//CT_Liver_Perfusion_Ortho_Views.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

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
#include "../YgorImages_Functors/Processing/CT_Reasonable_HU_Window.h"
#include "../YgorImages_Functors/Processing/Orthogonal_Slices.h"
#include "CT_Liver_Perfusion_Ortho_Views.h"
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


OperationDoc OpArgDocCT_Liver_Perfusion_Ortho_Views (){
    OperationDoc out;
    out.name = "CT_Liver_Perfusion_Ortho_Views ";
    out.desc = 
        "This operation performed dynamic contrast-enhanced CT perfusion image modeling on a time series image volume.";

    out.notes.emplace_back(
        "Use this mode when you are only interested in oblique/orthogonal views."
        " The point of this operation is to keep memory low so image sets can be compared."
    );
    return out;
}

bool CT_Liver_Perfusion_Ortho_Views(Drover &DICOM_data,
                                      const OperationArgPkg& /*OptArgs*/,
                                      std::map<std::string, std::string>& /*InvocationMetadata*/,
                                      const std::string& /*FilenameLex*/){

    //Get handles for each of the original image arrays so we can easily refer to them later.
    std::vector<std::shared_ptr<Image_Array>> orig_img_arrays;
    for(auto & img_arr : DICOM_data.image_data) orig_img_arrays.push_back(img_arr);

    //Force the window to something reasonable to be uniform and cover normal tissue HU range.
    if(true) for(auto & img_arr : orig_img_arrays){
        if(!img_arr->imagecoll.Process_Images( GroupIndividualImages,
                                               StandardAbdominalHUWindow,
                                               {}, {} )){
            throw std::runtime_error("Unable to force window to cover reasonable HU range");
        }
    }

    //Construct perpendicular image slices that align with first row and column of the first image.
    std::vector<std::shared_ptr<Image_Array>> intersecting_row;
    std::vector<std::shared_ptr<Image_Array>> intersecting_col;

    if(true) for(auto & img_arr : orig_img_arrays){ //temp_avgd){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
        intersecting_row.emplace_back( DICOM_data.image_data.back() );

        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( ) );
        intersecting_col.emplace_back( DICOM_data.image_data.back() );


        if(!img_arr->imagecoll.Process_Images( GroupTemporallyOverlappingImages,
                                               OrthogonalSlices,
                                               { std::ref(intersecting_row.back()->imagecoll),
                                                 std::ref(intersecting_col.back()->imagecoll) },
                                               {}, {} )){
            throw std::runtime_error("Unable to generate orthogonal image slices");
        }else{
            img_arr->imagecoll.images.clear();
        }
    }

    //Force the window to something reasonable to be uniform and cover normal tissue HU range.
    if(true) for(auto & img_arr : intersecting_row){
        if(!img_arr->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                               StandardAbdominalHUWindow,
                                               {}, {} )){
            throw std::runtime_error("Unable to force window to cover reasonable HU range");
        }
    }
    if(true) for(auto & img_arr : intersecting_col){
        if(!img_arr->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                               StandardAbdominalHUWindow,
                                               {}, {} )){
            throw std::runtime_error("Unable to force window to cover reasonable HU range");
        }
    }


    return true;
}
