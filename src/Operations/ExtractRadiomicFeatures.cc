//ExtractRadiomicFeatures.cc - A part of DICOMautomaton 2018. Written by hal clark.

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

#include "ExtractRadiomicFeatures.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.


OperationDoc OpArgDocExtractRadiomicFeatures(void){
    OperationDoc out;
    out.name = "ExtractRadiomicFeatures";

    out.desc = 
      "This operation extracts radiomic features from the selected images."
      " Features are implemented as per specification in the Image Biomarker Standardisation Initiative (IBSI)"
      " or pyradiomics documentation if the IBSI specification is unclear or ambiguous.";

    out.notes.emplace_back(
        "This routine is meant to be processed by an external analysis."
    );


    out.args.emplace_back();
    out.args.back().name = "UserComment";
    out.args.back().desc = "A string that will be inserted into the output file which will simplify merging output"
                           " with differing parameters, from different sources, or using sub-selections of the data.";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "", "Using XYZ", "Patient treatment plan C" };


    out.args.emplace_back();
    out.args.back().name = "FeaturesFileName";
    out.args.back().desc = "Features will be appended to this file."
                           " The format is CSV. Leave empty to dump to generate a unique temporary file."
                           " If an existing file is present, rows will be appended without writing a header.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile", "localfile.csv", "derivative_data.csv" };
    out.args.back().mimetype = "text/csv";


    out.args.emplace_back();
    out.args.back().name = "ImageSelection";
    out.args.back().desc = "Image arrays to operate on. Either 'none', 'last', 'first', or 'all'.";
    out.args.back().default_val = "last";
    out.args.back().expected = true;
    out.args.back().examples = { "none", "last", "first", "all" };


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

