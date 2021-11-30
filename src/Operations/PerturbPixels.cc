//PerturbPixels.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <random>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include <boost/geometry.hpp>
#include <boost/geometry/index/rtree.hpp>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../String_Parsing.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/Partitioned_Image_Voxel_Visitor_Mutator.h"

#include "PerturbPixels.h"

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorStats.h"



OperationDoc OpArgDocPerturbPixels(){
    OperationDoc out;
    out.name = "PerturbPixels";

    out.desc = 
        "This operation applies random noise to voxel intensities."
        " It can be used to help fuzz testing or benchmark statistical analysis.";

    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The image channel to use. Zero-based. Use '-1' to operate on all available channels.";
    out.args.back().default_val = "-1";
    out.args.back().expected = true;
    out.args.back().examples = { "-1", "0", "1", "2" };

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "ContourOverlap";
    out.args.back().desc = "Controls overlapping contours are treated."
                           " The default 'ignore' treats overlapping contours as a single contour, regardless of"
                           " contour orientation. This will effectively honour only the outermost contour regardless of"
                           " orientation, but provides the most predictable and consistent results."
                           " The option 'honour_opposite_orientations' makes overlapping contours"
                           " with opposite orientation cancel. Otherwise, orientation is ignored. This is useful"
                           " for Boolean structures where contour orientation is significant for interior contours (holes)."
                           " If contours do not have consistent overlap (e.g., if contours intersect) the results"
                           " can be unpredictable and hard to interpret."
                           " The option 'overlapping_contours_cancel' ignores orientation and alternately cancerls"
                           " all overlapping contours."
                           " Again, if the contours do not have consistent overlap (e.g., if contours intersect) the results"
                           " can be unpredictable and hard to interpret.";
    out.args.back().default_val = "ignore";
    out.args.back().expected = true;
    out.args.back().examples = { "ignore", "honour_opposite_orientations", 
                            "overlapping_contours_cancel", "honour_opps", "overlap_cancel" }; 
    out.args.back().samples = OpArgSamples::Exhaustive;

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
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";


    out.args.emplace_back();
    out.args.back().name = "Model";
    out.args.back().desc = "Controls which type of noise is applied."
                           "\n\n"
                           "'gaussian(centre, std_dev)' applies a Gaussian model centered on 'centre' with the given"
                           " standard deviation."
                           "\n\n"
                           "'uniform(lower, upper)' applies a uniform noise model where noise values are selected"
                           " with equal probability inside the range [lower, upper]."
                           "\n\n"
                           "Note that if any parameters have a '%' or 'x' suffix, they are treated as percentages or"
                           " fractions relative to the pre-perturbed voxel intensity.";
    out.args.back().default_val = "gaussian(0.0, 1.0)";
    out.args.back().expected = true;
    out.args.back().examples = { "gaussian(0.0, 1.0)",
                                 "gaussian(0.0, 0.5x)",
                                 "gaussian(0.0, 50%)",
                                 "gaussian(2.5, 50%)",
                                 "gaussian(0.2x, 0.1)",
                                 "uniform(-1.0, 1.0)",
                                 "uniform(-1.0x, 1.0x)" };

    out.args.emplace_back();
    out.args.back().name = "Method";
    out.args.back().desc = "Controls how the noise is applied to the voxel intensity.";
    out.args.back().default_val = "additive";
    out.args.back().expected = true;
    out.args.back().examples = { "additive", "multiplicative" };

    out.args.emplace_back();
    out.args.back().name = "Seed";
    out.args.back().desc = "The seed value to use for random number generation.";
    out.args.back().default_val = "1337";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "1337", "1500450271" };

    return out;
}



