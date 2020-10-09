//ComparePixels.cc - A part of DICOMautomaton 2019. Written by hal clark.

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
#include "../YgorImages_Functors/Compute/Compare_Images.h"
#include "ComparePixels.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)



OperationDoc OpArgDocComparePixels(){
    OperationDoc out;
    out.name = "ComparePixels";
    out.desc = 
        "This operation compares images ('test' images and 'reference' images) on a per-voxel/per-pixel basis."
        " Any combination of 2D and 3D images is supported, including images which do not fully overlap, but the"
        " reference image array must be rectilinear (this property is verified).";

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
        " must be evaluated for every voxel."
    );
    out.notes.emplace_back(
        "The distance-to-agreement comparison will tend to overestimate the distance, especially"
        " when the DTA value is low, because voxel size effects will dominate the estimation."
        " Reference images should be supersampled as necessary."
    );
    out.notes.emplace_back(
        "This operation optionally makes use of interpolation for sub-voxel distance estimation."
        " However, interpolation is currently limited to be along the edges connecting nearest-"
        " and next-nearest voxel centres."
        " In other words, true volumetric interpolation is **not** available."
        " Implicit interpolation is also used (via the intermediate value theorem) for the"
        " distance-to-agreement comparison, which results in distance estimation that may"
        " vary up to the largest caliper distance of a voxel."
        " For this reason, the accuracy of all comparisons should be expected to be limited by"
        " image spatial resolution (i.e., voxel dimensions)."
        " Reference images should be supersampled as necessary."
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
    out.args.back().name = "Method";
    out.args.back().desc = "The comparison method to compute. Three options are currently available:"
                           " distance-to-agreement (DTA), discrepancy, and gamma-index."
                           " All three are fully 3D, but can also work for 2D or mixed 2D-3D comparisons."
                           " DTA is a measure of how far away the nearest voxel (in the reference images)"
                           " is with a voxel intensity sufficiently close to each voxel in the test images."
                           " This comparison ignores pixel intensities except to test if the values match"
                           " within the specified tolerance. The voxel neighbourhood is exhaustively"
                           " explored until a suitable voxel is found. Implicit interpolation is used to"
                           " detect when the value could be found via interpolation, but explicit"
                           " interpolation is not used. Thus distance might be overestimated."
                           " A discrepancy comparison measures the point intensity discrepancy without"
                           " accounting for spatial shifts."
                           " A gamma analysis combines distance-to-agreement and point differences into"
                           " a single index which is best used to test if both DTA and discrepancy criteria"
                           " are satisfied (gamma <= 1 iff both pass). It was proposed by Low et al. in 1998"
                           " ((doi:10.1118/1.598248). Gamma analyses permits trade-offs between spatial"
                           " and dosimetric discrepancies which can arise when the image arrays slightly differ"
                           " in alignment or pixel values.";
    out.args.back().default_val = "gamma-index";
    out.args.back().expected = true;
    out.args.back().examples = { "gamma-index",
                                 "DTA",
                                 "discrepancy" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The channel to compare (zero-based)."
                           " Note that both test images and reference images will share this specifier.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "0",
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
    out.args.back().name = "RefImgLowerThreshold";
    out.args.back().desc = "Pixel lower threshold for the reference images."
                           " Only voxels with values above this threshold (inclusive) will be altered.";
    out.args.back().default_val = "-inf";
    out.args.back().expected = true;
    out.args.back().examples = { "-inf",
                                 "0.0",
                                 "200" };

    out.args.emplace_back();
    out.args.back().name = "RefImgUpperThreshold";
    out.args.back().desc = "Pixel upper threshold for the reference images."
                           " Only voxels with values below this threshold (inclusive) will be altered.";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "inf",
                                 "1.23",
                                 "1000" };

    out.args.emplace_back();
    out.args.back().name = "DiscType";
    out.args.back().desc = "Parameter for all comparisons estimating the direct, voxel-to-voxel discrepancy."
                           " There are currently three types available."
                           " 'Relative' is the absolute value of the difference"
                           " of two voxel values divided by the largest of the two values."
                           " 'Difference' is the difference of two voxel values."
                           " 'PinnedToMax' is the absolute value of the"
                           " difference of two voxel values divided by the largest voxel value in the selected"
                           " images.";
    out.args.back().default_val = "relative";
    out.args.back().expected = true;
    out.args.back().examples = { "relative",
                                 "difference",
                                 "pinned-to-max" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "DTAVoxValEqAbs";
    out.args.back().desc = "Parameter for all comparisons involving a distance-to-agreement (DTA) search."
                           " The difference in voxel values considered to be sufficiently equal (absolute;"
                           " in voxel intensity units). Note: This value CAN be zero. It is meant to"
                           " help overcome noise. Note that this value is ignored by all interpolation"
                           " methods.";
    out.args.back().default_val = "1.0E-3";
    out.args.back().expected = true;
    out.args.back().examples = { "1.0E-3",
                                 "1.0E-5",
                                 "0.0",
                                 "0.5" };

    out.args.emplace_back();
    out.args.back().name = "DTAVoxValEqRelDiff";
    out.args.back().desc = "Parameter for all comparisons involving a distance-to-agreement (DTA) search."
                           " The difference in voxel values considered to be sufficiently equal (~relative"
                           " difference; in %). Note: This value CAN be zero. It is meant to help overcome"
                           " noise. Note that this value is ignored by all interpolation methods.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.1",
                                 "1.0",
                                 "10.0" };

    out.args.emplace_back();
    out.args.back().name = "DTAMax";
    out.args.back().desc = "Parameter for all comparisons involving a distance-to-agreement (DTA) search."
                           " Maximally acceptable distance-to-agreement (in DICOM units: mm) above which to"
                           " stop searching. All voxels within this distance will be searched unless a"
                           " matching voxel is found. Note that a gamma-index comparison may terminate"
                           " this search early if the gamma-index is known to be greater than one."
                           " It is recommended to make this value approximately 1 voxel width larger than"
                           " necessary in case a matching voxel can be located near the boundary."
                           " Also note that some voxels beyond the DTA_max distance may be evaluated.";
    out.args.back().default_val = "30.0";
    out.args.back().expected = true;
    out.args.back().examples = { "3.0",
                                 "5.0",
                                 "50.0" };

    out.args.emplace_back();
    out.args.back().name = "DTAInterpolationMethod";
    out.args.back().desc = "Parameter for all comparisons involving a distance-to-agreement (DTA) search."
                           " Controls how precisely and how often the space between voxel centres are interpolated to identify the exact"
                           " position of agreement. There are currently three options: no interpolation ('None'),"
                           " nearest-neighbour ('NN'), and next-nearest-neighbour ('NNN')."
                           " (1) If no interpolation is selected, the agreement position will only be established to"
                           " within approximately the reference image voxels dimensions. To avoid interpolation, voxels that straddle the"
                           " target value are taken as the agreement distance. Conceptually, if you view a voxel as having a finite spatial"
                           " extent then this method may be sufficient for distance assessment. Though it is not precise, it is fast. "
                           " This method will tend to over-estimate the actual distance, though it is possible that it slightly"
                           " under-estimates it. This method works best when the reference image grid size is small in comparison to the"
                           " desired spatial accuracy (e.g., if computing gamma, the tolerance should be much larger than the largest voxel"
                           " dimension) so supersampling is recommended."
                           " (2) Nearest-neighbour interpolation considers the line connecting directly adjacent voxels. Using linear"
                           " interpolation along this line when adjacent voxels straddle the target value, the 3D point where the target value"
                           " appears can be predicted. This method can significantly improve distance estimation accuracy, though will"
                           " typically be much slower than no interpolation. On the other hand, this method lower amounts of supersampling,"
                           " though it is most reliable when the reference image grid size is small in comparison to the desired spatial"
                           " accuracy. Note that nearest-neighbour interpolation also makes use of the 'no interpolation' methods."
                           " If you have a fine reference image, prefer either no interpolation or nearest-neighbour interpolation."
                           " (3) Finally, next-nearest-neighbour considers the diagonally-adjacent neighbours separated by taxi-cab distance of 2"
                           " (so in-plane diagonals are considered, but 3D diagonals are not). Quadratic (i.e., bi-linear) interpolation is"
                           " analytically solved to determine where along the straddling diagonal the target value appears. This method is"
                           " more expensive than linear interpolation but will generally result in more accurate distance estimates. This"
                           " method may require lower amounts of supersampling than linear interpolation, but is most reliable when the"
                           " reference image grid size is small in comparison to the desired spatial accuracy. Use of this method may not be"
                           " appropriate in all cases considering that supersampling may be needed and a quadratic equation is solved for"
                           " every voxel diagonal. Note that next-nearest-neighbour interpolation also makes use of the nearest-neighbour and"
                           " 'no interpolation' methods.";
    out.args.back().default_val = "NN";
    out.args.back().expected = true;
    out.args.back().examples = { "None",
                                 "NN",
                                 "NNN" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "GammaDTAThreshold";
    out.args.back().desc = "Parameter for gamma-index comparisons."
                           " Maximally acceptable distance-to-agreement (in DICOM units: mm). When the measured DTA"
                           " is above this value, the gamma index will necessarily be greater than one."
                           " Note this parameter can differ from the DTA_max search cut-off, but should be <= to it.";
    out.args.back().default_val = "5.0";
    out.args.back().expected = true;
    out.args.back().examples = { "3.0",
                                 "5.0",
                                 "10.0" };

    out.args.emplace_back();
    out.args.back().name = "GammaDiscThreshold";
    out.args.back().desc = "Parameter for gamma-index comparisons."
                           " Voxel value discrepancies lower than this value are considered acceptable, but values"
                           " above will result in gamma values >1. The specific interpretation of this parameter"
                           " (and the units) depend on the specific type of discrepancy used. For percentage-based"
                           " discrepancies, this parameter is interpretted as a percentage (i.e., '5.0' = '5%')."
                           " For voxel intensity measures such as the absolute difference, this value is interpretted"
                           " as an absolute threshold with the same intensity units (i.e., '5.0' = '5 HU' or similar).";
    out.args.back().default_val = "5.0";
    out.args.back().expected = true;
    out.args.back().examples = { "3.0",
                                 "5.0",
                                 "10.0" };

    out.args.emplace_back();
    out.args.back().name = "GammaTerminateAboveOne";
    out.args.back().desc = "Parameter for gamma-index comparisons."
                           " Halt spatial searching if the gamma index will necessarily indicate failure (i.e.,"
                           " gamma >1). Note this can parameter can drastically reduce the computational effort"
                           " required to compute the gamma index, but the reported gamma values will be invalid"
                           " whenever they are >1. This is often tolerable since the magnitude only matters when"
                           " it is <1. In lieu of the true gamma-index, a value slightly >1 will be assumed.";
    out.args.back().default_val = "true";
    out.args.back().expected = true;
    out.args.back().examples = { "true",
                                 "false" };

    return out;
}



Drover ComparePixels(Drover DICOM_data,
                     const OperationArgPkg& OptArgs,
                     const std::map<std::string, std::string>& /*InvocationMetadata*/,
                     const std::string& /*FilenameLex*/){


    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto ReferenceImageSelectionStr = OptArgs.getValueStr("ReferenceImageSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto MethodStr = OptArgs.getValueStr("Method").value();
    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );
    const auto TestImgLowerThreshold = std::stod( OptArgs.getValueStr("TestImgLowerThreshold").value() );
    const auto TestImgUpperThreshold = std::stod( OptArgs.getValueStr("TestImgUpperThreshold").value() );
    const auto RefImgLowerThreshold = std::stod( OptArgs.getValueStr("RefImgLowerThreshold").value() );
    const auto RefImgUpperThreshold = std::stod( OptArgs.getValueStr("RefImgUpperThreshold").value() );

    const auto DiscTypeStr = OptArgs.getValueStr("DiscType").value();

    const auto DTAVoxValEqAbs = std::stod( OptArgs.getValueStr("DTAVoxValEqAbs").value() );
    const auto DTAVoxValEqRelDiff = std::stod( OptArgs.getValueStr("DTAVoxValEqRelDiff").value() );
    const auto DTAMax = std::stod( OptArgs.getValueStr("DTAMax").value() );
    const auto DTAInterpolationMethodStr = OptArgs.getValueStr("DTAInterpolationMethod").value();

    const auto GammaDTAThreshold = std::stod( OptArgs.getValueStr("GammaDTAThreshold").value() );
    const auto GammaDiscThreshold = std::stod( OptArgs.getValueStr("GammaDiscThreshold").value() );
    const auto GammaTerminateAboveOneStr = OptArgs.getValueStr("GammaTerminateAboveOne").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = Compile_Regex("^tr?u?e?$");

    const auto method_gam = Compile_Regex("^ga?m?m?a?-?i?n?d?e?x?$");
    const auto method_dta = Compile_Regex("^dta?$");
    const auto method_dis = Compile_Regex("^dis?c?r?e?p?a?n?c?y?$");

    const auto interp_none = Compile_Regex("^non?e?$");
    const auto interp_nn   = Compile_Regex("^nn$");
    const auto interp_nnn  = Compile_Regex("^nnn$");

    const auto disctype_rel = Compile_Regex("^re?l?a?t?i?v?e?$");
    const auto disctype_dif = Compile_Regex("^di?f?f?e?r?e?n?c?e?$");
    const auto disctype_pin = Compile_Regex("^pi?n?n?e?d?-?t?o?-?m?a?x?$");

    const auto GammaTerminateAboveOne = std::regex_match(GammaTerminateAboveOneStr, regex_true);
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
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){

        ComputeCompareImagesUserData ud;

        if(std::regex_match(MethodStr, method_gam)){
            ud.comparison_method = ComputeCompareImagesUserData::ComparisonMethod::GammaIndex;
        }else if(std::regex_match(MethodStr, method_dta)){
            ud.comparison_method = ComputeCompareImagesUserData::ComparisonMethod::DTA;
        }else if(std::regex_match(MethodStr, method_dis)){
            ud.comparison_method = ComputeCompareImagesUserData::ComparisonMethod::Discrepancy;
        }else{
            throw std::invalid_argument("Method not understood. Cannot continue.");
        }

        if(std::regex_match(DiscTypeStr, disctype_rel)){
            ud.discrepancy_type = ComputeCompareImagesUserData::DiscrepancyType::Relative;
            ud.gamma_Dis_threshold = GammaDiscThreshold / 100.0;

        }else if(std::regex_match(DiscTypeStr, disctype_dif)){
            ud.discrepancy_type = ComputeCompareImagesUserData::DiscrepancyType::Difference;
            ud.gamma_Dis_threshold = GammaDiscThreshold; // Note: not a percentage! Do not need to scale!

        }else if(std::regex_match(DiscTypeStr, disctype_pin)){
            ud.discrepancy_type = ComputeCompareImagesUserData::DiscrepancyType::PinnedToMax;
            ud.gamma_Dis_threshold = GammaDiscThreshold / 100.0;

        }else{
            throw std::invalid_argument("Discrepancy type not understood. Cannot continue.");
        }

        if(std::regex_match(DTAInterpolationMethodStr, interp_none)){
            ud.interpolation_method = ComputeCompareImagesUserData::InterpolationMethod::None;

        }else if(std::regex_match(DTAInterpolationMethodStr, interp_nnn)){
            ud.interpolation_method = ComputeCompareImagesUserData::InterpolationMethod::NNN;

        }else if(std::regex_match(DTAInterpolationMethodStr, interp_nn)){
            ud.interpolation_method = ComputeCompareImagesUserData::InterpolationMethod::NN;

        }else{
            throw std::invalid_argument("Interpolation method not understood. Cannot continue.");
        }

        ud.channel = Channel;

        ud.inc_lower_threshold = TestImgLowerThreshold;
        ud.inc_upper_threshold = TestImgUpperThreshold;
        ud.ref_img_inc_lower_threshold = RefImgLowerThreshold;
        ud.ref_img_inc_upper_threshold = RefImgUpperThreshold;

        ud.DTA_vox_val_eq_abs = DTAVoxValEqAbs;
        ud.DTA_vox_val_eq_reldiff = DTAVoxValEqRelDiff / 100.0;
        ud.DTA_max = DTAMax;

        ud.gamma_DTA_threshold = GammaDTAThreshold;

        ud.gamma_terminate_when_max_exceeded = GammaTerminateAboveOne;
        //ud.gamma_terminated_early = std::nextafter(1.0, std::numeric_limits<double>::infinity());

        if(!(*iap_it)->imagecoll.Compute_Images( ComputeCompareImages, 
                                                 RIARL, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to compare images.");
        }


        if(std::regex_match(MethodStr, method_gam)){
            FUNCINFO("Passing rate: " 
                     << ud.passed
                     << " out of " 
                     << ud.count 
                     << " = " 
                     << 100.0 * ud.passed / ud.count 
                     << " %");
        }
    }

    return DICOM_data;
}
