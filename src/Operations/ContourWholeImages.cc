//ContourWholeImages.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <algorithm>
#include <experimental/optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "ContourWholeImages.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.



std::list<OperationArgDoc> OpArgDocContourWholeImages(void){
    std::list<OperationArgDoc> out;

    // This operation constructs contours for an ROI that encompasses the whole of all specified images.
    // It is useful for operations that operate on ROIs whenever you want to compute something over the whole image.
    // This routine avoids having to manually contour anything.
    // The output is 'ephemeral' and is not commited to any database.
    //
    // NOTE: This routine will attempt to avoid repeat contours. Generated contours are tested for intersection with an
    //       image before the image is processed. 
    //
    // NOTE: Existing contours are ignored and unaltered.
    //
    // NOTE: Contours are set slightly inside the outer boundary so they can be easily visualized by overlaying on the
    //       image. All voxel centres will be within the bounds.
    //

    out.emplace_back();
    out.back().name = "ROILabel";
    out.back().desc = "A label to attach to the ROI contours.";
    out.back().default_val = "everything";
    out.back().expected = true;
    out.back().examples = { "everything", "whole_images", "unspecified" };

    
    out.emplace_back();
    out.back().name = "ImageSelection";
    out.back().desc = "Image collection to operate on. Either 'none', 'last', or 'all'.";
    out.back().default_val = "last";
    out.back().expected = true;
    out.back().examples = { "none", "last", "all" };

    return out;
}



Drover ContourWholeImages(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabel = OptArgs.getValueStr("ROILabel").value();
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    const auto regex_none = std::regex("no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last = std::regex("la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all  = std::regex("al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    if( !std::regex_match(ImageSelectionStr, regex_none)
    &&  !std::regex_match(ImageSelectionStr, regex_last)
    &&  !std::regex_match(ImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Image selection is not valid. Cannot continue.");
    }

    Explicator X(FilenameLex);
    const auto NormalizedROILabel = X(ROILabel);
    const long int ROINumber = 10001; // TODO: find highest existing and ++ it.

    //Construct a destination for the ROI contours.
    if(DICOM_data.contour_data == nullptr){
        std::unique_ptr<Contour_Data> output (new Contour_Data());
        DICOM_data.contour_data = std::move(output);
    }
    DICOM_data.contour_data->ccs.emplace_back();

    DICOM_data.contour_data->ccs.back().Raw_ROI_name = ROILabel;
    DICOM_data.contour_data->ccs.back().ROI_number = ROINumber;
    DICOM_data.contour_data->ccs.back().Minimum_Separation = 1.0; // TODO: is there a routine to do this? (YES: Unique_Contour_Planes()...)

    //Iterate over each requested image_array. Each image is processed independently, so a thread pool is used.
    auto iap_it = DICOM_data.image_data.begin();
    if(false){
    }else if(std::regex_match(ImageSelectionStr, regex_none)){ iap_it = DICOM_data.image_data.end();
    }else if(std::regex_match(ImageSelectionStr, regex_last)){
        if(!DICOM_data.image_data.empty()) iap_it = std::prev(DICOM_data.image_data.end());
    }
    for( ; iap_it != DICOM_data.image_data.end(); ++iap_it){
        std::list<std::reference_wrapper<planar_image<float,double>>> imgs;
        for(auto &animg : (*iap_it)->imagecoll.images){
            imgs.emplace_back( std::ref(animg) );
        }

        Encircle_Images_with_Contours_Opts opts;
        opts.inclusivity = Encircle_Images_with_Contours_Opts::Inclusivity::Centre;
        opts.contouroverlap = Encircle_Images_with_Contours_Opts::ContourOverlap::Disallow;

        //Prepare contour metadata using image metadata.
        //
        //Note: We currently attach *all* common image data to each contour. Should we filter some (most) out?
        std::map<std::string,std::string> metadata;
        metadata = DICOM_data.image_data.back()->imagecoll.get_common_metadata({});
        metadata["ROINumber"] = std::to_string(ROINumber);
        metadata["ROIName"] = ROILabel;
        metadata["NormalizedROIName"] = NormalizedROILabel;
        metadata["MinimumSeparation"] = metadata["SliceThickness"];
        metadata["Description"] = "Whole-Image Contour";

        auto cc = Encircle_Images_with_Contours(imgs, opts, metadata);

        DICOM_data.contour_data->ccs.back().contours.splice(
             DICOM_data.contour_data->ccs.back().contours.end(),
             cc.contours);
    }

    return DICOM_data;
}
