//PerlinImages.cc - A part of DICOMautomaton 2026. Written by hal clark.

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
#include <cstdint>

#include <boost/geometry.hpp>
#include <boost/geometry/index/rtree.hpp>

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorStats.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Perlin_Noise.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/Partitioned_Image_Voxel_Visitor_Mutator.h"

#include "PerlinImages.h"



OperationDoc OpArgDocPerlinImages(){
    OperationDoc out;
    out.name = "PerlinImages";

    out.tags.emplace_back("category: image processing");
    out.tags.emplace_back("category: contour processing");

    out.desc = 
        "This operation overwrites voxel data inside and/or outside of ROI(s) with Perlin noise.";


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
    out.args.back().name = "ExteriorOverwrite";
    out.args.back().desc = "Whether to overwrite voxels exterior to the specified ROI(s).";
    out.args.back().default_val = "true";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "InteriorOverwrite";
    out.args.back().desc = "Whether to overwrite voxels interior to the specified ROI(s).";
    out.args.back().default_val = "true";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
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
    out.args.back() = CCWhitelistOpArgDoc();
    out.args.back().name = "ROISelection";
    out.args.back().default_val = "all";

    out.args.emplace_back();
    out.args.back().name = "Seed";
    out.args.back().desc = "The random seed used for deterministic sampling."
                           " Different seeds will produce different (but reproducible) selections."
                           " Negative values will generate a random seed, but note that the same seed will"
                           " be used for each selected image array.";
    out.args.back().default_val = "-1";
    out.args.back().expected = true;
    out.args.back().examples = { "-1", "0", "12345", "54321", "99999" };


    out.args.emplace_back();
    out.args.back().name = "Scale";
    out.args.back().desc = "The 'scale' of the Perlin noise features, which is related to the frequency of the noise,"
                           " not the amplitude or intensity."
                           "\n\n"
                           "Note that the voxel spacing is already incorporated into the noise generation. For"
                           " 1.0 (unit) voxel spacing, it is recommended to use a scale of around 0.1 -- but the ideal"
                           " scale factor will depend on the application.";
    out.args.back().default_val = "0.1";
    out.args.back().expected = true;
    out.args.back().examples = { "0.001", "0.1", "1.0", "10.0" };


    return out;
}



bool PerlinImages(Drover &DICOM_data,
                     const OperationArgPkg& OptArgs,
                     std::map<std::string, std::string>& /*InvocationMetadata*/,
                     const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto InclusivityStr = OptArgs.getValueStr("Inclusivity").value();
    const auto ContourOverlapStr = OptArgs.getValueStr("ContourOverlap").value();

    const auto ExteriorOverwriteStr = OptArgs.getValueStr("ExteriorOverwrite").value();
    const auto InteriorOverwriteStr = OptArgs.getValueStr("InteriorOverwrite").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();

    const auto SeedStr = OptArgs.getValueStr("Seed").value();
    const auto Scale = std::stod(OptArgs.getValueStr("Scale").value());

    //-----------------------------------------------------------------------------------------------------------------

    const auto regex_true = Compile_Regex("^tr?u?e?$");

    const auto regex_centre = Compile_Regex("^ce?n?t?[re]?[er]?");
    const auto regex_pci = Compile_Regex("^pl?a?n?a?r?[_-]?c?o?r?n?e?r?s?[_-]?inc?l?u?s?i?v?e?$");
    const auto regex_pce = Compile_Regex("^pl?a?n?a?r?[_-]?c?o?r?n?e?r?s?[_-]?exc?l?u?s?i?v?e?$");

    const auto regex_ignore = Compile_Regex("^ign?o?r?e?$");
    const auto regex_honopps = Compile_Regex("^hon?o?u?r?[_-]?o?p?p?o?s?i?t?e?[_-]?o?r?i?e?n?t?a?t?i?o?n?s?$");
    const auto regex_cancel = Compile_Regex("^o?v?e?r?l?a?p?p?i?n?g?[_-]?c?o?n?t?o?u?r?s?[_-]?can?c?e?l?s?$");

    const auto ShouldOverwriteExterior = std::regex_match(ExteriorOverwriteStr, regex_true);
    const auto ShouldOverwriteInterior = std::regex_match(InteriorOverwriteStr, regex_true);

    const bool contour_overlap_ignore  = std::regex_match(ContourOverlapStr, regex_ignore);
    const bool contour_overlap_honopps = std::regex_match(ContourOverlapStr, regex_honopps);
    const bool contour_overlap_cancel  = std::regex_match(ContourOverlapStr, regex_cancel);

    int64_t Seed = std::stol(SeedStr);
    if(Seed < 0){
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<int64_t> dist;
        Seed = std::abs(dist(gen));
    }

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    PerlinNoise pn(Seed);

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        PartitionedImageVoxelVisitorMutatorUserData ud;

        ud.mutation_opts.editstyle = Mutate_Voxels_Opts::EditStyle::InPlace;
        ud.mutation_opts.aggregate = Mutate_Voxels_Opts::Aggregate::First;
        ud.mutation_opts.adjacency = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
        ud.mutation_opts.maskmod   = Mutate_Voxels_Opts::MaskMod::Noop;
        ud.description = "Highlighted ROIs";

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

        auto f_overwrite = [&](int64_t row, int64_t col, int64_t chan,
                               std::reference_wrapper<planar_image<float,double>> img_refw,
                               std::reference_wrapper<planar_image<float,double>> /*mask_img_refw*/,
                               float &voxel_val) {
            if( (Channel < 0) || (Channel == chan) ){
                const auto pos = img_refw.get().position(row, col);
                voxel_val = pn.sample(pos, Scale);
            }
            return;
        };

        if(ShouldOverwriteInterior){
            ud.f_bounded = f_overwrite;
        }
        if(ShouldOverwriteExterior){
            ud.f_bounded = f_overwrite;
        }

        if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                          PartitionedImageVoxelVisitorMutator,
                                                          {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to overwrite voxels within the specified ROI(s) with Perlin noise.");
        }
    }

    return true;
}
