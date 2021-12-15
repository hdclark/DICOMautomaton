//AnalyzeLightRadFieldCoincidence.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <exception>
#include <optional>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>
#include <filesystem>

#include "../Insert_Contours.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "AnalyzeLightRadFieldCoincidence.h"
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#if defined(YGOR_USE_GNU_GSL) && defined(DCMA_USE_GNU_GSL)
    #include "YgorMathBSpline.h" //Needed for basis_spline class.
#endif
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for GetFirstRegex(...)


OperationDoc OpArgDocAnalyzeLightRadFieldCoincidence(){
    OperationDoc out;
    out.name = "AnalyzeLightRadFieldCoincidence";

    out.desc = 
        "This operation analyzes the selected images to compare light and radiation field coincidence for fixed, symmetric"
        " field sizes. Coincidences are extracted automatically by fitting Gaussians to the peak nearest to one of the"
        " specified field boundaries and comparing offset from one another. So, for example, a 10x10cm MLC-defined field"
        " would be compared to a 15x15cm field if there are sharp edges (say, metal rulers) that define a 10x10cm field"
        " (i.e., considered to represent the light field). Horizontal and vertical directions (both positive and negative)"
        " are all analyzed separately.";
        
    out.notes.emplace_back(
        "This routine assumes both fields are squarely aligned with the image axes. Alignment need not be perfect,"
        " but the Gaussians may be significantly broadened if there is misalignment. This should be fixed in a future"
        " revision."
    );
        
    out.notes.emplace_back(
        "It is often useful to pre-process inputs by computing an in-image-plane derivative, gradient magnitude, or"
        " similar (i.e., something to emphasize edges) before calling this routine. It may not be necessary, however."
    );
        

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back().name = "ToleranceLevel";
    out.args.back().desc = "Controls detected edge visualization for easy identification of edges out of tolerance."
                      " Note: this value refers to edge-to-edge separation, not edge-to-nominal distances."
                      " This value is in DICOM units.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.5", "1.0", "2.0", "inf" };


    out.args.emplace_back();
    out.args.back().name = "EdgeLengths";
    out.args.back().desc = "Comma-separated list of (symmetric) edge lengths fields should be analyzed at."
                      " For example, if 50x50, 100x100, 150x150, and 200x200 (all in mm) fields are to be analyzed,"
                      " this argument would be '50,100,150,200' and it will be assumed that the field centre"
                      " is at DICOM position (0,0,0). All values are in DICOM units.";
    out.args.back().default_val = "100";
    out.args.back().expected = true;
    out.args.back().examples = { "100.0", "50,100,150,200,300", "10.273,20.2456" };


    out.args.emplace_back();
    out.args.back().name = "SearchDistance";
    out.args.back().desc = "The distance around the anticipated field edges to search for edges (actually sharp peaks"
                      " arising from edges). If an edge is further away than this value from the anticipated field"
                      " edge, then the coincidence will be ignored altogether. The value should be greater than"
                      " the largest action/tolerance threshold with some additional margin (so gross errors can"
                      " be observed), but small enough that spurious edges (i.e., unintended features in the"
                      " image, such as metal fasteners, or artifacts near the field edge) do not replace the"
                      " true field edges. The 'sharpness' of the field edge (resulting from the density of the"
                      " material used to demarcate the edge) can impact this value; if the edge is not sharp, then"
                      " the peak will be shallow, noisy, and may therefore travel around depending on how the image"
                      " is pre-processed. Note that both radiation field and light field edges may differ from"
                      " the 'nominal' anticipated edges, so this wobble factor should be incorporated in the"
                      " search distance. This quantity must be in DICOM units.";
    out.args.back().default_val = "3.0";
    out.args.back().expected = true;
    out.args.back().examples = { "2.5", "3.0", "5.0" };


    out.args.emplace_back();
    out.args.back().name = "PeakSimilarityThreshold";
    out.args.back().desc = "Images can be taken such that duplicate peaks will occur, such as when field sizes are re-used."
                      " Peaks are therefore de-duplicated. This value (as a %, ranging from [0,100]) specifies the"
                      " threshold of disimilarity below which peaks are considered duplicates. A low value will make"
                      " duplicates confuse the analysis, but a high value may cause legitimate peaks to be discarded"
                      " depending on the attenuation cababilties of the field edge markers.";
    out.args.back().default_val = "25";
    out.args.back().expected = true;
    out.args.back().examples = { "5", "10", "15", "50" };


    out.args.emplace_back();
    out.args.back().name = "UserComment";
    out.args.back().desc = "A string that will be inserted into the output file which will simplify merging output"
                      " with differing parameters, from different sources, or using sub-selections of the data."
                      " If left empty, the column will be omitted from the output.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "6MV", "Using XYZ", "Test with thick metal edges" };


    out.args.emplace_back();
    out.args.back().name = "OutputFileName";
    out.args.back().desc = "A filename (or full path) in which to append field edge coincidence data generated by this routine."
                      " The format is CSV. Leave empty to dump to generate a unique temporary file.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile", "localfile.csv", "derivative_data.csv" };
    out.args.back().mimetype = "text/csv";


    out.args.emplace_back();
    out.args.back().name = "InteractivePlots";
    out.args.back().desc = "Whether to interactively show plots showing detected edges.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };

    return out;
}

