//ResampleImages.cc - A part of DICOMautomaton 2023. Written by hal clark.

#include <optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>
#include <vector>

#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorImages.h"
#include "YgorString.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Metadata.h"
#include "../YgorImages_Functors/Compute/Joint_Pixel_Sampler.h"

#include "ResampleImages.h"



OperationDoc OpArgDocResampleImages(){
    OperationDoc out;
    out.name = "ResampleImages";

    out.tags.emplace_back("category: image processing");
    out.tags.emplace_back("category: contour processing");

    out.desc = 
        "This operation combines two image arrays. The voxel values from one array are resampled onto the"
        " geometry of the other. This routine is used to ensure two image arrays have consistent spatial"
        " characteristics (e.g., number of images, rows, columns, spatial extent, orientations, etc.),"
        " which can simplify and accelerate other operations.";

    out.notes.emplace_back(
        "No images are overwritten. A resampled image array is created."
    );
    out.notes.emplace_back(
        "The resampling con be confined using a region of interest (via a contour collection) or using"
        " intensity thresholds. Note that both of these are applied to the *reference* image array"
        " (i.e., the image array that provides the reference geometry)."
    );
    out.notes.emplace_back(
        "The image array providing voxel values must be rectilinear. (This is a requirement specific to this"
        " implementation, a less restrictive implementation could overcome the issue.)"
    );
    out.notes.emplace_back(
        "This operation will make use of trlinear interpolation if corresponding voxels do not exactly overlap."
    );


    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "first";
    out.args.back().desc = "The image array from which voxel values will be borrowed. "
                           "These voxel values are what is being resampled. "
                         + out.args.back().desc;

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ReferenceImageSelection";
    out.args.back().default_val = "last";
    out.args.back().desc = "The image array from which geometry will be borrowed. "
                           "This image array provides the reference geometry. "
                         + out.args.back().desc;

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
    out.args.back().name = "Channel";
    out.args.back().desc = "The channel to compare (zero-based)."
                           " Setting to -1 will compare each channel separately."
                           " Note that both image arrays must share this specifier.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "-1",
                                 "0",
                                 "1",
                                 "2" };

    out.args.emplace_back();
    out.args.back().name = "Lower";
    out.args.back().desc = "Voxel intensity filter lower threshold."
                           " Only voxels with values above this threshold (inclusive) will be altered.";
    out.args.back().default_val = "-inf";
    out.args.back().expected = true;
    out.args.back().examples = { "-inf",
                                 "0.0",
                                 "200" };

    out.args.emplace_back();
    out.args.back().name = "Upper";
    out.args.back().desc = "Voxel intensity filter upper threshold."
                           " Only voxels with values below this threshold (inclusive) will be altered.";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "inf",
                                 "1.23",
                                 "1000" };

    out.args.emplace_back();
    out.args.back().name = "IncludeNaN";
    out.args.back().desc = "Voxel intensity filter for non-finite values (i.e., NaNs)."
                           " This setting controls whether voxels with NaN intensity be altered.";
    out.args.back().default_val = "true";
    out.args.back().expected = true;
    out.args.back().examples = { "true",
                                 "false" };

    out.args.emplace_back();
    out.args.back().name = "InaccessibleValue";
    out.args.back().desc = "The voxel value to use as a fallback when a voxel cannot be reached.";
    out.args.back().default_val = "nan";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0",
                                 "1.0",
                                 "nan",
                                 "-inf" };

    return out;
}



bool ResampleImages(Drover &DICOM_data,
                    const OperationArgPkg& OptArgs,
                    std::map<std::string, std::string>& /*InvocationMetadata*/,
                    const std::string& /*FilenameLex*/){


    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto ReferenceImageSelectionStr = OptArgs.getValueStr("ReferenceImageSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();

    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );
    const auto ImgLowerThreshold = std::stod( OptArgs.getValueStr("Lower").value() );
    const auto ImgUpperThreshold = std::stod( OptArgs.getValueStr("Upper").value() );
    const auto IncludeNaNStr = OptArgs.getValueStr("IncludeNaN").value();
    const auto InaccessibleValue = std::stod( OptArgs.getValueStr("InaccessibleValue").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = Compile_Regex("^tr?u?e?$");
    const auto IncludeNaN = std::regex_match(IncludeNaNStr, regex_true);

    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }


    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    if(IAs.size() != 1){
        throw std::invalid_argument("Only one transformed image array can be specified.");
    }
    std::list<std::reference_wrapper<planar_image_collection<float, double>>> IARL = { std::ref( (*( IAs.front() ))->imagecoll ) };
    const auto cm = (*( IAs.front() ))->imagecoll.get_common_metadata({});


    auto RIAs_all = All_IAs( DICOM_data );
    auto RIAs = Whitelist( RIAs_all, ReferenceImageSelectionStr );
    if(RIAs.size() != 1){
        throw std::invalid_argument("Only one reference image array can be specified.");
    }
    auto ia_ptr = std::make_shared<Image_Array>();
    ia_ptr->imagecoll = (*( RIAs.front() ))->imagecoll;


    ComputeJointPixelSamplerUserData ud;
    ud.sampling_method = ComputeJointPixelSamplerUserData::SamplingMethod::LinearInterpolation;
    ud.channel = Channel;
    ud.description = "Resampled";
    ud.inc_lower_threshold = ImgLowerThreshold;
    ud.inc_upper_threshold = ImgUpperThreshold;
    ud.inc_nan = IncludeNaN;
    ud.inaccessible_val = InaccessibleValue;

    ud.f_reduce = []( std::vector<float> &vals, 
                      vec3<double>                ) -> float {
        //const auto val = vals.at(0); // Voxel intensity from geometry-providing array. Not needed.
        const auto val = vals.at(1); // Voxel intensity from intensity-providing array.
        return val;
    };

    if(!ia_ptr->imagecoll.Compute_Images( ComputeJointPixelSampler, 
                                          IARL, cc_ROIs, &ud )){
        throw std::runtime_error("Unable to resample images.");
    }

    for(auto &animg : ia_ptr->imagecoll.images){
        animg.metadata = cm;
        // TODO: some metadata likely needs to be changed here, e.g., SliceThickness, PixelSpacing, etc.
    }

    DICOM_data.image_data.emplace_back(ia_ptr);
    return true;
}
