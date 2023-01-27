//OptimizeStaticBeams.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <limits>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <optional>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>
#include <vector>
#include <utility>
#include <random>
#include <filesystem>

#ifdef DCMA_USE_NLOPT
#include <nlopt.hpp>
#endif // DCMA_USE_NLOPT

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"

#include "Explicator.h"

#include "../Insert_Contours.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Write_File.h"
#include "../Thread_Pool.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/Partitioned_Image_Voxel_Visitor_Mutator.h"

#include "OptimizeStaticBeams.h"

// This struct is used for evaluating a given beam weight configuration in terms of the dose to a target ROI.
struct dose_dist_stats {
    
    double D_min    = std::numeric_limits<double>::quiet_NaN();
    double D_mean   = std::numeric_limits<double>::quiet_NaN();
    double D_max    = std::numeric_limits<double>::quiet_NaN();

    double D_02     = std::numeric_limits<double>::quiet_NaN(); // 2nd percentile.
    double D_05     = std::numeric_limits<double>::quiet_NaN(); // 5th percentile.
    double D_50     = std::numeric_limits<double>::quiet_NaN(); // Median.
    double D_95     = std::numeric_limits<double>::quiet_NaN(); // 95th percentile.
    double D_98     = std::numeric_limits<double>::quiet_NaN(); // 98th percentile.

    double cost     = std::numeric_limits<double>::quiet_NaN();

};

// Global objects so lambdas can be non-capturing and thus passed as function pointers.
std::function< dose_dist_stats (std::vector<double> &, std::vector<double> &)> global_evaluate_weights;
std::vector<double> global_working;
bool generate_dose_dist_stats = false;


// This routine determines the normalization factor required to satisfy the given DVH criteria: $V_{D} \geq V_{min}$.
// Every element in the input should be multiplied with the return value to satisfy the DVH criteria.
//
// Note: it may not be possible to satisfy the criteria. Check the result for non-finite values.
//
// Note: Vmin is assumed to be a fraction (of the number of elements, but if each element represents the same fractional
//       volume of the whole then Vmin is also a fraction of the whole volume).
//
// Note: D is in whatever units the input data is; most likely absolute dose. If the normalization condition is a
//       fraction of the prescribed dose, it will need to be converted into absolute dose if the input data is in
//       absolute dose.
//
double DVH_Normalize(std::vector<double> in, 
                     double D, 
                     double Vmin){

    // Compute the current dose that corresponds to Vmin.
    const auto D_current = Stats::Percentile(std::move(in), (1.0 - Vmin));

    // Scale needed to make it coincide with desired dose.
    return D / D_current;
}