bool AnalyzeLightRadFieldCoincidence(Drover &DICOM_data,
                                       const OperationArgPkg& OptArgs,
                                       std::map<std::string, std::string>& /*InvocationMetadata*/,
                                       const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto ToleranceLevel = std::stod( OptArgs.getValueStr("ToleranceLevel").value() );
    const auto EdgeLengthsStr = OptArgs.getValueStr("EdgeLengths").value();
    const auto SearchDistance = std::stod( OptArgs.getValueStr("SearchDistance").value() );
    const auto PeakSimilarityThreshold = std::stod( OptArgs.getValueStr("PeakSimilarityThreshold").value() );

    const auto UserComment = OptArgs.getValueStr("UserComment");
    auto OutputFileName = OptArgs.getValueStr("OutputFileName").value();
    const auto InteractivePlotsStr = OptArgs.getValueStr("InteractivePlots").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = Compile_Regex("^tr?u?e?$");

    const auto InteractivePlots = std::regex_match(InteractivePlotsStr, regex_true);

    //Convert the edge lengths into anticipated offsets from DICOM (0,0,0), which is assumed to be at the centre of the image:
    //                                       
    //                     (symmetric        
    //                       field)          
    //                     edge length       
    //                  |- - - - - - - -|    
    //                   _______________     
    //                  |               |    
    //                  |               |    
    //                  |    (0,0,0)    |    
    //                  |       x       |    
    //                  |               |    
    //                  |               |    
    //                  |_______________|    
    //                                       
    //                  |< - - -|            
    //                          |- - - >|    
    //              anticipated  anticipated 
    //               negative     positive   
    //                offset       offset    
    //                                                                                              
    const auto EdgeLengthStrs = SplitStringToVector(EdgeLengthsStr,",",'d');
    std::vector<double> AFEs; // DICOM coordinates of anticipated field edges.
    for(const auto &els : EdgeLengthStrs){
        const auto afel = std::stod( els );
        AFEs.push_back(  0.5*afel ); //Field is symmetric and DICOM (0,0,0) is at centre, so halve the edge length.
        AFEs.push_back( -0.5*afel );
    }
    if(AFEs.empty()) throw std::invalid_argument("Edge lengths not recognized. Cannot continue.");
    std::sort( AFEs.begin(), AFEs.end() );


    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );

    for(auto & iap_it : IAs){
        std::vector<YgorMathPlottingGnuplot::Shuttle<samples_1D<double>>> row_sums;
        std::vector<YgorMathPlottingGnuplot::Shuttle<samples_1D<double>>> col_sums;

        std::stringstream row_cm;
        std::stringstream col_cm;

        std::vector< std::vector<samples_1D<double>> > row_fe_candidates(AFEs.size());
        std::vector< std::vector<samples_1D<double>> > col_fe_candidates(AFEs.size());

        {
            row_cm << "Row compatibility matrix:" << std::endl;
            col_cm << "Column compatibility matrix:" << std::endl;

            for(const auto & animg : (*iap_it)->imagecoll.images){

                //Sum pixel values row- and column-wise.
                std::vector<double> row_sum(animg.rows, 0.0);
                std::vector<double> col_sum(animg.columns, 0.0);
                for(auto row = 0; row < animg.rows; ++row){
                    for(auto col = 0; col < animg.columns; ++col){
                        for(auto chan = 0; chan < animg.channels; ++chan){
                            const auto val = animg.value(row, col, chan);
                            row_sum[row] += val;
                            col_sum[col] += val;
                        }//Loop over channels.
                    } //Loop over cols
                } //Loop over rows

                //Record the data in the form of comparative plots.
                {
                    //Extract row and column profiles by projecting the accumulated values back.
                    samples_1D<double> row_profile;
                    for(auto row = 0; row < animg.rows; ++row){
                        const auto pos = animg.position(row,0).Dot(animg.row_unit); //Relative to DICOM origin.
                        row_profile.push_back({ pos, 0.0, row_sum[row], 0.0 });
                    }
                    samples_1D<double> col_profile;
                    for(auto col = 0; col < animg.columns; ++col){
                        const auto pos = animg.position(0,col).Dot(animg.col_unit); //Relative to DICOM origin.
                        col_profile.push_back({ pos, 0.0, col_sum[col], 0.0 });
                    }

                    // Smooth profiles to make peak detection more robust.
                    row_profile = row_profile.Moving_Average_Two_Sided_Spencers_15_point();
                    col_profile = col_profile.Moving_Average_Two_Sided_Spencers_15_point();

                    //Perform a high-pass filter to remove some beam profile and imager bias dependence.
                    //
                    // Note: This can be problematic in the vicinity of large, sharp peaks, especially if they're near
                    //       the domain extrema. 
                    //row_profile = row_profile.Subtract(row_profile.Moving_Average_Two_Sided_Gaussian_Weighting(75) );
                    //col_profile = col_profile.Subtract(col_profile.Moving_Average_Two_Sided_Gaussian_Weighting(75) );

                    //Normalize so the self-overlap integral is zero.
                    if(false){
                        row_profile.Normalize_wrt_Self_Overlap();
                        col_profile.Normalize_wrt_Self_Overlap();
                    }

                    //Normalize for display purposes.
                    if(false){
                        //const auto row_area = row_profile.Integrate_Over_Kernel_unit()[0];
                        //const auto col_area = col_profile.Integrate_Over_Kernel_unit()[0];
                        const auto row_mm = row_profile.Get_Extreme_Datum_y();
                        const auto col_mm = col_profile.Get_Extreme_Datum_y();
                        const auto row_min = row_mm.first[2];
                        const auto col_min = col_mm.first[2];
                        const auto row_max = row_mm.second[2];
                        const auto col_max = col_mm.second[2];

                        row_profile = row_profile.Sum_With(-row_min);
                        col_profile = col_profile.Sum_With(-col_min);
                        row_profile = row_profile.Multiply_With(1.0/(row_max - row_min));
                        col_profile = col_profile.Multiply_With(1.0/(col_max - col_min));
                    }

                    //Now find all (local) peaks via the derivative of the crossing points.
                    //
                    // Note: We find peaks (gaps between leaves) instead of troughs (middle of leaves) because the background (i.e.,
                    //       dose behind the jaws) confounds trough isolation for leaves on the boundaries. Peaks are also sharper
                    //       (i.e., more confined spatially owing to the smaller gap they arise from), whereas troughs can undulate
                    //       considerably more.
                    auto all_row_peaks = row_profile.Peaks();
                    auto all_col_peaks = col_profile.Peaks();

                    //Rank the peaks.
                    all_row_peaks = all_row_peaks.Multiply_With(-1.0).Rank_y();
                    all_col_peaks = all_col_peaks.Multiply_With(-1.0).Rank_y();


                    // Find the most appropriate profile that corresponds to each anticipated field edge.
                    // This is tricky because each profile could have a peak near several anticipated field edges.
                    // We therefore 'vote' on which profile 'owns' the particular field edge, constructing a matrix of
                    // votes and trying to ensure that each field edge is accounted for once.

                    for(size_t i = 0; i < AFEs.size(); ++i){
                        const auto afe = AFEs[i];
                        auto row_subset = row_profile.Select_Those_Within_Inc( afe-SearchDistance, afe+SearchDistance );
                        auto col_subset = col_profile.Select_Those_Within_Inc( afe-SearchDistance, afe+SearchDistance );

                        //Check if this is a 'major' peak, or a spurious peak arising from noise.
                        //
                        // Note: I'm lax here to account for noisy peaks. Also remember that the curves are ~symmetric,
                        //       and that the edges can give rise to large, spurious peaks.
                        const auto is_major_row_peak = (all_row_peaks.Interpolate_Linearly(afe)[2] <= 10);
                        const auto is_major_col_peak = (all_col_peaks.Interpolate_Linearly(afe)[2] <= 10);

                        decltype(row_subset.Peaks()) row_peaks;
                        decltype(row_subset.Peaks()) col_peaks;

                        //Ensure the curve spanned the full distance and are a major peak for the curve.
                        if(is_major_row_peak 
                        && !row_subset.empty()){
                            auto rmm = row_subset.Get_Extreme_Datum_x();
                            const auto x_span = std::abs(rmm.second[0] - rmm.first[0]);
                            if(std::abs(PERCENT_ERR(2.0*SearchDistance, x_span)) < 10.0){
                                row_peaks = row_subset.Peaks();
                            }
                        }
                        if(is_major_col_peak
                        && !col_subset.empty()){
                            auto rmm = col_subset.Get_Extreme_Datum_x();
                            const auto x_span = std::abs(rmm.second[0] - rmm.first[0]);
                            if(std::abs(PERCENT_ERR(2.0*SearchDistance, x_span)) < 10.0){
                                col_peaks = col_subset.Peaks();
                            }
                        }

                        //There should only be 1-2 peaks within the anticipated field edge zone.
                        // If there are more, they're probably just noise. But better to filter them out later.
                        if(row_peaks.size() >= 1){
                            row_fe_candidates[i].emplace_back(row_subset);
                        }
                        if(col_peaks.size() >= 1){
                            col_fe_candidates[i].emplace_back(col_subset);
                        }

                        row_cm << !!(!row_peaks.empty()) << " ";
                        col_cm << !!(!col_peaks.empty()) << " ";

                    }
                    row_cm << std::endl;
                    col_cm << std::endl;
                }
            }
        }

        //Identify the largest image. It is used later to overlay contours of peak locations.
        planar_image<float,double> *largest_img = nullptr;
        {
            for(auto & animg : (*iap_it)->imagecoll.images){
                if( (largest_img == nullptr)
                ||  (largest_img->rows < animg.rows) ){
                    largest_img = &animg;
                }
            }
        }


        //Report the row and column compatability matrices.
        {
            std::cout << row_cm.str() << std::endl;
            std::cout << col_cm.str() << std::endl;
        }


        //Field edge peak aspect-ratio filtering and ranking.
        //
        // Spurious peaks are often shallow peaks. The most appropriate peaks are often the sharpest, so we rank based
        // on aspect ratio. 
        //
        // Note: It is common to have duplicate field edges (double exposures, duplicate radiation field shape for
        //       smaller light field images) and so near-duplicates are filtered out.
        {
            auto Aspect_Ratio = [](const samples_1D<double> &A) -> double {
                const auto A_extrema_x = A.Get_Extreme_Datum_x();
                const auto A_extrema_y = A.Get_Extreme_Datum_y();
                const auto A_aspect_r = (A_extrema_y.second[2] - A_extrema_y.first[2])/(A_extrema_x.second[0] - A_extrema_x.first[0]);
                return A_aspect_r;
            };

            auto Sort_and_Rank = [&Aspect_Ratio,&PeakSimilarityThreshold]( std::vector<samples_1D<double>> &candidates ) -> void {
                //Sort by aspect ratio (largest first).
                std::sort(candidates.begin(), candidates.end(), [&Aspect_Ratio](const samples_1D<double> &A, const samples_1D<double> &B) -> bool {
                    return ( Aspect_Ratio(A) > Aspect_Ratio(B) );
                });

                //Remove near-duplicates.
                auto last = std::unique(candidates.begin(), candidates.end(), [&Aspect_Ratio,&PeakSimilarityThreshold](const samples_1D<double> &A, const samples_1D<double> &B) -> bool {
                    //Note: Currently using percent-difference. Should we use integral-based overlap? Probably more reliable...
                    const auto A_ar = Aspect_Ratio(A);
                    const auto B_ar = Aspect_Ratio(B);
                    return (std::abs(PERCENT_ERR(A_ar, B_ar)) < PeakSimilarityThreshold);
                });
                candidates.erase(last, candidates.end());

                //Keep only the top 2 (or maybe few, if the image is noisy?).
                while(candidates.size() > 2){
                    candidates.pop_back();
                }
                if(candidates.size() != 2){
                    throw std::runtime_error("Unable to find peak coincidence. Are your criteria too stringent? Are your field edges bright and clearly visible?");
                }
            };
            for(size_t i = 0; i < AFEs.size(); ++i){
                Sort_and_Rank(row_fe_candidates[i]);
                Sort_and_Rank(col_fe_candidates[i]);
            }

        }


        //Prepare plots of the field edges.
        {
            for(size_t i = 0; i < AFEs.size(); ++i){
                for(auto &profile : row_fe_candidates[i]) row_sums.emplace_back(profile, "");
                for(auto &profile : col_fe_candidates[i]) col_sums.emplace_back(profile, "");
            }
        }

        //Analyze the field edge coincidences.
        {
            auto Find_Peak_Nearest = [](samples_1D<double> &s, double target_x) -> double {
                double peak_x = std::numeric_limits<double>::quiet_NaN();

#if defined(YGOR_USE_GNU_GSL) && defined(DCMA_USE_GNU_GSL)
                // Estimate the (highest) peak location by scanning through a basis spline approximation.
                {
                    basis_spline bs(s);
                    const auto dx = 0.001;
                    const auto min_x = s.Get_Extreme_Datum_x().first[0] + dx;
                    const auto max_x = s.Get_Extreme_Datum_x().second[0] - dx;

                    auto max_f = -(std::numeric_limits<double>::infinity());
                    for(auto x = min_x ; x < max_x; x += dx){
                        const auto f = bs.Sample(x)[2];
                        if(f > max_f){
                            max_f = f;
                            peak_x = x;
                        }
                    }
                }
#else
                //Peak-based. No functions are fit here -- the field edge peak locations are directly used.
                {
                    FUNCWARN("Using inferior peak detection routine due to inaccessible GNU GSL functionality");
                    const auto peaks = s.Peaks();
                    auto min_dist = std::numeric_limits<double>::infinity();
                    for(const auto &samp : peaks.samples){
                        const auto x = samp[0];
                        const auto dist = std::abs(target_x - x);
                        if(dist < min_dist){
                            min_dist = dist;
                            peak_x = x;
                        }
                    }
                }
#endif

                return peak_x;
            };


            //Report the findings. 
            FUNCINFO("Attempting to claim a mutex");
            {
                //File-based locking is used so this program can be run over many patients concurrently.
                //
                //Try open a named mutex. Probably created in /dev/shm/ if you need to clear it manually...
                boost::interprocess::named_mutex mutex(boost::interprocess::open_or_create,
                                                       "dcma_op_analyzelightradcoincidence_mutex");
                boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(mutex);

                DICOM_data.contour_data->ccs.emplace_back();

                if(OutputFileName.empty()){
                    const auto base = std::filesystem::temp_directory_path() / "dcma_analyzelightradcoincidence_";
                    OutputFileName = Get_Unique_Sequential_Filename(base.string(), 6, ".csv");
                }
                const auto FirstWrite = !Does_File_Exist_And_Can_Be_Read(OutputFileName);
                std::fstream FO(OutputFileName, std::fstream::out | std::fstream::app);
                if(!FO){
                    throw std::runtime_error("Unable to open file for reporting results. Cannot continue.");
                }
                if(FirstWrite){ // Write a CSV header.
                    FO << "Nominal field boundary"
                       << ",Direction"
                       << ",User comment"
                       << ",Edge separation"
                       << ",Distance to nominal (1)"
                       << ",Distance to nominal (2)"
                       << std::endl;
                }
                for(size_t i = 0; i < AFEs.size(); ++i){
                    const auto afe = AFEs[i];
                    const auto rfe1 = Find_Peak_Nearest(row_fe_candidates[i][0], afe);
                    const auto rfe2 = Find_Peak_Nearest(row_fe_candidates[i][1], afe);
                    const auto cfe1 = Find_Peak_Nearest(col_fe_candidates[i][0], afe);
                    const auto cfe2 = Find_Peak_Nearest(col_fe_candidates[i][1], afe);

                    if(largest_img != nullptr){
                        auto m = largest_img->metadata;
                        const auto ru = largest_img->row_unit;
                        const auto cu = largest_img->col_unit;

                        const auto drfe = std::abs(rfe1-rfe2);
                        const auto dcfe = std::abs(cfe1-cfe2);

                        if(drfe < ToleranceLevel){
                            m["OutlineColour"] = "blue";
                        }else{
                            m["OutlineColour"] = "red";
                        }
                        Inject_Thin_Line_Contour(*largest_img,
                                                 line<double>(ru*rfe1, ru*rfe1 + cu),
                                                 DICOM_data.contour_data->ccs.back(),
                                                 m);
                        Inject_Thin_Line_Contour(*largest_img,
                                                 line<double>(ru*rfe2, ru*rfe2 + cu),
                                                 DICOM_data.contour_data->ccs.back(),
                                                 m);
                        if(dcfe < ToleranceLevel){
                            m["OutlineColour"] = "blue";
                        }else{
                            m["OutlineColour"] = "red";
                        }
                        Inject_Thin_Line_Contour(*largest_img,
                                                 line<double>(cu*cfe1, cu*cfe1 + ru),
                                                 DICOM_data.contour_data->ccs.back(),
                                                 m);
                        Inject_Thin_Line_Contour(*largest_img,
                                                 line<double>(cu*cfe2, cu*cfe2 + ru),
                                                 DICOM_data.contour_data->ccs.back(),
                                                 m);
                    }

                    FO << std::setprecision(3) << afe
                       << "," << "row"
                       << "," << UserComment.value_or("")
                       << "," << std::setprecision(3) << std::abs(rfe1-rfe2)
                       << "," << std::setprecision(3) << (rfe1-afe)
                       << "," << std::setprecision(3) << (rfe2-afe) 
                       << std::endl;

                    FO << std::setprecision(3) << afe
                       << "," << "column"
                       << "," << UserComment.value_or("")
                       << "," << std::setprecision(3) << std::abs(cfe1-cfe2)
                       << "," << std::setprecision(3) << (cfe1-afe)
                       << "," << std::setprecision(3) << (cfe2-afe) 
                       << std::endl;
                }
                FO.flush();
                FO.close();
            }
        }


        // Display the detected edges for visual inspection.
        if(InteractivePlots){
            try{
                YgorMathPlottingGnuplot::Plot<double>(row_sums, "Field Edges (Along Rows)", "DICOM position", "Pixel intensity");
                YgorMathPlottingGnuplot::Plot<double>(col_sums, "Field Edges (Along Columns)", "DICOM position", "Pixel intensity");
            }catch(const std::exception &e){
                FUNCWARN("Failed to plot: " << e.what());
            }
        }
    }

    return true;
}
