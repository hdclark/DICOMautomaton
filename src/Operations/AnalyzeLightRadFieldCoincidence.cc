//AnalyzeLightRadFieldCoincidence.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>    
#include <vector>
#include <set> 
#include <map>
#include <unordered_map>
#include <list>
#include <functional>
#include <thread>
#include <array>
#include <mutex>
#include <limits>
#include <cmath>

#include <getopt.h>           //Needed for 'getopts' argument parsing.
#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <experimental/optional>

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMathChebyshev.h" //Needed for cheby_approx class.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorContainers.h"   //Needed for bimap class.
#include "YgorPerformance.h"  //Needed for YgorPerformance_dt_from_last().
#include "YgorAlgorithms.h"   //Needed for For_Each_In_Parallel<..>(...)
#include "YgorArguments.h"    //Needed for ArgumentHandler class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorImagesPlotting.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"

#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/ConvenienceRoutines.h"

#include "../YgorImages_Functors/Processing/DCEMRI_AUC_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_S0_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_T1_Map.h"
#include "../YgorImages_Functors/Processing/Highlight_ROI_Voxels.h"
#include "../YgorImages_Functors/Processing/Kitchen_Sink_Analysis.h"
#include "../YgorImages_Functors/Processing/IVIMMRI_ADC_Map.h"
#include "../YgorImages_Functors/Processing/Time_Course_Slope_Map.h"
#include "../YgorImages_Functors/Processing/CT_Perfusion_Clip_Search.h"
#include "../YgorImages_Functors/Processing/CT_Perf_Pixel_Filter.h"
#include "../YgorImages_Functors/Processing/CT_Convert_NaNs_to_Air.h"
#include "../YgorImages_Functors/Processing/Min_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/Max_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/CT_Reasonable_HU_Window.h"
#include "../YgorImages_Functors/Processing/Slope_Difference.h"
#include "../YgorImages_Functors/Processing/Centralized_Moments.h"
#include "../YgorImages_Functors/Processing/Logarithmic_Pixel_Scale.h"
#include "../YgorImages_Functors/Processing/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Processing/DBSCAN_Time_Courses.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bilinear_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bicubic_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Pixel_Decimate.h"
#include "../YgorImages_Functors/Processing/ImagePartialDerivative.h"
#include "../YgorImages_Functors/Processing/Orthogonal_Slices.h"

#include "../YgorImages_Functors/Transform/DCEMRI_C_Map.h"
#include "../YgorImages_Functors/Transform/DCEMRI_Signal_Difference_C.h"
#include "../YgorImages_Functors/Transform/CT_Perfusion_Signal_Diff.h"
#include "../YgorImages_Functors/Transform/DCEMRI_S0_Map_v2.h"
#include "../YgorImages_Functors/Transform/DCEMRI_T1_Map_v2.h"
#include "../YgorImages_Functors/Transform/Pixel_Value_Histogram.h"
#include "../YgorImages_Functors/Transform/Subtract_Spatially_Overlapping_Images.h"

#include "../YgorImages_Functors/Compute/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Compute/Contour_Similarity.h"

#include "AnalyzeLightRadFieldCoincidence.h"


std::list<OperationArgDoc> OpArgDocAnalyzeLightRadFieldCoincidence(void){
    std::list<OperationArgDoc> out;

    // This operation analyzes the selected images to compare light and radiation field coincidence for fixed, symmetric
    // field sizes. Coincidences are extracted automatically by fitting Gaussians to the peak nearest to one of the
    // specified field boundaries and comparing offset from one another. So, for example, a 10x10cm MLC-defined field
    // would be compared to a 15x15cm field if there are sharp edges (say, metal rulers) that define a 10x10cm field
    // (i.e., considered to represent the light field). Horizontal and vertical directions (both positive and negative)
    // are all analyzed separately.
    //
    // Note: This routine assumes both fields are squarely aligned with the image axes. Alignment need not be perfect,
    //       but the Gaussians may be significantly broadened if there is misalignment. This should be fixed in a future
    //       revision.
    //
    // Note: It is often useful to pre-process inputs by computing an in-image-plane derivative, gradient magnitude, or
    //       similar (i.e., something to emphasize edges) before calling this routine. It may not be necessary, however.
    //

    out.emplace_back();
    out.back().name = "ImageSelection";
    out.back().desc = "Images to operate on. Either 'none', 'last', 'first', or 'all'.";
    out.back().default_val = "last";
    out.back().expected = true;
    out.back().examples = { "none", "last", "first", "all" };

    out.emplace_back();
    out.back().name = "EdgeLengths";
    out.back().desc = "Comma-separated list of (symmetric) edge lengths fields should be analyzed at."
                      " For example, if 50x50, 100x100, 150x150, and 200x200 (all in mm) fields are to be analyzed,"
                      " this argument would be '50,100,150,200' and it will be assumed that the field centre"
                      " is at DICOM position (0,0,0). All values are in DICOM units.";
    out.back().default_val = "100";
    out.back().expected = true;
    out.back().examples = { "100.0", "50,100,150,200,300", "10.273,20.2456" };

    return out;
}