Drover ExtractRadiomicFeatures(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    auto FeaturesFileName = OptArgs.getValueStr("FeaturesFileName").value();

    const auto UserComment = OptArgs.getValueStr("UserComment").value_or("");

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_none  = std::regex("^no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_first = std::regex("^fi?r?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last  = std::regex("^la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all   = std::regex("^al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    if( !std::regex_match(ImageSelectionStr, regex_none)
    &&  !std::regex_match(ImageSelectionStr, regex_first)
    &&  !std::regex_match(ImageSelectionStr, regex_last)
    &&  !std::regex_match(ImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Image selection is not valid. Cannot continue.");
    }

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );

    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    // TODO: extract all ROI names and append them together. Best to put it in a top-level file for access elsehwere
    // too.
    // std::list<std::string> get_unique_values_for_key(const std::string &akey) const;
    const auto ROINameOpt = cc_ROIs.front().get().contours.front().GetMetadataValueAs<std::string>("ROIName");  // HACK! FIXME. TODO.
    const auto ROIName = ROINameOpt.value();



    std::mutex common_access;
    std::stringstream header;
    std::stringstream report;

    // Cycle over the Image_Arrays, processing each one-at-a-time.
    auto iap_it = DICOM_data.image_data.begin();
    if(false){
    }else if(std::regex_match(ImageSelectionStr, regex_none)){ iap_it = DICOM_data.image_data.end();
    }else if(std::regex_match(ImageSelectionStr, regex_last)){
        if(!DICOM_data.image_data.empty()) iap_it = std::prev(DICOM_data.image_data.end());
    }
    while(iap_it != DICOM_data.image_data.end()){
        if((*iap_it)->imagecoll.images.empty()) throw std::invalid_argument("Unable to find an image to analyze.");

        //Determine which PatientID(s) to report.
        const auto PatientIDs = (*iap_it)->imagecoll.get_unique_values_for_key("PatientID");
        std::string PatientID;
        if(PatientIDs.empty()){
            PatientID = "Unknown";
        }else if(PatientIDs.size() == 1){
            PatientID = PatientIDs.front();
        }else if(PatientIDs.size() == 1){
            PatientID = std::accumulate(std::begin(PatientIDs), std::end(PatientIDs), std::string(), 
                            [](const std::string &run, const std::string &id){
                                return (run + (run.empty() ? "" : "_") + id);
                            });
        }

        std::vector<double> voxel_vals;

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
        ud.f_bounded = [&](long int row, 
                           long int col, 
                           long int chan, 
                           float &voxel_val) {
            // Append the value to the voxel store.
            voxel_vals.emplace_back(voxel_val);

            // Append the value rounded to the nearest integer to the voxel store.
            //voxel_vals.emplace_back( static_cast<long int>( std::round(voxel_val) ) );
            return;
        };

        if(!(*iap_it)->imagecoll.Process_Images( GroupIndividualImages,
                                                 PartitionedImageVoxelVisitorMutator,
                                                 {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to harvest voxels within the specified ROI(s).");
        }

        // Process the voxel data.
        if(voxel_vals.empty()){
            throw std::domain_error("No voxels identified interior to the selected ROI(s). Cannot continue.");
        }

        //Resegment the voxel values to a fixed window for comparison with IBSI benchmarks.
        {
            voxel_vals.erase( std::remove_if( std::begin(voxel_vals), std::end(voxel_vals), [&](double I){
                        //Remove all intensities not within the 10th and 90th percentiles.
                        return !isininc(-500.0, I, 400.0);
                     }),
                     std::end(voxel_vals) );
        }
FUNCERR("This commit was saved for reference purposes and must be edited to proceed");


        //Reset the header.
        {
            std::stringstream dummy;
            header = std::move(dummy);
        }

        const auto N_I    = static_cast<double>(voxel_vals.size());
        const auto I_min  = Stats::Min(voxel_vals);
        const auto I_max  = Stats::Max(voxel_vals);
        const auto I_mean = Stats::Mean(voxel_vals);
        const auto I_02   = Stats::Percentile(voxel_vals, 0.02);
        const auto I_05   = Stats::Percentile(voxel_vals, 0.05);
        const auto I_10   = Stats::Percentile(voxel_vals, 0.10);
        const auto I_25   = Stats::Percentile(voxel_vals, 0.25);
        const auto I_50   = Stats::Percentile(voxel_vals, 0.50);
        const auto I_75   = Stats::Percentile(voxel_vals, 0.75);
        const auto I_90   = Stats::Percentile(voxel_vals, 0.90);
        const auto I_95   = Stats::Percentile(voxel_vals, 0.95);
        const auto I_98   = Stats::Percentile(voxel_vals, 0.98);

        // Patient metadata.
        {
            header << "PatientID";
            report << PatientID;
            
            header << ",ROIName";
            report << "," << ROIName;
            
            header << ",UserComment";
            report << "," << UserComment;
        }


        // Simple first-order statistics and derived quantities.
        {
            header << ",Min";
            report << "," << I_min;

            header << ",Percentile02";
            report << "," << I_02;

            header << ",Percentile05";
            report << "," << I_05;

            header << ",Percentile10";
            report << "," << I_10;

            header << ",Percentile25";
            report << "," << I_25;

            header << ",Mean";
            report << "," << I_mean;

            header << ",Median";
            report << "," << I_50;

            header << ",Percentile75";
            report << "," << I_75;

            header << ",Percentile90";
            report << "," << I_90;

            header << ",Percentile95";
            report << "," << I_95;

            header << ",Percentile98";
            report << "," << I_98;

            header << ",Max";
            report << "," << I_max;

            header << ",InterQuartileRange";
            report << "," << (I_75 - I_25);

            header << ",Range";
            report << "," << (I_max - I_min);

/*
            header << ",";
            report << "," << ;
*/

        }

        // Deviations.
        {
            const auto Var = std::accumulate(
                               std::begin(voxel_vals), std::end(voxel_vals),
                               static_cast<double>(0),
                               [&](double run, double I) -> double {
                                   return run + (I - I_mean) * (I - I_mean);
                               }) / N_I;
                                
            header << ",Variance";
            report << "," << Var;

            const auto StdDev = std::sqrt(Var);

            header << ",StandardDeviation";
            report << "," << StdDev;


            const auto CM3 = std::accumulate(
                               std::begin(voxel_vals), std::end(voxel_vals),
                               static_cast<double>(0),
                               [&](double run, double I) -> double {
                                   return run + (I - I_mean) * (I - I_mean) * (I - I_mean);
                               }) / N_I;
                                
            const auto CM4 = std::accumulate(
                               std::begin(voxel_vals), std::end(voxel_vals),
                               static_cast<double>(0),
                               [&](double run, double I) -> double {
                                   return run + (I - I_mean) * (I - I_mean) * (I - I_mean) * (I - I_mean);
                               }) / N_I;
                                
            const auto CV = StdDev / I_mean;

            header << ",CoefficientOfVariation";
            report << "," << CV;

            const auto Skewness = CM3 / (StdDev * StdDev * StdDev);

            header << ",Skewness";
            report << "," << Skewness;

            const auto pSkewness = 3.0 * (I_mean - I_50) / StdDev;

            header << ",PearsonsMedianSkewness"; // Also known as Pearson's non-parametric second skewness coefficient.
            report << "," << pSkewness;

            const auto Kurtosis = CM4 / (StdDev * StdDev * StdDev * StdDev);

            header << ",Kurtosis";
            report << "," << Kurtosis;

            const auto eKurtosis = Kurtosis - 3.0;

            header << ",ExcessKurtosis";
            report << "," << eKurtosis;

            const auto MAD = std::accumulate(
                               std::begin(voxel_vals), std::end(voxel_vals),
                               static_cast<double>(0),
                               [&](double run, double I) -> double {
                                   return run + std::abs(I - I_mean);
                               }) / N_I;
                                
            header << ",MeanAbsoluteDeviation";
            report << "," << MAD;

            auto v = voxel_vals; // A vector comprised only of the inner 10th-90th percentile data.
            v.erase( std::remove_if( std::begin(v), std::end(v), [&](double I){
                        //Remove all intensities not within the 10th and 90th percentiles.
                        return !isininc(I_10, I, I_90);
                     }),
                     std::end(v) );

            const auto v_N_I    = static_cast<double>(v.size());
            const auto v_I_mean = Stats::Mean(v);
            const auto rMAD = std::accumulate(
                               std::begin(v), std::end(v),
                               static_cast<double>(0),
                               [&](double run, double I) -> double {
                                   return run + std::abs(I - v_I_mean);
                               }) / v_N_I;
                                
            header << ",RobustMeanAbsoluteDeviation";
            report << "," << rMAD;
        }

        // Pixel intensity 'image energy.' Also a shifted energy with voxel intensities translated so the smallest voxel
        // intensity contributes zero energy.
        {
            const auto E = std::accumulate(std::begin(
                               voxel_vals), std::end(voxel_vals),
                               static_cast<double>(0),
                               [&](double run, double I) -> double {
                                   return run + I*I;
                               });
                                
            header << ",IntensityEnergy";
            report << "," << E;

            const auto RMSI = std::sqrt(1.0*E/N_I);
            header << ",RootMeanSquaredIntensity";
            report << "," << RMSI;

            const auto E_shifted = std::accumulate(std::begin(
                               voxel_vals), std::end(voxel_vals),
                               static_cast<double>(0),
                               [&](double run, double I) -> double {
                                   return run + (I + I_min) * (I + I_min);
                               });
                                
            header << ",ShiftedIntensityEnergy";
            report << "," << E_shifted;

            const auto RMSI_shifted = std::sqrt(1.0*E_shifted/N_I);
            header << ",ShiftedRootMeanSquaredIntensity";
            report << "," << RMSI_shifted;
        }







        ++iap_it;
        if(std::regex_match(ImageSelectionStr, regex_first)) break;
    }



/*
            const auto weight = weights[beam];
            std::transform( working.begin(), working.end(),
                            voxels[beam].begin(), 
                            working.begin(),
                            [=](const double &L, const double &R){ return L + weight * R; });

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


        std::vector<double> weights(open_weights);
        const auto sum = std::accumulate(weights.begin(), weights.end(), 0.0);
        std::transform(weights.begin(), weights.end(), weights.begin(), [=](double ow) -> double { return ow / sum; });

    std::vector<double> weights(open_weights);
    const auto sum = std::accumulate(weights.begin(), weights.end(), 0.0);
    std::transform(weights.begin(), weights.end(), 
                   weights.begin(), [=](double ow) -> double { return ow / sum; });



    // Construct a report.
    report << "The best weights are: " << std::endl;
    for(size_t i = 0; ( (i < weights.size()) && (i < beam_id.size()) ); ++i){
        report << beam_id[i] << ": " << weights[i] << std::endl;
    }
    report << std::endl;
    
    report << "# of voxels = " << N_voxels << std::endl
            << "# of beams  = " << N_beams << std::endl
            << std::endl;

    report << "D_min  = " << res.D_min << std::endl
            << "D_mean = " << res.D_mean << std::endl
            << "D_max  = " << res.D_max << std::endl
            << std::endl;

    report << "D_02   = " << res.D_02 << std::endl
            << "D_05   = " << res.D_05 << std::endl
            << "D_50   = " << res.D_50 << std::endl
            << "D_95   = " << res.D_95 << std::endl
            << "D_98   = " << res.D_98 << std::endl
            << std::endl;

    report << "D_min -- D_max span = " << std::abs(res.D_min - res.D_max) << std::endl
            << "D_02  -- D_98  span = " << std::abs(res.D_02  - res.D_98) << std::endl
            << "D_05  -- D_95  span = " << std::abs(res.D_05  - res.D_95) << std::endl
            << std::endl;

    report << "cost   = " << res.cost << std::endl
            << std::endl;
*/


    //Finalize the report.
    header << std::endl;
    report << std::endl;

    //Print the report.
    //std::cout << report.str();

    //Write the report to file.
    try{
        auto gen_filename = [&](void) -> std::string {
            if(FeaturesFileName.empty()){
                FeaturesFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_extractradiomicfeatures_", 6, ".csv");
            }
            return FeaturesFileName;
        };

        FUNCINFO("About to claim a mutex");
        Append_File( gen_filename,
                     "dicomautomaton_operation_extractradiomicfeatures_mutex",
                     header.str(),
                     report.str() );

    }catch(const std::exception &e){
        FUNCERR("Unable to write to output file: '" << e.what() << "'");
    }

    return DICOM_data;
}
