//ContourWholeImages.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <algorithm>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "ContourWholeImages.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocContourWholeImages(){
    OperationDoc out;
    out.name = "ContourWholeImages";

    out.desc = 
        "This operation constructs contours for an ROI that encompasses the whole of all specified images."
        " It is useful for operations that operate on ROIs whenever you want to compute something over the whole image."
        " This routine avoids having to manually contour anything."
        " The output is 'ephemeral' and is not commited to any database.";
        
    out.notes.emplace_back(
        "This routine will attempt to avoid repeat contours. Generated contours are tested for intersection with an"
        " image before the image is processed."
    );
        
    out.notes.emplace_back(
        "Existing contours are ignored and unaltered."
    );
        
    out.notes.emplace_back(
        "Contours are set slightly inside the outer boundary so they can be easily visualized by overlaying on the"
        " image. All voxel centres will be within the bounds."
    );

    out.args.emplace_back();
    out.args.back().name = "ROILabel";
    out.args.back().desc = "A label to attach to the ROI contours.";
    out.args.back().default_val = "everything";
    out.args.back().expected = true;
    out.args.back().examples = { "everything", "whole_images", "unspecified" };

    
    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    return out;
}



bool ContourWholeImages(Drover &DICOM_data,
                          const OperationArgPkg& OptArgs,
                          std::map<std::string, std::string>& /*InvocationMetadata*/,
                          const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabel = OptArgs.getValueStr("ROILabel").value();
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    Explicator X(FilenameLex);
    const auto NormalizedROILabel = X(ROILabel);
    const long int ROINumber = 10001; // TODO: find highest existing and ++ it.
    DICOM_data.Ensure_Contour_Data_Allocated();

    //Iterate over each requested image_array. Each image is processed independently, so a thread pool is used.
    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
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
        metadata["ROIName"] = ROILabel;
        metadata["NormalizedROIName"] = NormalizedROILabel;
        metadata["MinimumSeparation"] = metadata["SliceThickness"]; // TODO: is there a routine to do this? (YES: Unique_Contour_Planes()...)
        metadata["ROINumber"] = std::to_string(ROINumber);
        metadata["Description"] = "Whole-Image Contour";

        auto cc = Encircle_Images_with_Contours(imgs, opts, metadata);

        //Construct a destination for the ROI contours.
        DICOM_data.contour_data->ccs.emplace_back();

        DICOM_data.contour_data->ccs.back().contours.splice(
             DICOM_data.contour_data->ccs.back().contours.end(),
             cc.contours);
    }

    return true;
}
