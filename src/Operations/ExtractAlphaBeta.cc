//ExtractAlphaBeta.cc - A part of DICOMautomaton 2019. Written by hal clark.

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
#include "../BED_Conversion.h"
#include "../YgorImages_Functors/Compute/Joint_Pixel_Sampler.h"
#include "ExtractAlphaBeta.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)



OperationDoc OpArgDocExtractAlphaBeta(){
    OperationDoc out;
    out.name = "ExtractAlphaBeta";
    out.desc = 
        "This operation compares two images arrays: either a biologically-equivalent dose"
        " ($BED_{\\alpha/\\beta}$) transformed array"
        " or an equivalent dose in $d$ dose per fraction ($EQD_{d}$) array and a 'reference' untransformed array."
        " The $\\alpha/\\beta$ used for each voxel are extracted by comparing corresponding voxels."
        " Each voxel is overwritten with the value of $\\alpha/\\beta$ needed to accomplish the given transform."
        " This routine is best used to inspect a given transformation (e.g., for QA purposes).";

    out.notes.emplace_back(
        "Images are overwritten, but ReferenceImages are not."
        " Multiple Images may be specified, but only one ReferenceImages may be specified."
    );
    out.notes.emplace_back(
        "The reference image array must be rectilinear. (This is a requirement specific to this"
        " implementation, a less restrictive implementation could overcome the issue.)"
    );
    out.notes.emplace_back(
        "For the fastest and most accurate results, test and reference image arrays should spatially align."
        " However, alignment is **not** necessary. If test and reference image arrays are aligned,"
        " image adjacency can be precomputed and the analysis will be faster. If not, image adjacency"
        " must be evaluated for each image slice. If this also fails, it will be evaluated for every voxel."
    );
    out.notes.emplace_back(
        "This operation will make use of interpolation if corresponding voxels do not exactly overlap."
    );


    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "TransformedImageSelection";
    out.args.back().default_val = "first";
    out.args.back().desc = "The transformed image array where voxel intensities represent BED or EQDd. "
                         + out.args.back().desc;

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ReferenceImageSelection";
    out.args.back().default_val = "last";
    out.args.back().desc = "The un-transformed image array where voxel intensities represent (non-BED) dose. "
                         + out.args.back().desc;

    out.args.emplace_back();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                           " all available ROIs. Be aware that input spaces are trimmed to a single space."
                           " If your ROI name has more than two sequential spaces, use regex to avoid them."
                           " All ROIs have to match the single regex, so use the 'or' token if needed."
                           " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                                 R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                                 R"***(Left Parotid|Right Parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "ROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                           " all available ROIs. Be aware that input spaces are trimmed to a single space."
                           " If your ROI name has more than two sequential spaces, use regex to avoid them."
                           " All ROIs have to match the single regex, so use the 'or' token if needed."
                           " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                                 R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                                 R"***(left_parotid|right_parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "Model";
    out.args.back().desc = "The model of BED or EQD transformation to assume."
                           " Currently, only 'simple-lq-eqd' is available."
                           " The 'simple-lq-eqd' model does not take into account elapsed time or any cell repopulation effects.";
    out.args.back().default_val = "simple-lq-eqd";
    out.args.back().expected = true;
    out.args.back().examples = { "simple-lq-eqd" };

    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The channel to compare (zero-based)."
                           " Setting to -1 will compare each channel separately."
                           " Note that both test images and reference images must share this specifier.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "-1",
                                 "0",
                                 "1",
                                 "2" };

    out.args.emplace_back();
    out.args.back().name = "TestImgLowerThreshold";
    out.args.back().desc = "Pixel lower threshold for the test images."
                           " Only voxels with values above this threshold (inclusive) will be altered.";
    out.args.back().default_val = "-inf";
    out.args.back().expected = true;
    out.args.back().examples = { "-inf",
                                 "0.0",
                                 "200" };

    out.args.emplace_back();
    out.args.back().name = "TestImgUpperThreshold";
    out.args.back().desc = "Pixel upper threshold for the test images."
                           " Only voxels with values below this threshold (inclusive) will be altered.";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "inf",
                                 "1.23",
                                 "1000" };

    out.args.emplace_back();
    out.args.back().name = "NumberOfFractions";
    out.args.back().desc = "Number of fractions assumed in the BED or EQDd transformation.";
    out.args.back().default_val = "35";
    out.args.back().expected = true;
    out.args.back().examples = { "1",
                                 "5",
                                 "35" };

    out.args.emplace_back();
    out.args.back().name = "NominalDosePerFraction";
    out.args.back().desc = "The nominal dose per fraction (in DICOM units; Gy) assumed by an EQDd transformation."
                           " This parameter is the 'd' in 'EQDd';"
                           " for EQD2 transformations, this parameter must be 2 Gy.";
    out.args.back().default_val = "2.0";
    out.args.back().expected = true;
    out.args.back().examples = { "1.8",
                                 "2.0",
                                 "8.0" };

    return out;
}