OperationDoc OpArgDocOptimizeStaticBeams(){
    OperationDoc out;
    out.name = "OptimizeStaticBeams";

    out.desc = 
      "This operation takes dose matrices corresponding to single, static RT beams and attempts to"
      " optimize beam weighting to create an optimal plan subject to various criteria.";

    out.notes.emplace_back(
        "This routine is a simplisitic routine that attempts to estimate the optimal beam weighting."
        " It should NOT be used for clinical purposes, except maybe as a secondary check or a means"
        " to guess reasonable beam weights prior to optimization within the clinical TPS."
    );

    out.notes.emplace_back(
        "Because beam weights are (generally) not specified in DICOM RTDOSE files, the beam weights"
        " are assumed to all be 1.0. If they are not all 1.0, the weights reported here will be relative"
        " to whatever the existing weights are."
    );

    out.notes.emplace_back(
        "If no PTV ROI is available, the BODY contour may suffice. If this is not available, dose outside"
        " the body should somehow be set to zero to avoid confusing D_{max} metrics."
        " For example, bolus D_{max} can be high, but is ultimately irrelevant."
    );

    out.notes.emplace_back(
        "By default, this routine uses all available images. This may be fixed in a future release."
        " Patches are welcome."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "all";

    out.args.emplace_back();
    out.args.back().name = "ResultsSummaryFileName";
    out.args.back().desc = "This file will contain a brief summary of the results."
                           " The format is CSV. Leave empty to dump to generate a unique temporary file."
                           " If an existing file is present, rows will be appended without writing a header.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile", "localfile.csv", "derivative_data.csv" };
    out.args.back().mimetype = "text/csv";


    out.args.emplace_back();
    out.args.back().name = "UserComment";
    out.args.back().desc = "A string that will be inserted into the output file which will simplify merging output"
                           " with differing parameters, from different sources, or using sub-selections of the data.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "Using XYZ", "Patient treatment plan C" };


    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";


    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";


    out.args.emplace_back();
    out.args.back().name = "MaxVoxelSamples";
    out.args.back().desc = "The maximum number of voxels to randomly sample (deterministically) within the PTV."
                           " Setting lower will result in faster calculation, but lower precision."
                           " A reasonable setting depends on the size of the target structure; small"
                           " targets may suffice with a few hundred voxels, but larger targets"
                           " probably require several thousand.";
    out.args.back().default_val = "1000";
    out.args.back().expected = true;
    out.args.back().examples = { "200", "500", "1000", "2000", "5000" };


    out.args.emplace_back();
    out.args.back().name = "NormalizationD";
    out.args.back().desc = "The isodose value that should envelop a given volume in the PTV ROI."
                           " In other words, this parameter is the 'D' parameter in a DVH constraint"
                           " of the form $V_{D} \\geq V_{min}$. It should be given as a fraction"
                           " within [0:1] relative to the prescription dose."
                           " For example, 95% isodose should be provided as '0.95'.";
    out.args.back().default_val = "0.95";
    out.args.back().expected = true;
    out.args.back().examples = { "0.90", "0.95", "0.98", "0.99", "1.0" };


    out.args.emplace_back();
    out.args.back().name = "NormalizationV";
    out.args.back().desc = "The minimal fractional volume of ROI that should be enclosed within one or more surfaces"
                           " that demarcate the given isodose value."
                           " In other words, this parameter is the 'Vmin' parameter in a DVH constraint"
                           " of the form $V_{D} \\geq V_{min}$. It should be given as a fraction"
                           " within [0:1] relative to the volume of the ROI (typically discretized to the number of"
                           " voxels in the ROI)."
                           " For example, if Vmin = 99%, provide the value '0.99'.";
    out.args.back().default_val = "0.99";
    out.args.back().expected = true;
    out.args.back().examples = { "0.90", "0.95", "0.98", "0.99", "1.0" };


    out.args.emplace_back();
    out.args.back().name = "RxDose";
    out.args.back().desc = "The dose prescribed to the ROI that will be optimized."
                           " The units depend on the DICOM file, but will likely be Gy.";
    out.args.back().default_val = "70.0";
    out.args.back().expected = true;
    out.args.back().examples = { "48.0", "60.0", "63.3", "70.0", "100.0" };

    return out;
}