Drover AnalyzeLightRadFieldCoincidence(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto EdgeLengthsStr = OptArgs.getValueStr("EdgeLengths").value();

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

    const auto EdgeLengthStrs = SplitStringToVector(EdgeLengthsStr,",",'d');
    std::vector<double> EdgeLengths;
    for(const auto &els : EdgeLengthStrs) EdgeLengths.push_back( std::stod( els ) );
    if(EdgeLengths.empty()) throw std::invalid_argument("Edge lengths not recognized. Cannot continue.");
    std::sort(EdgeLengths.begin(), EdgeLengths.end());


    const auto dfe = 2.5;  // Permissible maxiumum deviation for peak detection. 
    std::vector<double> fes; // DICOM coordinates of anticipated field edges.
    for(auto &f : EdgeLengths){
        fes.push_back(  0.5*f ); //Field is symmetric and DICOM (0,0,0) is at centre, so halve the edge length.
        fes.push_back( -0.5*f );
    }
    std::sort( fes.begin(), fes.end() );


    auto iap_it = DICOM_data.image_data.begin();
    if(false){
    }else if(std::regex_match(ImageSelectionStr, regex_none)){ iap_it = DICOM_data.image_data.end();
    }else if(std::regex_match(ImageSelectionStr, regex_last)){
        if(!DICOM_data.image_data.empty()) iap_it = std::prev(DICOM_data.image_data.end());
    }
    while(iap_it != DICOM_data.image_data.end()){

        std::vector<YgorMathPlottingGnuplot::Shuttle<samples_1D<double>>> row_sums;
        std::vector<YgorMathPlottingGnuplot::Shuttle<samples_1D<double>>> col_sums;
        {

            std::cout << "Row compatibility matrix:" << std::endl;

            for(const auto & animg : (*iap_it)->imagecoll.images){
                std::vector<double> row_sum(animg.rows, 0.0);
                std::vector<double> col_sum(animg.columns, 0.0);

                //Sum pixel values row- and column-wise.
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
                    samples_1D<double> row_profile;
                    samples_1D<double> col_profile;

                    for(auto row = 0; row < animg.rows; ++row){
                        const auto pos = animg.position(row,0).Dot(animg.row_unit); //Relative to DICOM origin.
                        row_profile.push_back({ pos, 0.0, row_sum[row], 0.0 });
                    }
                    for(auto col = 0; col < animg.columns; ++col){
                        const auto pos = animg.position(0,col).Dot(animg.col_unit); //Relative to DICOM origin.
                        col_profile.push_back({ pos, 0.0, col_sum[col], 0.0 });
                    }

                    // Smooth the profiles to make peak detection more robust.
                    row_profile = row_profile.Moving_Average_Two_Sided_Spencers_15_point();
                    col_profile = col_profile.Moving_Average_Two_Sided_Spencers_15_point();

                    //Perform a high-pass filter to remove some beam profile and imager bias dependence.
                    //row_profile = row_profile.Subtract(row_profile.Moving_Average_Two_Sided_Gaussian_Weighting(75) );
                    //col_profile = col_profile.Subtract(col_profile.Moving_Average_Two_Sided_Gaussian_Weighting(75) );

                    //Normalize so the self-overlap integral is zero.
                    if(true){
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

                    //auto row_peaks_pos = row_peaks.Select_Those_Within_Inc(0.0,1E99);
                    //auto col_peaks_pos = col_peaks.Select_Those_Within_Inc(0.0,1E99);
                    //auto row_peaks_neg = row_peaks.Select_Those_Within_Inc(-1E99,0.0);
                    //auto col_peaks_neg = col_peaks.Select_Those_Within_Inc(-1E99,0.0);

                    //Rank the peaks.
                    all_row_peaks = all_row_peaks.Multiply_With(-1.0).Rank_y();
                    all_col_peaks = all_col_peaks.Multiply_With(-1.0).Rank_y();


                    // Find the most appropriate profile that corresponds to each anticipated field edge.
                    // This is tricky because each profile could have a peak near several anticipated field edges.
                    // We therefore 'vote' on which profile 'owns' the particular field edge, constructing a matrix of
                    // votes and trying to ensure that each field edge is accounted for once.

                    for(const auto &fe : fes){
                        auto row_subset = row_profile.Select_Those_Within_Inc( fe-dfe, fe+dfe );
                        auto col_subset = col_profile.Select_Those_Within_Inc( fe-dfe, fe+dfe );

                        //Check if this is a 'major' peak, or a spurious peak arising from noise.
                        //
                        // Note: I'm lax here to account for noisy peaks. Also remember that the curves are ~symmetric,
                        //       and that the edges can give rise to large, spurious peaks.
                        const auto is_major_row_peak = (all_row_peaks.Interpolate_Linearly(fe)[2] <= 10);
                        const auto is_major_col_peak = (all_col_peaks.Interpolate_Linearly(fe)[2] <= 10);

                        decltype(row_subset.Peaks()) row_peaks;
                        decltype(row_subset.Peaks()) col_peaks;

                        //Ensure the curve spanned the full distance and are a major peak for the curve.
                        if(is_major_row_peak 
                        && !row_subset.empty()){
                            auto rmm = row_subset.Get_Extreme_Datum_x();
                            const auto x_span = std::abs(rmm.second[0] - rmm.first[0]);
                            if(std::abs(PERCENT_ERR(2.0*dfe, x_span)) < 10.0){
                                row_peaks = row_subset.Peaks();
                            }
                        }
                        if(is_major_col_peak
                        && !col_subset.empty()){
                            auto rmm = col_subset.Get_Extreme_Datum_x();
                            const auto x_span = std::abs(rmm.second[0] - rmm.first[0]);
                            if(std::abs(PERCENT_ERR(2.0*dfe, x_span)) < 10.0){
                                col_peaks = col_subset.Peaks();
                            }
                        }

                        //There should only be 1-2 peaks within the anticipated field edge zone.
                        // If there are more, they're probably just noise.

                        //if( (row_peaks.size() == 1) || (row_peaks.size() == 2) ){
                        if(row_peaks.size() == 1){
                            row_sums.emplace_back(row_subset, "Row Profile");
//Also add to a pool for this fe for later scoring.                            
                        }
                        //if( (col_peaks.size() == 1) || (col_peaks.size() == 2) ){
                        if(col_peaks.size() == 1){
                            col_sums.emplace_back(col_subset, "Column Profile");
//Also add to a pool for this fe for later scoring.                            
                        }

                        std::cout << !!(!row_peaks.empty()) << " ";

                    }
                    std::cout << std::endl;

/*
                        const auto row_peaks_pos = row_profile.Select_Those_Within_Inc( h-dh, h+dh).Peaks();
                        const auto col_peaks_pos = col_profile.Select_Those_Within_Inc( h-dh, h+dh).Peaks();
                        const auto row_peaks_neg = row_profile.Select_Those_Within_Inc(-h-dh,-h+dh).Peaks();
                        const auto col_peaks_neg = col_profile.Select_Those_Within_Inc(-h-dh,-h+dh).Peaks();

                        const auto score = (row_peaks_pos.empty() ? 0 : 1)
                                         + (row_peaks_neg.empty() ? 0 : 1)
                                         + (col_peaks_pos.empty() ? 0 : 1)
                                         + (col_peaks_neg.empty() ? 0 : 1);

                        if(score == 4){
                            FUNCINFO("Found peak for h = " << h);
                            row_profile = row_profile.Select_Those_Within_Inc(-h-dh,h+dh);
                            col_profile = col_profile.Select_Those_Within_Inc(-h-dh,h+dh);
                            break;
                        }
                    }
*/                        

                    //row_sums.emplace_back(row_profile, "Row Profile");
                    //col_sums.emplace_back(col_profile, "Column Profile");
                }
            }
        }

//Score the row and column pools for each fe. 
//Keep only the top two (maybe three).
//
// Idea: aspect ratio filtering.
//       1. Remove curves with highly-similar aspect ratios (most likely duplicates).
//          (Don't forget to subtract the y_min for correct aspect ratio calculation...)
//       2. Keep the top 2 (maybe three) for each pool.
//
// TODO.



        // Display the row and column sum profiles for visual estimation of edge coincidence.
        {
            try{
                YgorMathPlottingGnuplot::Plot<double>(row_sums, "Row sums", "DICOM position", "Pixel intensity");
                YgorMathPlottingGnuplot::Plot<double>(col_sums, "Column sums", "DICOM position", "Pixel intensity");
            }catch(const std::exception &e){
                FUNCWARN("Failed to plot: " << e.what());
            }
        }

        // Loop control.
        ++iap_it;
        if(std::regex_match(ImageSelectionStr, regex_first)) break;
    }

    return std::move(DICOM_data);
}
