//ExtractRadiomicFeatures.cc - A part of DICOMautomaton 2018. Written by hal clark.

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
#include <vector>
#include <utility>

#ifdef DCMA_USE_CGAL
#else
    #error "Attempted to compile without CGAL support, which is required."
#endif

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"

#include "Explicator.h"

#include "../Insert_Contours.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Write_File.h"
#include "../Thread_Pool.h"
#include "../Surface_Meshes.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/Partitioned_Image_Voxel_Visitor_Mutator.h"

#include "ExtractRadiomicFeatures.h"

OperationDoc OpArgDocExtractRadiomicFeatures(){
    OperationDoc out;
    out.name = "ExtractRadiomicFeatures";

    out.desc = 
      "This operation extracts radiomic features from the selected images."
      " Features are implemented as per specification in the Image Biomarker Standardisation Initiative (IBSI)"
      " or pyradiomics documentation if the IBSI specification is unclear or ambiguous.";

    out.notes.emplace_back(
        "This routine is meant to be processed by an external analysis."
    );
    out.notes.emplace_back(
        "If this routine is slow, simplifying ROI contours may help speed surface-mesh-based feature extraction."
        " Often removing the highest-frequency components of the contour will help, such as edges that conform"
        " tightly to individual voxels."
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
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";


    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";


    return out;
}

Drover ExtractRadiomicFeatures(Drover DICOM_data,
                               const OperationArgPkg& OptArgs,
                               const std::map<std::string, std::string>& /*InvocationMetadata*/,
                               const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    auto FeaturesFileName = OptArgs.getValueStr("FeaturesFileName").value();

    const auto UserComment = OptArgs.getValueStr("UserComment").value_or("");

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

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
    // std::list<std::string> get_distinct_values_for_key(const std::string &akey) const;
    const auto ROINameOpt = cc_ROIs.front().get().contours.front().GetMetadataValueAs<std::string>("ROIName");  // HACK! FIXME. TODO.
    const auto& ROIName = ROINameOpt.value();


    // Contour-based features.
    std::stringstream contours_header;
    std::stringstream contours_report;
    {
        double TotalPerimeter = std::numeric_limits<double>::quiet_NaN();
        double LongestPerimeter = std::numeric_limits<double>::quiet_NaN();

        for(const auto &cc_refw : cc_ROIs){
            const auto p = cc_refw.get().Perimeter();
            if(!std::isfinite(TotalPerimeter)){
                TotalPerimeter = p;
            }else{
                TotalPerimeter += p;
            }

            const auto pl = cc_refw.get().Longest_Perimeter();
            if(!std::isfinite(LongestPerimeter)){
                LongestPerimeter = pl;
            }else{
                LongestPerimeter = std::max(LongestPerimeter, pl);
            }
        }
        contours_header << ",TotalPerimeter";
        contours_report << "," << TotalPerimeter;

        contours_header << ",LongestPerimeter";
        contours_report << "," << LongestPerimeter;

        double LongestVertVertDistance = -1.0;
        for(const auto &cc_refw : cc_ROIs){
            for(const auto &cA : cc_refw.get().contours){
                for(const auto &vA : cA.points){
                    for(const auto &cB : cc_refw.get().contours){
                        for(const auto &vB : cB.points){
                            const auto dist = vB.distance( vA );
                            if(dist > LongestVertVertDistance) LongestVertVertDistance = dist;
                        }
                    }
                }
            }
        }
        contours_header << ",LongestVertexVertexDistance";
        contours_report << "," << LongestVertVertDistance;

    }

    // Surface-mesh-based features.
    std::stringstream smesh_header;
    std::stringstream smesh_report;
    {
        dcma_surface_meshes::Parameters meshing_params;
        meshing_params.RQ = dcma_surface_meshes::ReproductionQuality::Medium;
        meshing_params.GridRows = 1024;
        meshing_params.GridColumns = 1024;
        auto smesh = dcma_surface_meshes::Estimate_Surface_Mesh( cc_ROIs, meshing_params );

        //if(!polyhedron_processing::SaveAsOFF(smesh, "/tmp/test.off")){
        //    FUNCERR("Unable to write mesh as OFF file");
        //}


        //polyhedron_processing::Subdivide(smesh, MeshSubdivisions);
        //polyhedron_processing::Simplify(smesh, MeshSimplificationEdgeCountLimit);
        //polyhedron_processing::SaveAsOFF(smesh, base_dir + "_polyhedron.off");

        const auto V = polyhedron_processing::Volume(smesh);
        smesh_header << ",MeshVolume";
        smesh_report << "," << V;

        const auto A = polyhedron_processing::SurfaceArea(smesh);
        smesh_header << ",MeshSurfaceArea";
        smesh_report << "," << A;

        const auto SA_V = A/V;
        smesh_header << ",MeshSurfaceAreaVolumeRatio";
        smesh_report << "," << SA_V;

        const auto pi = std::acos(-1.0);
        const auto Sph = std::pow(36.0 * pi * V * V, 1.0/3.0)/A;
        smesh_header << ",MeshSphericity";
        smesh_report << "," << Sph;

        const auto C = V/std::sqrt( pi * std::pow(A, 3.0) );
        smesh_header << ",MeshCompactness";
        smesh_report << "," << C;
    }


    std::mutex common_access;
    std::stringstream header;
    std::stringstream report;

    // Cycle over the Image_Arrays, processing each one-at-a-time.
    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        if((*iap_it)->imagecoll.images.empty()) throw std::invalid_argument("Unable to find an image to analyze.");

        //Determine which PatientID(s) to report.
        const auto PatientIDs = (*iap_it)->imagecoll.get_distinct_values_for_key("PatientID");
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

        Mutate_Voxels_Functor<float,double> f_noop;
        ud.f_unbounded = f_noop;
        ud.f_visitor = f_noop;
        ud.f_bounded = [&](long int /*row*/, 
                           long int /*col*/, 
                           long int /*chan*/, 
                           std::reference_wrapper<planar_image<float,double>> /*img_refw*/, 
                           std::reference_wrapper<planar_image<float,double>> /*mask_img_refw*/, 
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

/*
        //Ignore voxels outside of a specific window.
        //
        // Note: require the user to provide the window, or do not enable this step! TODO
        {
            voxel_vals.erase( std::remove_if( std::begin(voxel_vals), std::end(voxel_vals), [&](double I){
                        //Remove all intensities not within the 10th and 90th percentiles.
                        return !isininc(-500.0, I, 400.0);
                     }),
                     std::end(voxel_vals) );
        }
*/       

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


        // Add the contour- and surface-mesh-based features.
        header << contours_header.str();
        report << contours_report.str();

        header << smesh_header.str();
        report << smesh_report.str();

        header << std::endl;
        report << std::endl;
    }

    //Print the report.
    //std::cout << report.str();

    //Write the report to file.
    try{
        auto gen_filename = [&]() -> std::string {
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