bool OptimizeStaticBeams(Drover &DICOM_data,
                           const OperationArgPkg& OptArgs,
                           std::map<std::string, std::string>& /*InvocationMetadata*/,
                           const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    auto ResultsSummaryFileName = OptArgs.getValueStr("ResultsSummaryFileName").value();

    const auto UserComment = OptArgs.getValueStr("UserComment");

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto MaxVoxelSamples = std::stol( OptArgs.getValueStr("MaxVoxelSamples").value() );

    const auto dvh_D_frac = std::stod(  OptArgs.getValueStr("NormalizationD").value() );
    const auto dvh_Vmin_frac = std::stod(  OptArgs.getValueStr("NormalizationV").value() );
    const auto D_Rx = std::stod(  OptArgs.getValueStr("RxDose").value() );

    //-----------------------------------------------------------------------------------------------------------------

    if(ResultsSummaryFileName.empty()){
        ResultsSummaryFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_optimizestaticbeamssummary_", 6, ".csv");
    }

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );

    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    std::mutex common_access;
    std::vector<std::vector<double>> voxels;
    std::vector<std::string> beam_id; // Something that will identify each beam.

    // Cycle over the Image_Arrays, extracting for each a collection of relevant voxels.
    //
    // Note: The 'relevancy' oracle must be identical for all fields/Image_Arrays. An ROI would be ideal.
    //       In practice, it might be best to use any single field and contour at a very low threshold dose.
    //       There are obvious issues if the fields do not overlap. Another option is to boolean-or contours
    //       from every field. This might be overkill.
    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    IAs = Whitelist(IAs, "Modality", "RTDOSE");
    for(auto & iap_it : IAs){
        if((*iap_it)->imagecoll.images.empty()) throw std::invalid_argument("Unable to find an image to analyze.");

        std::optional<std::string> BeamID;
        if(auto BeamNumber = (*iap_it)->imagecoll.images.front().GetMetadataValueAs<std::string>("ReferencedRTPlanSequence/ReferencedFractionGroupSequence/ReferencedBeamSequence/ReferencedBeamNumber")){
            BeamID = BeamNumber.value();
        }else if(auto BeamNumber = (*iap_it)->imagecoll.images.front().GetMetadataValueAs<std::string>("ReferencedBeamNumber")){
            BeamID = BeamNumber.value();
        }

        std::optional<std::string> Fname;
        if(auto Filename = (*iap_it)->imagecoll.images.front().GetMetadataValueAs<std::string>("Filename")){
            Fname = Filename.value();
        }

        beam_id.emplace_back(BeamID.value_or("unknown beam number") + " (" + Fname.value_or("unknown field name") + ")");
        YLOGINFO("Processing dose corresponding to beam number: " << beam_id.back());

        PartitionedImageVoxelVisitorMutatorUserData ud;
        ud.mutation_opts.editstyle = Mutate_Voxels_Opts::EditStyle::InPlace;
        ud.mutation_opts.aggregate = Mutate_Voxels_Opts::Aggregate::First;
        ud.mutation_opts.adjacency = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
        ud.mutation_opts.maskmod   = Mutate_Voxels_Opts::MaskMod::Noop;
        ud.description = "";

        //ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::HonourOppositeOrientations;
        //ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::ImplicitOrientations;
        ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::Ignore;

        //ud.mutation_opts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Exclusive;
        //ud.mutation_opts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Inclusive;
        ud.mutation_opts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Centre;

        Mutate_Voxels_Functor<float,double> f_noop;
        ud.f_unbounded = f_noop;
        ud.f_visitor = f_noop;
        ud.f_bounded = [&](long int /*row*/, long int /*col*/, long int /*chan*/,
                           std::reference_wrapper<planar_image<float,double>> /*img_refw*/,
                           std::reference_wrapper<planar_image<float,double>> /*mask_img_refw*/,
                           float &voxel_val) {
            // For small ROIs this routine will infrequently be called because most time will be spent checking whether
            // voxels are inside the ROI. It is faster to parallelize with a spinlock than using a single core.
            voxels.back().emplace_back(voxel_val);
        };

        voxels.emplace_back();
        if(!(*iap_it)->imagecoll.Process_Images( GroupIndividualImages,
                                                 PartitionedImageVoxelVisitorMutator,
                                                 {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to harvest voxels within the specified ROI(s).");
        }
        if(voxels.back().empty()){
            voxels.pop_back();
            beam_id.pop_back();
        }
    }

    if(voxels.empty()) throw std::domain_error("No voxels identified interior to the selected ROI(s). Cannot continue.");

    {
        auto same_size = [&](const std::vector<double> &v){
            return (voxels.front().size() == v.size());
        };
        if(! std::all_of(voxels.begin(), voxels.end(), same_size) ){
            throw std::domain_error("Dose matrices do not align. Cannot continue.");
            // Note: this is a reasonable scenario, but not currently supported. If needed you could try: resampling
            //       or resizing all matrices, implementing a grid-independent sampling routine for this operation, or
            //       using a dose meld + zeroing-out dose matrices to get a common grid.
        }
    }

    //Reduce the number of voxels by randomly trimming until a small, *hopefully* representative collection remain.
    //
    // TODO: Use a more consistent and intellingent sampling routine to ensure representative coverage of the ROI.
    const long int N_voxels_max = MaxVoxelSamples;
    const long int random_seed = 123456;
    std::mt19937 re_orig( random_seed );
    {
        for(auto &vec : voxels){
            auto re = re_orig;
            std::shuffle(vec.begin(), vec.end(), re);
        }
        if(static_cast<long int>(voxels.front().size()) > N_voxels_max){
            for(auto &vec : voxels){
                vec.resize( N_voxels_max );
            }
        }
    }

    const auto N_beams = static_cast<long int>(voxels.size());
    const auto N_voxels = voxels.front().size();

    std::stringstream ss;

    // This routine evaluates weighting schemes to produce cost and quality metrics.
    auto evaluate_weights = [&,N_beams](std::vector<double> &weights, 
                                        std::vector<double> &working) -> dose_dist_stats {

        dose_dist_stats out;

        // Compute the total dose using the current weighting scheme.
        //
        // Note: This requires consistent voxel ordering!
        std::fill(working.begin(), working.end(), 0.0);
        for(long int beam = 0; beam < N_beams; ++beam){
            const auto weight = weights[beam];
            std::transform( working.begin(), working.end(),
                            voxels[beam].begin(), 
                            working.begin(),
                            [=](const double &L, const double &R){ return L + weight * R; });
        }

        // Sanity check.
        const auto D_max = Stats::Max(working);
        if(!std::isfinite(D_max) || (D_max < 1E-3)){
            out.cost = std::numeric_limits<double>::max();
        }

        // Scale the weighted dose distribution to achieve the specified normalization.
        const auto DVH_norm_D = dvh_D_frac * D_Rx;
        const auto DVH_norm_Vmin = dvh_Vmin_frac;
        const auto dose_scaler = DVH_Normalize(working, DVH_norm_D, DVH_norm_Vmin);

        std::transform(working.begin(), working.end(), 
                       working.begin(), [=](double D) -> double { return D*dose_scaler; });
        
        // Generate descriptive stats for the dose distribution.
        if(generate_dose_dist_stats){
            out.D_min  = 100.0 * Stats::Min(working) / D_Rx;
            out.D_max  = 100.0 * Stats::Max(working) / D_Rx;
            out.D_mean = 100.0 * Stats::Mean(working) / D_Rx;
            out.D_02   = Stats::Percentile(working, 0.02);
            out.D_05   = Stats::Percentile(working, 0.05);
            out.D_50   = Stats::Percentile(working, 0.50);
            out.D_95   = Stats::Percentile(working, 0.95);
            out.D_98   = Stats::Percentile(working, 0.98);
        }

        //Compute the cost function for each dose element.
        std::transform(working.begin(), working.end(), 
                       working.begin(), [=](double D) -> double { return std::pow(D - D_Rx, 2.0); });
        out.cost = Stats::Sum(working);

/*
            std::cout << "Evaluation # " << eval_count << " yields " << total_cost << " with weights ";
            for(auto &w : weights) std::cout << w << " ";
            std::cout << "Dmin= " << Dmin << "\% Dmean= " << Dmean << "\% Dmax= " << Dmax << "\%";
            std::cout << std::endl;
*/
        return out;
    };
    global_evaluate_weights = evaluate_weights;

    //Constrained surface optimization.
    auto f_to_optimize = [](const std::vector<double> &open_weights, 
                            std::vector<double> &grad, 
                            void * ) -> double {
        if(!grad.empty()) throw std::logic_error("This implementation cannot handle derivatives.");

        std::vector<double> weights(open_weights);
        const auto sum = std::accumulate(weights.begin(), weights.end(), 0.0);
        std::transform(weights.begin(), weights.end(), weights.begin(), [=](double ow) -> double { return ow / sum; });

        auto res = global_evaluate_weights(weights, global_working);
        return res.cost;
    };

    std::vector<double> open_weights(N_beams, 0.5);
    std::vector<double> working(N_voxels, 0.0);
    global_working = working;

#ifdef DCMA_USE_NLOPT
    //nlopt::opt optimizer(nlopt::LN_NELDERMEAD, N_beams);
    nlopt::opt optimizer(nlopt::GN_DIRECT_L, N_beams);
    //nlopt::opt optimizer(nlopt::GN_ISRES, N_beams);
    //nlopt::opt optimizer(nlopt::GN_ESCH, N_beams);

    std::vector<double> lower_bounds(N_beams, 0.0);
    std::vector<double> upper_bounds(N_beams, 1.0);

    optimizer.set_lower_bounds(lower_bounds);
    optimizer.set_upper_bounds(upper_bounds);
    optimizer.set_min_objective(f_to_optimize, nullptr);
    optimizer.set_ftol_abs(-HUGE_VAL);
    optimizer.set_ftol_rel(1.0E-8);
    optimizer.set_xtol_abs(-HUGE_VAL);
    optimizer.set_xtol_rel(-HUGE_VAL);
    optimizer.set_maxeval(500'000);
    double minf;

    generate_dose_dist_stats = false;
    YLOGINFO("Beginning optimization now..")
    nlopt::result nlopt_result = optimizer.optimize(open_weights, minf); // open_weights will contain the current-best weights on success.
    YLOGINFO("Optimizer result: " << nlopt_result);
#else // DCMA_USE_NLOPT
    throw std::runtime_error("Unable to optimize -- nlopt was not used");
#endif // DCMA_USE_NLOPT

    std::vector<double> weights(open_weights);
    const auto sum = std::accumulate(weights.begin(), weights.end(), 0.0);
    std::transform(weights.begin(), weights.end(), 
                   weights.begin(), [=](double ow) -> double { return ow / sum; });

    generate_dose_dist_stats = true;
    const auto res = global_evaluate_weights(weights, global_working);

    // Construct a summary.
    std::stringstream summary;
    summary << "The best weights are: " << std::endl;
    for(size_t i = 0; ( (i < weights.size()) && (i < beam_id.size()) ); ++i){
        summary << beam_id[i] << ": " << weights[i] << std::endl;
    }
    summary << std::endl;
    
    summary << "# of voxels = " << N_voxels << std::endl
            << "# of beams  = " << N_beams << std::endl
            << std::endl;

    summary << "D_min  = " << res.D_min << std::endl
            << "D_mean = " << res.D_mean << std::endl
            << "D_max  = " << res.D_max << std::endl
            << std::endl;

    summary << "D_02   = " << res.D_02 << std::endl
            << "D_05   = " << res.D_05 << std::endl
            << "D_50   = " << res.D_50 << std::endl
            << "D_95   = " << res.D_95 << std::endl
            << "D_98   = " << res.D_98 << std::endl
            << std::endl;

    summary << "D_min -- D_max span = " << std::abs(res.D_min - res.D_max) << std::endl
            << "D_02  -- D_98  span = " << std::abs(res.D_02  - res.D_98) << std::endl
            << "D_05  -- D_95  span = " << std::abs(res.D_05  - res.D_95) << std::endl
            << std::endl;

    summary << "cost   = " << res.cost << std::endl
            << std::endl;

    std::cout << summary.str();

    //Write the summary to file.
    {
        auto gen_filename = [&]() -> std::string {
            if(!ResultsSummaryFileName.empty()){
                return ResultsSummaryFileName;
            }
            const auto base = std::filesystem::temp_directory_path() / "dcma_staticbeamoptisummary_";
            return Get_Unique_Sequential_Filename(base.string(), 6, ".csv");
        };

        YLOGINFO("About to claim a mutex");
        Append_File( gen_filename,
                     "dcma_op_optimizestaticbeams_mutex",
                     "",
                     summary.str() );
    }

    return true;
}
