//InterpolateSlices.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Compute/Interpolate_Image_Slices.h"
#include "InterpolateSlices.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)



OperationDoc OpArgDocInterpolateSlices(){
    OperationDoc out;
    out.name = "InterpolateSlices";
    out.desc = 
        "This operation interpolates the slices of an image array using a reference image array, effectively"
        " performing trilinear interpolation."
        " This operation is meant to prepare image arrays to be compared or operated on in a per-voxel manner.";

    out.notes.emplace_back(
        "No images are overwritten by this operation."
        " The outgoing images will inherit (interpolated) voxel values from the selected images and image"
        " geometry from the reference images."
    );
    out.notes.emplace_back(
        "If all images (selected and reference, altogether) are detected to be rectilinear, this operation will"
        " avoid in-plane interpolation and will thus be much faster."
        " There is no **need** for rectilinearity, however without it sections of the image that cannot"
        " reasonably be interpolated (via plane-orthogonal projection onto the reference images) will be"
        " invalid and marked with NaNs. Non-rectilearity which amounts to a differing number of rows"
        " or columns will merely be slower to interpolate."
    );


    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "all";

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ReferenceImageSelection";
    out.args.back().default_val = "all";

    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The channel to compare (zero-based)."
                           " A negative value will result in all channels being interpolated, otherwise"
                           " unspecified channels are merely default initialized."
                           " Note that both test images and reference images will share this specifier.";
    out.args.back().default_val = "-1";
    out.args.back().expected = true;
    out.args.back().examples = { "-1",
                                 "0",
                                 "1",
                                 "2" };

    return out;
}



Drover InterpolateSlices(Drover DICOM_data, 
                           const OperationArgPkg& OptArgs, 
                           const std::map<std::string,std::string>& /*InvocationMetadata*/, 
                           const std::string& /*FilenameLex*/ ){


    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto ReferenceImageSelectionStr = OptArgs.getValueStr("ReferenceImageSelection").value();

    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    //-----------------------------------------------------------------------------------------------------------------

    auto RIAs_all = All_IAs( DICOM_data );
    auto RIAs = Whitelist( RIAs_all, ReferenceImageSelectionStr );
    if(RIAs.size() != 1){
        throw std::invalid_argument("Only one reference image collection can be specified.");
    }

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        const auto common_metadata = (*iap_it)->imagecoll.get_common_metadata({});

        ComputeInterpolateImageSlicesUserData ud;
        ud.channel = Channel;

        std::list<std::reference_wrapper<planar_image_collection<float, double>>> IARL = { std::ref( (*iap_it)->imagecoll ) };

        auto edit_imagecoll = (*( RIAs.front() ))->imagecoll;
        if(!edit_imagecoll.Compute_Images( ComputeInterpolateImageSlices, 
                                           IARL, {}, &ud )){
            throw std::runtime_error("Unable to interpolate image slices.");
        }

        for(auto &img : edit_imagecoll.images){
            img.metadata = common_metadata;
        }

        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>() );
        DICOM_data.image_data.back()->imagecoll.images.splice(
            DICOM_data.image_data.back()->imagecoll.images.end(),
            edit_imagecoll.images );
    }

    return DICOM_data;
}
