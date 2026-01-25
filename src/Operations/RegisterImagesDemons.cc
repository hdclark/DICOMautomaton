//RegisterImagesDemons.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>
#include <vector>
#include <cstdint>

#include "Explicator.h"

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorStats.h"
#include "YgorString.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "../Alignment_Rigid.h"
#include "../Alignment_Field.h"
#include "../Alignment_Demons.h"

#include "RegisterImagesDemons.h"


OperationDoc OpArgDocRegisterImagesDemons(){
    OperationDoc out;
    out.name = "RegisterImagesDemons";

    out.tags.emplace_back("category: image processing");
    out.tags.emplace_back("category: spatial transform processing");
    out.tags.emplace_back("category: image registration");

    out.desc = 
        "This operation uses two image arrays (one 'moving' and the other 'stationary' or 'fixed') to perform"
        " deformable image registration using the demons algorithm."
        " The demons algorithm is an intensity-based registration method that iteratively computes"
        " a deformation field to align a moving image to a fixed (stationary) image."
        " The resulting transformation can later be applied to warp other objects.";
        
    out.notes.emplace_back(
        "The 'moving' image is *not* warped by this operation -- this operation merely identifies a suitable"
        " transformation. Separation of the identification and application of a warp allows the warp to more easily"
        " be re-used and applied to multiple objects."
    );
    out.notes.emplace_back(
        "The output of this operation is a deformation field transformation that can later be applied to"
        " images, point clouds, surface meshes, and contours."
    );
    out.notes.emplace_back(
        "This operation handles images that are not aligned or have different orientations"
        " by first resampling the moving image onto the fixed image's grid."
    );
    out.notes.emplace_back(
        "The demons algorithm works best when images have similar intensity distributions."
        " If images are from different modalities or scanners, consider using histogram matching."
    );
    out.notes.emplace_back(
        "The diffeomorphic demons variant uses an exponential update scheme to ensure the transformation"
        " is invertible (diffeomorphic). This is generally preferred for medical imaging applications"
        " where invertibility is important."
    );
    out.notes.emplace_back(
        "Registration quality is highly dependent on parameter selection."
        " The smoothing parameters control regularization and determine how 'smooth' the deformation will be."
        " Larger sigma values produce smoother deformations but may miss fine details."
        " Smaller sigma values allow more detailed deformations but may be more susceptible to noise."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "MovingImageSelection";
    out.args.back().default_val = "last";
    out.args.back().desc = "The image array that will be registered to the fixed image. "
                           "This image will be resampled onto the fixed image's grid before registration begins. "
                         + out.args.back().desc;

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "FixedImageSelection";
    out.args.back().default_val = "first";
    out.args.back().desc = "The stationary (fixed) image array to use as a reference for registration. "
                         + out.args.back().desc
                         + " Note that this image is not modified.";

    out.args.emplace_back();
    out.args.back().name = "MaxIterations";
    out.args.back().desc = "The maximum number of iterations to perform."
                           " Registration will stop early if convergence is achieved."
                           " More iterations allow for more detailed registration but take longer to compute.";
    out.args.back().default_val = "100";
    out.args.back().expected = true;
    out.args.back().examples = { "50", "100", "200", "500" };

    out.args.emplace_back();
    out.args.back().name = "ConvergenceThreshold";
    out.args.back().desc = "The convergence threshold for the mean squared error."
                           " Registration stops when the change in MSE between iterations is below this value."
                           " Smaller values require tighter convergence but may take more iterations.";
    out.args.back().default_val = "0.001";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0001", "0.001", "0.01" };

    out.args.emplace_back();
    out.args.back().name = "DeformationFieldSmoothingSigma";
    out.args.back().desc = "The standard deviation (in DICOM units, mm) of the Gaussian kernel used to smooth"
                           " the deformation field. This controls regularization and ensures smooth deformations."
                           " Larger values produce smoother, more regular deformations."
                           " A value of 0.0 disables smoothing.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0", "0.5", "1.0", "2.0", "5.0" };

    out.args.emplace_back();
    out.args.back().name = "UpdateFieldSmoothingSigma";
    out.args.back().desc = "The standard deviation (in DICOM units, mm) of the Gaussian kernel used to smooth"
                           " the update field. This is primarily used in diffeomorphic demons."
                           " A value of 0.0 disables smoothing.";
    out.args.back().default_val = "0.5";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0", "0.25", "0.5", "1.0" };

    out.args.emplace_back();
    out.args.back().name = "UseDiffeomorphic";
    out.args.back().desc = "Whether to use the diffeomorphic demons variant."
                           " If true, uses an exponential update scheme that ensures diffeomorphic (invertible) transformations."
                           " This is generally preferred for medical imaging applications.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "UseHistogramMatching";
    out.args.back().desc = "Whether to apply histogram matching to the moving image before registration."
                           " This can help when images have different intensity distributions"
                           " (e.g., different scanners, protocols, or modalities)."
                           " Histogram matching maps the intensity distribution of the moving image"
                           " to match the fixed image.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "HistogramBins";
    out.args.back().desc = "The number of histogram bins to use for histogram matching."
                           " More bins provide finer intensity mapping but may be more susceptible to noise.";
    out.args.back().default_val = "256";
    out.args.back().expected = true;
    out.args.back().examples = { "64", "128", "256", "512" };

    out.args.emplace_back();
    out.args.back().name = "HistogramOutlierFraction";
    out.args.back().desc = "The fraction of intensity values to exclude when determining histogram bounds."
                           " This helps handle outliers. For example, 0.01 means use the range from"
                           " 1st to 99th percentile, excluding the most extreme 1% of values on each end.";
    out.args.back().default_val = "0.01";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0", "0.01", "0.05" };

    out.args.emplace_back();
    out.args.back().name = "NormalizationFactor";
    out.args.back().desc = "Normalization factor for the demons force (gradient magnitude)."
                           " This controls the step size and affects convergence speed and stability."
                           " Larger values make the registration more conservative (smaller steps)."
                           " Smaller values allow larger updates but may be less stable.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.5", "1.0", "2.0", "5.0" };

    out.args.emplace_back();
    out.args.back().name = "MaxUpdateMagnitude";
    out.args.back().desc = "Maximum update magnitude per iteration (in DICOM units, mm)."
                           " This prevents large, unstable updates that could cause the registration to diverge."
                           " The value should be on the order of a few voxel widths.";
    out.args.back().default_val = "2.0";
    out.args.back().expected = true;
    out.args.back().examples = { "1.0", "2.0", "5.0", "10.0" };

    out.args.emplace_back();
    out.args.back().name = "Verbosity";
    out.args.back().desc = "Verbosity level for logging intermediate results."
                           " 0 = minimal output, 1 = normal output, 2 = detailed output.";
    out.args.back().default_val = "1";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "1", "2" };

    out.args.emplace_back();
    out.args.back().name = "TransformName";
    out.args.back().desc = "A name or label to attach to the resulting transformation.";
    out.args.back().default_val = "demons_registration";
    out.args.back().expected = true;
    out.args.back().examples = { "demons_registration", "deformable_registration", "nonrigid_warp" };

    out.args.emplace_back();
    out.args.back().name = "Metadata";
    out.args.back().desc = "A semicolon-separated list of 'key@value' metadata to imbue into the transform."
                           " This metadata will be attached to the resulting deformation field.";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "keyA@valueA;keyB@valueB" };

    return out;
}