bool PerturbPixels(Drover &DICOM_data,
                   const OperationArgPkg& OptArgs,
                   std::map<std::string, std::string>& /*InvocationMetadata*/,
                   const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto InclusivityStr = OptArgs.getValueStr("Inclusivity").value();
    const auto ContourOverlapStr = OptArgs.getValueStr("ContourOverlap").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto ModelStr = OptArgs.getValueStr("Model").value();
    const auto MethodStr = OptArgs.getValueStr("Method").value();
    const auto Seed = std::stol( OptArgs.getValueStr("Seed").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_add  = Compile_Regex("^ad?d?i?t?i?v?e?l?y?$");
    const auto regex_mult = Compile_Regex("^mu?l?t?i?p?l?i?c?a?t?i?v?e?l?y?$");

    const auto regex_gaussian = Compile_Regex("^ga?u?s?s?i?a?n?.*");
    const auto regex_uniform  = Compile_Regex("^un?i?f?o?r?m?.*");

    const auto regex_centre = Compile_Regex("^ce?n?t?[re]?[er]?");
    const auto regex_pci = Compile_Regex("^pl?a?n?a?r?[_-]?c?o?r?n?e?r?s?[_-]?inc?l?u?s?i?v?e?$");
    const auto regex_pce = Compile_Regex("^pl?a?n?a?r?[_-]?c?o?r?n?e?r?s?[_-]?exc?l?u?s?i?v?e?$");

    const auto regex_ignore = Compile_Regex("^ig?n?o?r?e?$");
    const auto regex_honopps = Compile_Regex("^ho?n?o?u?r?_?o?p?p?o?s?i?t?e?[_-]?o?r?i?e?n?t?a?t?i?o?n?s?$");
    const auto regex_cancel = Compile_Regex("^ov?e?r?l?a?p?p?i?n?g?[_-]?c?o?n?t?o?u?r?s?[_-]?c?a?n?c?e?l?s?$");

    const bool contour_overlap_ignore  = std::regex_match(ContourOverlapStr, regex_ignore);
    const bool contour_overlap_honopps = std::regex_match(ContourOverlapStr, regex_honopps);
    const bool contour_overlap_cancel  = std::regex_match(ContourOverlapStr, regex_cancel);
    //-----------------------------------------------------------------------------------------------------------------
    const bool method_is_add  = std::regex_match(MethodStr, regex_add);
    const bool method_is_mult = std::regex_match(MethodStr, regex_mult);

    const bool model_is_gaussian  = std::regex_match(ModelStr, regex_gaussian);
    const bool model_is_uniform   = std::regex_match(ModelStr, regex_uniform);

    auto pfs = retain_only_numeric_parameters(parse_functions(ModelStr));
    if(pfs.size() != 1){
        throw std::invalid_argument("Model accepts a single function only");
    }
    auto pf = pfs.front();

    if( !method_is_add
    &&  !method_is_mult ){
        throw std::invalid_argument("Method not understood");
    }
    if(model_is_gaussian){
        if(pf.parameters.size() != 2){
            throw std::invalid_argument("Invalid number of arguments supplied for Gaussian model");
        }

    }else if(model_is_uniform){
        if(pf.parameters.size() != 2){
            throw std::invalid_argument("Invalid number of arguments supplied for uniform model");
        }

    }else{
        throw std::invalid_argument("Model not understood");
    }
    
    std::mt19937 re(Seed);

    const auto apply_function = [=,&re](float intensity) -> float {

        // Scale numbers as needed.
        std::vector<double> l_numbers;
        l_numbers.reserve(pf.parameters.size());
        for(const auto &fp : pf.parameters){
            if(fp.is_fractional){
                l_numbers.push_back( fp.number.value() * intensity );
            }else if(fp.is_percentage){
                l_numbers.push_back( fp.number.value() * intensity / 100.0 );
            }else{
                l_numbers.push_back( fp.number.value() );
            }
        }

        // Apply the model.
        if(model_is_gaussian){
            std::normal_distribution<float> rd(l_numbers[0], l_numbers[1]);
            const auto r = rd(re);
            if(method_is_add){
                intensity += r;
            }else if(method_is_mult){
                intensity *= r;
            }

        }else if(model_is_uniform){
            std::uniform_real_distribution<float> rd(l_numbers[0], l_numbers[1]);
            const auto r = rd(re);
            if(method_is_add){
                intensity += r;
            }else if(method_is_mult){
                intensity *= r;
            }

        }else{
            throw std::logic_error("Model not supported");
        }
        return intensity;
    };

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        PartitionedImageVoxelVisitorMutatorUserData ud;

        ud.mutation_opts.editstyle = Mutate_Voxels_Opts::EditStyle::InPlace;
        ud.mutation_opts.aggregate = Mutate_Voxels_Opts::Aggregate::First;
        ud.mutation_opts.adjacency = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
        ud.mutation_opts.maskmod   = Mutate_Voxels_Opts::MaskMod::Noop;
        ud.description = "Perturbed voxels";

        if( contour_overlap_ignore ){
            ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::Ignore;
        }else if( contour_overlap_honopps ){
            ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::HonourOppositeOrientations;
        }else if( contour_overlap_cancel ){
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

        Mutate_Voxels_Functor<float,double> f_noop;
        ud.f_bounded = f_noop;
        ud.f_unbounded = f_noop;
        ud.f_visitor = f_noop;

        ud.f_bounded = [&](long int /*row*/, long int /*col*/, long int chan,
                           std::reference_wrapper<planar_image<float,double>> /*img_refw*/,
                           std::reference_wrapper<planar_image<float,double>> /*mask_img_refw*/,
                           float &voxel_val) {
            if( (Channel < 0) || (Channel == chan) ){
                voxel_val = apply_function(voxel_val);
            }
        };

        // Note: this operation is not performed in parallel so that random number generation is deterministic.
        if(!(*iap_it)->imagecoll.Process_Images( GroupIndividualImages,
                                                 PartitionedImageVoxelVisitorMutator,
                                                 {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to perturb voxels within the specified ROI(s).");
        }
    }

    return true;
}
