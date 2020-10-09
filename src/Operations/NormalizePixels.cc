//NormalizePixels.cc - A part of DICOMautomaton 2019. Written by hal clark.

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
#include "YgorStats.h"       //Needed for Stats:: namespace.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/ConvenienceRoutines.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Compute/Volumetric_Neighbourhood_Sampler.h"
#include "../YgorImages_Functors/Processing/Partitioned_Image_Voxel_Visitor_Mutator.h"

#include "NormalizePixels.h"

OperationDoc OpArgDocNormalizePixels(){
    OperationDoc out;
    out.name = "NormalizePixels";

    out.desc = 
        "This routine normalizes voxel intensities by adjusting them so they satisfy a 'normalization'"
        " criteria. This operation is useful as a pre-processing step when performing convolution or"
        " thresholding with absolute magnitudes.";
    
    out.notes.emplace_back(
         "This operation considers entire image arrays, not just single images."
    );
    out.notes.emplace_back(
         "This operation does not *reduce* voxels (i.e., the neighbourhood surrounding is voxel is"
         " ignored). This operation effectively applies a linear mapping to every scalar voxel value"
         " independently. Neighbourhood-based reductions are implemented in another operation."
    );
    
    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";


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
    out.args.back().name = "Inclusivity";
    out.args.back().desc = "Controls how voxels are deemed to be 'within' the interior of the selected ROI(s)."
                           " The default 'center' considers only the central-most point of each voxel."
                           " There are two corner options that correspond to a 2D projection of the voxel onto the image plane."
                           " The first, 'planar_corner_inclusive', considers a voxel interior if ANY corner is interior."
                           " The second, 'planar_corner_exclusive', considers a voxel interior if ALL (four) corners are interior.";
    out.args.back().default_val = "center";
    out.args.back().expected = true;
    out.args.back().examples = { "center", "centre", 
                                 "planar_corner_inclusive", "planar_inc",
                                 "planar_corner_exclusive", "planar_exc" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back().name = "ContourOverlap";
    out.args.back().desc = "Controls overlapping contours are treated."
                           " The default 'ignore' treats overlapping contours as a single contour, regardless of"
                           " contour orientation. The option 'honour_opposite_orientations' makes overlapping contours"
                           " with opposite orientation cancel. Otherwise, orientation is ignored. The latter is useful"
                           " for Boolean structures where contour orientation is significant for interior contours (holes)."
                           " The option 'overlapping_contours_cancel' ignores orientation and cancels all contour overlap.";
    out.args.back().default_val = "ignore";
    out.args.back().expected = true;
    out.args.back().examples = { "ignore", "honour_opposite_orientations", 
                                 "overlapping_contours_cancel", "honour_opps", "overlap_cancel" }; 
    out.args.back().samples = OpArgSamples::Exhaustive;
    

    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The channel to operate on (zero-based)."
                           " Negative values will cause all channels to be operated on.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "-1",
                                 "0",
                                 "1" };


    out.args.emplace_back();
    out.args.back().name = "Method";
    out.args.back().desc = "Controls the specific type of normalization that will be applied."
                           " 'Stretch01' will rescale the voxel values so the minima are 0"
                           " and the maxima are 1. Likewise, 'stretch11' will rescale such"
                           " that the minima are -1 and the maxima are 1. Clamp will ensure"
                           " all voxel intensities are within [0:1] by setting those lower than"
                           " 0 to 0 and those higher than 1 to 1. (Voxels already within [0:1]"
                           " will not be altered.)"
                           " 'Sum-to-zero' will shift all voxels so that the sum of all voxel"
                           " intensities is zero. (This is useful for convolution kernels.)";
    out.args.back().default_val = "stretch11";
    out.args.back().expected = true;
    out.args.back().examples = { "clamp",
                                 "stretch01",
                                 "stretch11",
                                 "sum-to-zero" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}

Drover NormalizePixels(Drover DICOM_data,
                       const OperationArgPkg& OptArgs,
                       const std::map<std::string, std::string>&
                       /*InvocationMetadata*/,
                       const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto InclusivityStr = OptArgs.getValueStr("Inclusivity").value();
    const auto ContourOverlapStr = OptArgs.getValueStr("ContourOverlap").value();

    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );
    const auto MethodStr = OptArgs.getValueStr("Method").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_centre = Compile_Regex("^cent.*");
    const auto regex_pci = Compile_Regex("^planar_?c?o?r?n?e?r?s?_?inc?l?u?s?i?v?e?$");
    const auto regex_pce = Compile_Regex("^planar_?c?o?r?n?e?r?s?_?exc?l?u?s?i?v?e?$");

    const auto regex_ignore = Compile_Regex("^ig?n?o?r?e?$");
    const auto regex_honopps = Compile_Regex("^ho?n?o?u?r?_?o?p?p?o?s?i?t?e?_?o?r?i?e?n?t?a?t?i?o?n?s?$");
    const auto regex_cancel = Compile_Regex("^ov?e?r?l?a?p?p?i?n?g?_?c?o?n?t?o?u?r?s?_?c?a?n?c?e?l?s?$");

    const auto regex_clmp = Compile_Regex("^cl?a?m?p?$");
    const auto regex_st01 = Compile_Regex("^st?r?e?t?c?h?01$");
    const auto regex_st11 = Compile_Regex("^st?r?e?t?c?h?-11$");
    const auto regex_smtz = Compile_Regex("^su?m?.*t?o?.*z?e?r?o?$");

    const bool op_is_clmp = std::regex_match(MethodStr, regex_clmp);
    const bool op_is_st01 = std::regex_match(MethodStr, regex_st01);
    const bool op_is_st11 = std::regex_match(MethodStr, regex_st11);
    const bool op_is_smtz = std::regex_match(MethodStr, regex_smtz);

    //-----------------------------------------------------------------------------------------------------------------

    // Identify the contours to use.
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    // Perform the convolution once for every kernel.
    const auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        
        if((*iap_it)->imagecoll.images.empty()) continue;

        PartitionedImageVoxelVisitorMutatorUserData ud;
        ud.mutation_opts.editstyle = Mutate_Voxels_Opts::EditStyle::InPlace;
        ud.mutation_opts.aggregate = Mutate_Voxels_Opts::Aggregate::First;
        ud.mutation_opts.adjacency = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
        ud.mutation_opts.maskmod   = Mutate_Voxels_Opts::MaskMod::Noop;
        ud.description = "Normalized";

        if( std::regex_match(ContourOverlapStr, regex_ignore) ){
            ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::Ignore;
        }else if( std::regex_match(ContourOverlapStr, regex_honopps) ){
            ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::HonourOppositeOrientations;
        }else if( std::regex_match(ContourOverlapStr, regex_cancel) ){
            ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::ImplicitOrientations;
        }else{
            throw std::invalid_argument("ContourOverlap argument '"_s + ContourOverlapStr + "' is not valid");
        }
        if( std::regex_match(InclusivityStr, regex_centre) ){
            ud.mutation_opts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Centre;
        }else if( std::regex_match(InclusivityStr, regex_pci) ){
            ud.mutation_opts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Inclusive;
        }else if( std::regex_match(InclusivityStr, regex_pce) ){
            ud.mutation_opts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Exclusive;
        }else{
            throw std::invalid_argument("Inclusivity argument '"_s + InclusivityStr + "' is not valid");
        }

        if(op_is_clmp){
            ud.f_bounded = [Channel](long int, long int, long int chan, std::reference_wrapper<planar_image<float,double>>, float &val) {
                if( (Channel < 0) || (Channel == chan) ){
                    if(val < 0.0) val = 0.0;
                    if(1.0 < val) val = 1.0;
                }
                return;
            };

        }else if( op_is_st01
              ||  op_is_st11 ){

            // Determine the min and max voxel intensities.
            Stats::Running_MinMax<float> minmax;

            ud.f_bounded = [&](long int, long int, long int chan, std::reference_wrapper<planar_image<float,double>>, float &val) {
                if( (Channel < 0) || (Channel == chan) ){
                    minmax.Digest(val);
                }
                return;
            };
            if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                              PartitionedImageVoxelVisitorMutator,
                                                              {}, cc_ROIs, &ud )){
                throw std::runtime_error("Unable to determine min and max voxel intensities.");
            }
            const auto min = minmax.Current_Min();
            const auto max = minmax.Current_Max();

            // Prepare another functor for adjusting the voxel intensities..
            ud.f_bounded = [Channel,min,max,op_is_st01,op_is_st11](long int, long int, long int chan, std::reference_wrapper<planar_image<float,double>>, float &val) {
                if( (Channel < 0) || (Channel == chan) ){

                    if(op_is_st01){
                        val = ((max - val)/(max - min));

                    }else if(op_is_st11){
                        val = 2.0*((max - val)/(max - min)) - 1.0;
                    }
                }
                return;
            };

        }else if(op_is_smtz){
            
            // Calculate the sum of all voxels.
            double total_sum = 0.0;
            long int total_count = 0;
            ud.f_bounded = [&](long int, long int, long int chan, std::reference_wrapper<planar_image<float,double>>, float &val) {
                if( (Channel < 0) || (Channel == chan) ){
                    total_sum += val;
                    ++total_count;
                }
                return;
            };
            if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                              PartitionedImageVoxelVisitorMutator,
                                                              {}, cc_ROIs, &ud )){
                throw std::runtime_error("Unable to determine sum of voxel intensities.");
            }
            const auto per_voxel_sum = total_sum / static_cast<double>(total_count);

            // Prepare another functor for applying the shift.
            ud.f_bounded = [Channel,per_voxel_sum](long int, long int, long int chan, std::reference_wrapper<planar_image<float,double>>, float &val) {
                if( (Channel < 0) || (Channel == chan) ){
                    val -= per_voxel_sum;
                }
                return;
            };

        }else{
            throw std::logic_error("Requested method is not understood. Cannot continue.");
        }

        // Apply the adjustment closure.
        if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                          PartitionedImageVoxelVisitorMutator,
                                                          {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to normalize images.");
        }
    }

    return DICOM_data;
}
