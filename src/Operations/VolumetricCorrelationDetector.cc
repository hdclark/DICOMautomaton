//VolumetricCorrelationDetector.cc - A part of DICOMautomaton 2019. Written by hal clark.

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
#include "../YgorImages_Functors/Compute/Volumetric_Correlation_Detector.h"

#include "VolumetricCorrelationDetector.h"


OperationDoc OpArgDocVolumetricCorrelationDetector(){
    OperationDoc out;
    out.name = "VolumetricCorrelationDetector";
    out.tags.emplace_back("category: image processing");

    out.desc = 
        "This operation can assess 3D correlations by sampling the neighbourhood surrounding each voxel"
        " and assigning a similarity score. This routine is useful for detecting repetitive (regular)"
        " patterns that are known in advance.";

    out.notes.emplace_back(
        "The provided image collection must be rectilinear."
    );
    out.notes.emplace_back(
        "At the moment this routine can only be modified via recompilation."
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
    out.args.back() = CCWhitelistOpArgDoc();
    out.args.back().name = "ROISelection";
    out.args.back().default_val = "all";


    out.args.emplace_back();
    out.args.back().name = "Low";
    out.args.back().desc = "The low percentile.";
    out.args.back().default_val = "0.05";
    out.args.back().expected = true;
    out.args.back().examples = { "0.05",
                                 "0.5",
                                 "0.99" };


    out.args.emplace_back();
    out.args.back().name = "High";
    out.args.back().desc = "The high percentile.";
    out.args.back().default_val = "0.95";
    out.args.back().expected = true;
    out.args.back().examples = { "0.95",
                                 "0.5",
                                 "0.05" };


    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The channel to operated on (zero-based)."
                           " Negative values will cause all channels to be operated on.";
    out.args.back().default_val = "-1";
    out.args.back().expected = true;
    out.args.back().examples = { "-1",
                                 "0",
                                 "1" };


    return out;
}

bool VolumetricCorrelationDetector(Drover &DICOM_data,
                                     const OperationArgPkg& OptArgs,
                                     std::map<std::string, std::string>& /*InvocationMetadata*/,
                                     const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();

    const auto Low = std::stod( OptArgs.getValueStr("Low").value() );
    const auto High = std::stod( OptArgs.getValueStr("High").value() );

    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_gauss = Compile_Regex("^ga?u?s?s?i?a?n?$");

    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){

        // Planar derivatives.
        ComputeVolumetricCorrelationDetectorUserData ud;
        ud.channel = Channel;
        ud.low = Low;
        ud.high = High;

        if(!(*iap_it)->imagecoll.Compute_Images( ComputeVolumetricCorrelationDetector,
                                                 {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to detect volumetric correlations.");
        }
    }

    return true;
}