Drover ExtractAlphaBeta(Drover DICOM_data, 
                           const OperationArgPkg& OptArgs, 
                           const std::map<std::string,std::string>& /*InvocationMetadata*/, 
                           const std::string& /*FilenameLex*/ ){


    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TransformedImageSelectionStr = OptArgs.getValueStr("TransformedImageSelection").value();
    const auto ReferenceImageSelectionStr = OptArgs.getValueStr("ReferenceImageSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto ModelStr = OptArgs.getValueStr("Model").value();
    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );
    const auto TestImgLowerThreshold = std::stod( OptArgs.getValueStr("TestImgLowerThreshold").value() );
    const auto TestImgUpperThreshold = std::stod( OptArgs.getValueStr("TestImgUpperThreshold").value() );

    const auto NumberOfFractions = std::stof( OptArgs.getValueStr("NumberOfFractions").value() );
    const auto NominalDosePerFraction = std::stof( OptArgs.getValueStr("NominalDosePerFraction").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto model_simple_lq = Compile_Regex("^si?m?p?l?e?[-_]?l?q?[-_]?e?q?d?2?$");

    //-----------------------------------------------------------------------------------------------------------------

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    auto RIAs_all = All_IAs( DICOM_data );
    auto RIAs = Whitelist( RIAs_all, ReferenceImageSelectionStr );
    if(RIAs.size() != 1){
        throw std::invalid_argument("Only one reference image collection can be specified.");
    }
    std::list<std::reference_wrapper<planar_image_collection<float, double>>> RIARL = { std::ref( (*( RIAs.front() ))->imagecoll ) };


    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, TransformedImageSelectionStr );
    for(auto & iap_it : IAs){

        ComputeJointPixelSamplerUserData ud;
        ud.sampling_method = ComputeJointPixelSamplerUserData::SamplingMethod::LinearInterpolation;

        ud.channel = Channel;
        ud.description = "Extracted alpha/beta";

        ud.inc_lower_threshold = TestImgLowerThreshold;
        ud.inc_upper_threshold = TestImgUpperThreshold;

        if(std::regex_match(ModelStr, model_simple_lq)){
            ud.f_reduce = [NumberOfFractions,
                           NominalDosePerFraction]( std::vector<float> &vals, 
                                                    vec3<double>                ) -> float {
                const auto D_BED  = vals.at(0); // Transformed dose.
                const auto D_orig = vals.at(1); // Original dose.

                // Rearrange the EQDd relation to extract alpha/beta.
                // EQD2 = D * (D/n + abr)/(2d + abr)
                //
                // EQD2 * 2d + EQD2 * abr = D * (D/n + abr)
                //
                // abr * (EQD2 - D) = D * D/n - EQD2 * 2d

                const auto numer = (D_orig * D_orig / NumberOfFractions) - D_BED * NominalDosePerFraction;
                const auto denom = D_BED - D_orig;
                const auto abr = numer / denom;
                return abr;
            };
        }else{
            throw std::invalid_argument("Model not understood. Cannot continue.");
        }

        if(!(*iap_it)->imagecoll.Compute_Images( ComputeJointPixelSampler, 
                                                 RIARL, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to sample images.");
        }
    }

    return DICOM_data;
}
