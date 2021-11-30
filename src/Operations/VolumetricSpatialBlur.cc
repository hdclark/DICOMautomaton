//VolumetricSpatialBlur.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Compute/Volumetric_Spatial_Blur.h"

#include "VolumetricSpatialBlur.h"


OperationDoc OpArgDocVolumetricSpatialBlur(){
    OperationDoc out;
    out.name = "VolumetricSpatialBlur";

    out.desc = 
        "This operation performs blurring of voxel values within 3D rectilinear image arrays.";

    out.notes.emplace_back(
        "The provided image collection must be rectilinear."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
    

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";


    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";


    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The channel to operated on (zero-based)."
                           " Negative values will cause all channels to be operated on.";
    out.args.back().default_val = "-1";
    out.args.back().expected = true;
    out.args.back().examples = { "-1",
                                 "0",
                                 "1" };


    out.args.emplace_back();
    out.args.back().name = "Estimator";
    out.args.back().desc = "Controls which type of blur is computed."
                           " Currently, 'Gaussian' refers to a fixed sigma=1 (in pixel coordinates, not DICOM units)"
                           " Gaussian blur that extends for 3*sigma thus providing a 7x7x7 window."
                           " Note that applying this kernel N times will approximate a Gaussian with sigma=N."
                           " Also note that boundary voxels will cause accessible voxels within the same window to be more"
                           " heavily weighted. Try avoid boundaries or add extra margins if possible.";
    out.args.back().default_val = "Gaussian";
    out.args.back().expected = true;
    out.args.back().examples = { "Gaussian" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}

bool VolumetricSpatialBlur(Drover &DICOM_data,
                             const OperationArgPkg& OptArgs,
                             std::map<std::string, std::string>& /*InvocationMetadata*/,
                             const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    const auto EstimatorStr = OptArgs.getValueStr("Estimator").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_gauss = Compile_Regex("^ga?u?s?s?i?a?n?$");

    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){

        // Planar derivatives.
        ComputeVolumetricSpatialBlurUserData ud;
        ud.channel = Channel;
        if(std::regex_match(EstimatorStr, regex_gauss)){
            ud.estimator = VolumetricSpatialBlurEstimator::Gaussian;
        }else{
            throw std::invalid_argument("Estimator not understood. Refusing to continue.");
        }

        if(!(*iap_it)->imagecoll.Compute_Images( ComputeVolumetricSpatialBlur,
                                                 {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to compute volumetric blur.");
        }
    }

    return true;
}