bool RegisterImagesDemons(Drover &DICOM_data,
                          const OperationArgPkg& OptArgs,
                          std::map<std::string, std::string>& /*InvocationMetadata*/,
                          const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MovingImageSelectionStr = OptArgs.getValueStr("MovingImageSelection").value();
    const auto FixedImageSelectionStr = OptArgs.getValueStr("FixedImageSelection").value();
    const auto MaxIterations = std::stol(OptArgs.getValueStr("MaxIterations").value());
    const auto ConvergenceThreshold = std::stod(OptArgs.getValueStr("ConvergenceThreshold").value());
    const auto DeformationFieldSmoothingSigma = std::stod(OptArgs.getValueStr("DeformationFieldSmoothingSigma").value());
    const auto UpdateFieldSmoothingSigma = std::stod(OptArgs.getValueStr("UpdateFieldSmoothingSigma").value());
    const auto UseDiffeomorphicStr = OptArgs.getValueStr("UseDiffeomorphic").value();
    const auto UseHistogramMatchingStr = OptArgs.getValueStr("UseHistogramMatching").value();
    const auto HistogramBins = std::stol(OptArgs.getValueStr("HistogramBins").value());
    const auto HistogramOutlierFraction = std::stod(OptArgs.getValueStr("HistogramOutlierFraction").value());
    const auto NormalizationFactor = std::stod(OptArgs.getValueStr("NormalizationFactor").value());
    const auto MaxUpdateMagnitude = std::stod(OptArgs.getValueStr("MaxUpdateMagnitude").value());
    const auto Verbosity = std::stol(OptArgs.getValueStr("Verbosity").value());
    const auto TransformName = OptArgs.getValueStr("TransformName").value();
    const auto MetadataOpt = OptArgs.getValueStr("Metadata");

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = Compile_Regex("^tr?u?e?$");
    const auto regex_false = Compile_Regex("^fa?l?s?e?$");

    const bool UseDiffeomorphic = std::regex_match(UseDiffeomorphicStr, regex_true);
    const bool UseHistogramMatching = std::regex_match(UseHistogramMatchingStr, regex_true);

    // Parse user-provided metadata.
    std::map<std::string, std::string> Metadata;
    if(MetadataOpt){
        // First, check if this is a multi-key@value statement.
        const auto regex_split = Compile_Regex("^.*;.*$");
        const auto ismulti = std::regex_match(MetadataOpt.value(), regex_split);

        std::vector<std::string> v_kvs;
        if(!ismulti){
            v_kvs.emplace_back(MetadataOpt.value());
        }else{
            v_kvs = SplitStringToVector(MetadataOpt.value(), ';', 'd');
            if(v_kvs.size() < 1){
                throw std::logic_error("Unable to separate multiple key@value tokens");
            }
        }

        // Now attempt to parse each key@value statement.
        for(auto & k_at_v : v_kvs){
            const auto regex_split = Compile_Regex("^.*@.*$");
            const auto iskv = std::regex_match(k_at_v, regex_split);
            if(!iskv){
                throw std::logic_error("Unable to parse key@value token: '"_s + k_at_v + "'. Refusing to continue.");
            }
            
            auto v_k_v = SplitStringToVector(k_at_v, '@', 'd');
            if(v_k_v.size() <= 1) throw std::logic_error("Unable to separate key@value specifier");
            if(v_k_v.size() != 2) break; // Not a key@value statement (hint: maybe multiple @'s present?).
            Metadata[v_k_v.front()] = v_k_v.back();
        }
    }

    // Get the image arrays.
    auto IAs_all = All_IAs(DICOM_data);
    
    auto IAs_moving = Whitelist(IAs_all, MovingImageSelectionStr);
    if(IAs_moving.size() != 1){
        throw std::invalid_argument("Exactly one moving image array must be selected. "_s
                                  + "Found " + std::to_string(IAs_moving.size()) + ".");
    }
    
    auto IAs_fixed = Whitelist(IAs_all, FixedImageSelectionStr);
    if(IAs_fixed.size() != 1){
        throw std::invalid_argument("Exactly one fixed image array must be selected. "_s
                                  + "Found " + std::to_string(IAs_fixed.size()) + ".");
    }

    const auto moving_img_arr = (*IAs_moving.front())->imagecoll;
    const auto fixed_img_arr = (*IAs_fixed.front())->imagecoll;

    if(moving_img_arr.images.empty()){
        throw std::invalid_argument("Moving image array is empty. Cannot continue.");
    }
    if(fixed_img_arr.images.empty()){
        throw std::invalid_argument("Fixed image array is empty. Cannot continue.");
    }

    // Set up parameters.
    AlignViaDemonsParams params;
    params.max_iterations = MaxIterations;
    params.convergence_threshold = ConvergenceThreshold;
    params.deformation_field_smoothing_sigma = DeformationFieldSmoothingSigma;
    params.update_field_smoothing_sigma = UpdateFieldSmoothingSigma;
    params.use_diffeomorphic = UseDiffeomorphic;
    params.use_histogram_matching = UseHistogramMatching;
    params.histogram_bins = HistogramBins;
    params.histogram_outlier_fraction = HistogramOutlierFraction;
    params.normalization_factor = NormalizationFactor;
    params.max_update_magnitude = MaxUpdateMagnitude;
    params.verbosity = Verbosity;

    // Perform the registration.
    YLOGINFO("Starting demons registration");
    auto deform_field_opt = AlignViaDemons(params, moving_img_arr, fixed_img_arr);

    if(!deform_field_opt){
        throw std::runtime_error("Demons registration failed");
    }

    YLOGINFO("Demons registration completed successfully");

    // Package the deformation field into a Transform3 object.
    auto transform = std::make_shared<Transform3>();
    transform->name = TransformName;
    transform->metadata = Metadata;
    
    // Store the deformation field
    transform->transform = deform_field_opt.value();

    // Add metadata about the registration
    transform->metadata["RegistrationMethod"] = "Demons";
    transform->metadata["UseDiffeomorphic"] = UseDiffeomorphic ? "true" : "false";
    transform->metadata["UseHistogramMatching"] = UseHistogramMatching ? "true" : "false";
    transform->metadata["MaxIterations"] = std::to_string(MaxIterations);
    transform->metadata["DeformationFieldSmoothingSigma"] = std::to_string(DeformationFieldSmoothingSigma);

    // Add to the Drover.
    DICOM_data.trans_data.emplace_back(transform);

    YLOGINFO("Deformation field added to transformation collection");

    return true;
}

