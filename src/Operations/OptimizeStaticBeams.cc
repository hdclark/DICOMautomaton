//OptimizeStaticBeams.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <limits>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <experimental/optional>
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
#include <vector>
#include <utility>

#include "../Insert_Contours.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Write_File.h"
#include "../Thread_Pool.h"

#include "OptimizeStaticBeams.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.


OperationDoc OpArgDocOptimizeStaticBeams(void){
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
        "This operation uses Image_Arrays, so convert from Dose_Arrays if necessary prior to calling"
        " this routine. This may be fixed in a future release. Patches are welcome."
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
    out.args.back().name = "ResultsSummaryFileName";
    out.args.back().desc = "This file will contain a brief summary of the results."
                           " The format is CSV. Leave empty to dump to generate a unique temporary file."
                           " If an existing file is present, rows will be appended without writing a header.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile", "localfile.csv", "derivative_data.csv" };
    out.args.back().mimetype = "text/csv";


    out.args.emplace_back();
    out.args.back().name = "FullResultsFileName";
    out.args.back().desc = "This file will contain an exhaustive summary of the results."
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

    return out;
}

Drover OptimizeStaticBeams(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    auto ResultsSummaryFileName = OptArgs.getValueStr("ResultsSummaryFileName").value();
    auto FullResultsFileName = OptArgs.getValueStr("FullResultsFileName").value();

    const auto UserComment = OptArgs.getValueStr("UserComment");

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    //-----------------------------------------------------------------------------------------------------------------

    if(ResultsSummaryFileName.empty()){
        ResultsSummaryFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_optimizestaticbeamssummary_", 6, ".csv");
    }
    if(FullResultsFileName.empty()){
        FullResultsFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_optimizestaticbeamsfull_", 6, ".csv");
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
    auto iap_it = DICOM_data.image_data.begin();
    while(iap_it != DICOM_data.image_data.end()){
        if((*iap_it)->imagecoll.images.empty()) throw std::invalid_argument("Unable to find an image to analyze.");

        std::experimental::optional<std::string> BeamID;
        if(false){
        }else if(auto BeamNumber = (*iap_it)->imagecoll.images.front().GetMetadataValueAs<std::string>("ReferencedRTPlanSequence/ReferencedFractionGroupSequence/ReferencedBeamSequence/ReferencedBeamNumber")){
            BeamID = BeamNumber.value();
        }else if(auto BeamNumber = (*iap_it)->imagecoll.images.front().GetMetadataValueAs<std::string>("ReferencedBeamNumber")){
            BeamID = BeamNumber.value();
        }

        std::experimental::optional<std::string> Fname;
        if(false){
        }else if(auto Filename = (*iap_it)->imagecoll.images.front().GetMetadataValueAs<std::string>("Filename")){
            Fname = Filename.value();
        }

        beam_id.emplace_back(BeamID.value_or("unknown") + " (" + Fname.value_or("unknown") + ")");
        FUNCINFO("Processing dose corresponding to beam number: " << beam_id.back());

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

        std::function<void(long int, long int, long int, float &)> f_noop;
        ud.f_unbounded = f_noop;
        ud.f_visitor = f_noop;
        ud.f_bounded = [&](long int /*row*/, long int /*col*/, long int chan, float &voxel_val) {
            // For small ROIs this routine will infrequently be called because most time will be spent checking whether
            // voxels are inside the ROI. It is faster to parallelize with a spinlock than using a single core.
//            std::lock_guard<std::mutex> lock(common_access);
            voxels.back().emplace_back(voxel_val);
        };

        voxels.emplace_back();
        //if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
        if(!(*iap_it)->imagecoll.Process_Images( GroupIndividualImages,
                                                 PartitionedImageVoxelVisitorMutator,
                                                 {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to harvest voxels within the specified ROI(s).");
        }
        if(voxels.back().empty()){
            voxels.pop_back();
            beam_id.pop_back();
        }
        ++iap_it;
    }

    if(voxels.empty()) throw std::domain_error("No voxels identified interior to the selected ROI(s). Cannot continue.");

    auto same_size = [&](const std::vector<double> &v){
        return (voxels.front().size() == v.size());
    };
    if(! std::all_of(voxels.begin(), voxels.end(), same_size) ){
        throw std::domain_error("Dose matrices do not align. Cannot continue.");
        // Note: this is a reasonable scenario, but not currently supported. If needed you could try: resampling
        //       or resizing all matrices, implementing a grid-independent sampling routine for this operation, or
        //       using a dose meld + zeroing-out dose matrices to get a common grid.
    }

    //Reduce the number of voxels by randomly trimming until a small, hopefully representative collection remain.
    const long int N_voxels_max = 3000;
    const long int random_seed = 123456;
    std::mt19937 re_orig( random_seed );
    {
        for(auto &vec : voxels){
            auto re = re_orig;
            std::shuffle(vec.begin(), vec.end(), re);
        }
        if(voxels.front().size() > N_voxels_max){
            for(auto &vec : voxels){
                vec.resize( N_voxels_max );
            }
        }
    }

    const auto N_beams = static_cast<long int>(voxels.size());
    const auto N_voxels = voxels.front().size();

    const double W_min = 0.0; // Inclusive weighting endpoints for individual beams.
    const double W_max = 1.0;

    // What the sum of all weights should be. Need NOT be within [W_min,W_max], but will result in combinations that are
    // artificially excluded by the per-beam W_min and W_max limits if outside. This can be problematic when weights are
    // renormalized afterward (because some renormalizations may make invalid combinations valid).
    const double W_target = 1.0; 

    const long int W_N = 100 + 1; // The number of weights to try for each beam. Defines the search grid size.
    const double dW = (W_max - W_min) / static_cast<double>(W_N - 1);

    const long int W_budget = static_cast<long int>( std::round((W_target - W_min)/dW) );

    std::stringstream ss;

    double current_best = 0.0;
    std::vector<double> best_weights;

    // Recursive combinatoric generator for candidate weights. This routine generates candidate weightings and then
    // invokes an evaluation routine to validate them.
    //
    // Note: This routine enumerates points on the surface of an N-cross-polytope, which is a surface defined by the
    //       taxicab distance on an integer grid. The grid is a discretization of beam weights, and the N-cross-polytope
    //       surface is bounded such that the sum of all weights is a constant. 
    std::function<void(std::vector<double> &,
                       std::vector<double> &,
                       const std::function<void(std::vector<double> &, std::vector<double> &)> &,
                       long int,
                       long int )> cycle_thru_weights;
    cycle_thru_weights = [&](std::vector<double> &weights,
                             std::vector<double> &working,
                             const std::function<void(std::vector<double> &, std::vector<double> &)> &f,
                             long int beam,
                             long int remaining_budget ) -> void {
        if(remaining_budget < 0) return; // Nothing left to do, so do not recurse.

        if(beam >= N_beams){
            if(remaining_budget == 0) f(weights, working);
        }else{
            for(long int i = 0; i < W_N; ++i){
                weights[beam] = W_min + dW * i;
                const auto new_budget = remaining_budget - i;
                cycle_thru_weights(weights, working, f, beam+1, new_budget);
            }
        }
        return;
    };
            
    // This routine evaluates a given weighting scheme.
    auto evaluate_weights = [&,N_beams](std::vector<double> &weights, std::vector<double> &working) -> void {
        //Compute the total dose using the current weighting scheme.
        std::fill(working.begin(), working.end(), 0.0);
        for(long int beam = 0; beam < N_beams; ++beam){
            const auto weight = weights[beam];
            std::transform( working.begin(), working.end(),
                            voxels[beam].begin(), 
                            working.begin(),
                            [=](const double &L, const double &R){ return L + weight * R; });
        }

        //Compute the D_max.
        const auto D_max = Stats::Max(working);
        if(!std::isfinite(D_max) || (D_max < 1E-3)) return;

        //Scale so the D_max is the maximum permissable.
        {
            const double scale = 107.0 / D_max;
            for(auto &D : working) D *= scale;
        }

        //Compute the number of voxels above the Xth percentile.
        const long int N_above = std::count_if(working.begin(), working.end(), 
                                         [=](const double &D){ return (D >= 95.0); });

        std::lock_guard<std::mutex> lock(common_access);

        //Evaluate if this is better than the previous best.
        if(N_above > current_best){
            current_best = N_above;
            best_weights = weights;
        }

        //Record the results.
        for(long int beam = 0; beam < N_beams; ++beam){
            const auto weight = weights[beam];
            ss << weight << ",";
        }
        ss << std::accumulate(weights.begin(), weights.end(), 0.0) << ",";
        ss << N_above << std::endl;
    };

    //Launch in serial.
    if(false){
        std::vector<double> working(N_voxels, 0.0);
        std::vector<double> weights(N_beams, 0.0);
        cycle_thru_weights(weights, working, evaluate_weights, 0, W_budget);
    }

    //Launch in parallel.
    if(true){
        const long int beam = 0;
        std::mutex printer;
        long int counter = 0;

        FUNCINFO("Launching thread pool");
        asio_thread_pool tp;
        for(long int i = 0; i < W_N; ++i){
            tp.submit_task([&,i,N_voxels,N_beams](void) -> void {
                std::vector<double> working(N_voxels, 0.0);
                std::vector<double> weights(N_beams, 0.0);

                weights[beam] = W_min + dW * i;
                cycle_thru_weights(weights, working, evaluate_weights, beam+1, W_budget-i);

                {
                    std::lock_guard<std::mutex> lock(printer);
                    ++counter;
                    FUNCINFO("Completed " << counter << " of " << W_N << " --> " 
                      << static_cast<long int>(1000.0*(counter)/W_N)/10.0 << "\% done");
                }
            }); // thread pool task closure.
        }
    }


    std::cout << "The best weights are: " << std::endl;
    for(size_t i = 0; ( (i < best_weights.size()) && (i < beam_id.size()) ); ++i){
        std::cout << beam_id[i] << ": " << best_weights[i] << std::endl;
    }
    std::cout << std::endl;

    std::cout << "At best, " << current_best << " out of " << N_voxels 
              << " (" << static_cast<long int>(1000.0*(current_best)/N_voxels)/10.0 << "%)"
              << " are above 95\%" 
              << std::endl;


    //Write the full report.
    FUNCINFO("Attempting to claim a mutex");
    try{
        auto gen_filename = [&](void) -> std::string {
            if(FullResultsFileName.empty()){
                FullResultsFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_staticbeamoptifull_", 6, ".csv");
            }
            return FullResultsFileName;
        };

        std::stringstream header;
        header << "Weights,Number_of_voxels_above_X" << std::endl;

        Append_File( gen_filename,
                     "dicomautomaton_operation_analyzepicketfence_mutex",
                     header.str(),
                     ss.str() );

    }catch(const std::exception &e){
        FUNCERR("Unable to write to output file: '" << e.what() << "'");
    }

/*
    //Write the full report.
    FUNCINFO("Attempting to claim a mutex");
    try{
        auto gen_filename = [&](void) -> std::string {
            if(ResultsSummaryFileName.empty()){
                ResultsSummaryFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_pfsummary_", 6, ".csv");
            }
            return ResultsSummaryFileName;
        };

        std::stringstream header;
        header << "Quantity,"
               << "Result"
               << std::endl;

        std::stringstream body;
        body << "Patient ID,"
             << PatientID
             << std::endl;
        body << "Detected uncompensated rotation (degrees),"
             << PFC.CollimatorCompensation
             << std::endl;

        Append_File( gen_filename,
                     "dicomautomaton_operation_analyzepicketfence_mutex",
                     header.str(),
                     body.str() );

    }catch(const std::exception &e){
        FUNCERR("Unable to write to output file: '" << e.what() << "'");
    }
*/

    return DICOM_data;
}
