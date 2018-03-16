//AnalyzePicketFence.cc - A part of DICOMautomaton 2018. Written by hal clark.

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

#include <eigen3/Eigen/Dense>

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
#include "../Insert_Contours.h"

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

#include "AnalyzePicketFence.h"


std::list<OperationArgDoc> OpArgDocAnalyzePicketFence(void){
    std::list<OperationArgDoc> out;

    // This operation extracts MLC positions from a picket fence image.
    //
    // Note: This routine requires data to be pre-processed. The gross picket area should be isolated and the leaf
    //       junction areas contoured (one contour per junction). Both can be accomplished via thresholding.

    out.emplace_back();
    out.back().name = "ImageSelection";
    out.back().desc = "Images to operate on. Either 'none', 'last', 'first', or 'all'.";
    out.back().default_val = "last";
    out.back().expected = true;
    out.back().examples = { "none", "last", "first", "all" };
    
    out.emplace_back();
    out.back().name = "ROILabelRegex";
    out.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.back().default_val = ".*";
    out.back().expected = true;
    out.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                            R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                            R"***(left_parotid|right_parotid)***" };

    out.emplace_back();
    out.back().name = "NormalizedROILabelRegex";
    out.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.back().default_val = ".*";
    out.back().expected = true;
    out.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };

    out.emplace_back();
    out.back().name = "NumberOfLeaves";
    out.back().desc = "The total number of leaves in the image.";
    out.back().default_val = "60";
    out.back().expected = true;
    out.back().examples = { "60" };


    out.emplace_back();
    out.back().name = "MinimumJunctionSeparation";
    out.back().desc = "The minimum distance between junctions in DICOM units. This number is used to"
                      " de-duplicate automatically detected junctions. Analysis results should not be"
                      " sensitive to the specific value.";
    out.back().default_val = "10.0";
    out.back().expected = true;
    out.back().examples = { "5.0", "10.0", "15.0", "25.0" };

    return out;
}

Drover AnalyzePicketFence(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();

    const auto NumberOfLeaves = std::stol( OptArgs.getValueStr("NumberOfLeaves").value() );
    const auto MinimumJunctionSeparation = std::stol( OptArgs.getValueStr("MinimumJunctionSeparation").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto roiregex = std::regex(ROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto roinormalizedregex = std::regex(NormalizedROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

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
    std::list<std::reference_wrapper<contour_of_points<double>>> cop_all;
    for(auto & cc : DICOM_data.contour_data->ccs){
        for(auto & cop : cc.contours){
            cop_all.push_back( std::ref(cop) );
        }
    }

    //Whitelist contours using the provided regex.
    auto cop_ROIs = cop_all;
    cop_ROIs.remove_if([=](std::reference_wrapper<contour_of_points<double>> cop) -> bool {
                   const auto ROINameOpt = cop.get().GetMetadataValueAs<std::string>("ROIName");
                   const auto ROIName = ROINameOpt.value_or("");
                   return !(std::regex_match(ROIName,roiregex));
    });
    cop_ROIs.remove_if([=](std::reference_wrapper<contour_of_points<double>> cop) -> bool {
                   const auto ROINameOpt = cop.get().GetMetadataValueAs<std::string>("NormalizedROIName");
                   const auto ROIName = ROINameOpt.value_or("");
                   return !(std::regex_match(ROIName,roinormalizedregex));
    });
    const auto NumberOfJunctions = static_cast<long int>(cop_ROIs.size());

    if(NumberOfJunctions < 2) throw std::domain_error("At least 2 junctions are needed for this analysis.");


    auto iap_it = DICOM_data.image_data.begin();
    if(false){
    }else if(std::regex_match(ImageSelectionStr, regex_none)){ iap_it = DICOM_data.image_data.end();
    }else if(std::regex_match(ImageSelectionStr, regex_last)){
        if(!DICOM_data.image_data.empty()) iap_it = std::prev(DICOM_data.image_data.end());
    }
    while(iap_it != DICOM_data.image_data.end()){

        if((*iap_it)->imagecoll.images.empty()) throw std::invalid_argument("Unable to find an image to analyze.");

        planar_image<float, double> *animg = &( (*iap_it)->imagecoll.images.front() );

        // Using the contours that surround the leaf junctions, extract a 'long' and 'short' direction along the
        // junctions. This will orient the image and give us some orientation independence. We use principle component
        // analysis for this.
        //
        // Note: We can estimate these directions for each contour, but the analysis will be more consistent if all
        //       contours are considered simultaneously. The trade-off is that results may depend on the aspect ratio of
        //       the image. A zoomed-in picket fence may result in incorrect PCA estimation when using all contours
        //       simultaneously.
        //

        size_t N_verts = 0;
        for(auto &acop : cop_ROIs) N_verts += acop.get().points.size();
        Eigen::MatrixXd verts( N_verts, 3 );
        {
            size_t i = 0;
            for(auto &acop : cop_ROIs){
                const auto cent = acop.get().Centroid(); // We're interested in ROI shape, not ROI arrangement.
                for(auto &p : acop.get().points){
                    const auto c = (p - cent);
                    verts(i,0) = c.x;
                    verts(i,1) = c.y;
                    verts(i,2) = c.z;
                    ++i;
                }
            }
        }

        Eigen::MatrixXd centred = verts.rowwise() - verts.colwise().mean();
        Eigen::MatrixXd cov = centred.adjoint() * centred;
        Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> eig(cov); // Solves the eigenproblem.

        Eigen::VectorXd long_dir = eig.eigenvectors().col(2); // Principle component eigenvector (long axis).
        Eigen::VectorXd short_dir = eig.eigenvectors().col(1); // (short axis).
        //Eigen::VectorXd ortho_dir = eig.eigenvectors().col(1); // (orthogonal out-of-plane direction).

        const auto long_unit  = (vec3<double>( long_dir(0),  long_dir(1),  long_dir(2)  )).unit();
        const auto short_unit = (vec3<double>( short_dir(0), short_dir(1), short_dir(2) )).unit();

        FUNCINFO("Junction-aligned direction is " << long_unit );
        FUNCINFO("Junction-orthogonal direction is " << short_unit );

        //Produce a pixel value profile-projection by ray-casting along the junction-orthogonal direction.
        const bool inhibit_sort = true;
        samples_1D<double> junction_ortho_projections;
        const auto corner_pos = animg->position(0, 0);

/* */
        //Use all pixels for profile generation.
        //
        // Note: these profiles are noisy. It might be better to only sample the region between junctions, which could
        //       also be cheaper than scanning every pixel.   TODO.
        for(long int row = 0; row < animg->rows; ++row){
            for(long int col = 0; col < animg->columns; ++col){
                const auto val = animg->value(row, col, 0);
                const auto rel_pos = (animg->position(row, col) - corner_pos);
                const auto long_line_L = rel_pos.Dot(long_unit);
                junction_ortho_projections.push_back( { long_line_L, 0.0, val, 0.0 }, inhibit_sort );
            }
        }
/* */        

        //Use pixels in-between junctions for profile generation.
        std::vector<vec3<double>> junction_centroids;
        for(auto &acop : cop_ROIs) junction_centroids.emplace_back( acop.get().Centroid() );

        //Remove duplicate junctions, which appear when the junction is piecemeal contoured (and is likely to happen).
        {
            std::sort(junction_centroids.begin(), junction_centroids.end(), [&](const vec3<double> &A, const vec3<double> &B) -> bool {
                const auto A_pos = A.Dot(short_unit);
                const auto B_pos = B.Dot(short_unit);
                return (A_pos < B_pos);
            });
            auto last = std::unique(junction_centroids.begin(), 
                                    junction_centroids.end(),
                                    [&](const vec3<double> &A, const vec3<double> &B) -> bool {
                                        const auto d = std::abs( (A - B).Dot(short_unit) );
                                        return (d < 0.5*MinimumJunctionSeparation);
                                    });
            junction_centroids.erase(last, junction_centroids.end());
        }

        //Add thin contours for inspecting the junction lines.
        DICOM_data.contour_data->ccs.emplace_back();
        for(const auto &cent : junction_centroids){
            Inject_Thin_Line_Contour(*animg,
                                     line<double>(cent, cent + long_unit),
                                     DICOM_data.contour_data->ccs.back(),
                                     animg->metadata);
        }

        std::vector<double> junction_cax_separations;
        for(size_t i = 0; i < junction_centroids.size(); ++i){
            const auto dR = (junction_centroids[i] - vec3<double>( 0.0, 0.0, 0.0 ));
            junction_cax_separations.push_back( dR.Dot(short_unit) );
        }
        std::sort(junction_cax_separations.begin(), junction_cax_separations.end());
        std::cout << "Junction-CAX separations: ";
        for(auto &d : junction_cax_separations) std::cout << d << "  ";
        std::cout << std::endl;

        std::vector<double> junction_separations;
        for(size_t i = 1; i < junction_centroids.size(); ++i){
            const auto dR = (junction_centroids[i] - junction_centroids[i-1]);
            junction_separations.push_back( std::abs( dR.Dot(short_unit) ) );
        }
        //std::sort(junction_separations.begin(), junction_separations.end());
        const auto min_junction_sep = Stats::Min(junction_separations);
        std::cout << "Minimum junction separation: " << min_junction_sep << std::endl;

/*
        //Used only for profile generation using pixels in-between junctions only.

        auto near_a_junction = [=](const vec3<double> &p) -> bool {
            //Determines if a given point is sufficiently far from the nearest junction.
            for(const auto &jc : junction_centroids){
                line<double> l(jc, jc + long_unit);
                const auto d = l.Distance_To_Point(p);
                if(d < 0.25 * min_junction_sep) return true;
            }
            return false;
        };
        auto between_junctions = [=](const vec3<double> &p) -> bool {
            //Determines if a given point is between at least two junctions.
            const auto d = (p - vec3<double>( 0.0, 0.0, 0.0 )).Dot(short_unit);
            return (  (junction_cax_separations.front() < d)
                   && (d < junction_cax_separations.back()) );
        };

        for(long int row = 0; row < animg->rows; ++row){
            for(long int col = 0; col < animg->columns; ++col){
                const auto pos = animg->position(row, col);
                //if(near_a_junction(pos) || !between_junctions(pos)) continue;
                if(!between_junctions(pos)) continue;

                const auto val = animg->value(row, col, 0);
                const auto rel_pos = (pos - corner_pos);
                const auto long_line_L = rel_pos.Dot(long_unit);
                junction_ortho_projections.push_back( { long_line_L, 0.0, val, 0.0 }, inhibit_sort );
            }
        }
*/        



        //Prepare the data to identify peaks in the junction-orthogonal direction to locate leaf pairs.
        junction_ortho_projections.stable_sort();
        const long int N_bins = static_cast<long int>( std::max(animg->rows, animg->columns));
        auto junction_ortho_profile = junction_ortho_projections.Aggregate_Equal_Sized_Bins_Weighted_Mean(N_bins);

        //auto junction_ortho_profile2 = junction_ortho_profile.Moving_Average_Two_Sided_Hendersons_23_point();
        //auto junction_ortho_profile2 = junction_ortho_profile.Moving_Median_Filter_Two_Sided_Equal_Weighting(2);
        //auto junction_ortho_profile2 = junction_ortho_profile.Moving_Average_Two_Sided_Gaussian_Weighting(4);
        auto junction_ortho_profile2 = junction_ortho_profile.Moving_Average_Two_Sided_Spencers_15_point();
        //bool l_OK;
        //auto junction_ortho_profile2 = NPRLL::Attempt_Auto_Smooth(junction_ortho_profile, &l_OK);
        //if(!l_OK) throw std::runtime_error("Unable to perform NPRLL to smooth junction-orthogonal profile.");

        //Perform a high-pass filter to remove some beam profile and imager bias dependence.
        junction_ortho_profile2 = junction_ortho_profile2.Subtract(
                                        junction_ortho_profile2.Moving_Average_Two_Sided_Gaussian_Weighting(15) );

        //Now find all (local) peaks via the derivative of the crossing points.
        //
        // Note: We find peaks (gaps between leaves) instead of troughs (middle of leaves) because the background (i.e.,
        //       dose behind the jaws) confounds trough isolation for leaves on the boundaries. Peaks are also sharper
        //       (i.e., more confined spatially owing to the smaller gap they arise from), whereas troughs can undulate
        //       considerably more.
        auto peaks = junction_ortho_profile2.Peaks();

        //Merge peaks that are separated by a small distance.
        //
        // Note: We are generous here because the leaf thickness is bounded to >2mm or so.
        peaks.Average_Coincident_Data(2.0);

        //Filter out spurious peaks that are not 'sharp' enough.
        //
        // Fit a line to the left-adjacent N datum.
        // Fit a line to the right-adjacent N datum.
        // Threshold on the smallest angle that separates them.

        if(peaks.size() < 5) std::runtime_error("Leaf-leakage peaks not correctly detected. Please verify input.");


        //Create lines for each detected leaf pair.
        std::vector<line<double>> leaf_lines;
        for(const auto &peak4 : peaks.samples){
            const auto cent = junction_centroids.front();
            const auto d = peak4[0]; // Projection of relative position onto the long-axis unit vector.

            const auto offset = (cent - corner_pos).Dot(long_unit);
            const auto R_peak = cent + long_unit * (d - offset); // Actual position of peak (on this junction).

            leaf_lines.emplace_back( R_peak, R_peak + short_unit );
        }

        //Add thin contours for visually inspecting the location of the peaks.
        DICOM_data.contour_data->ccs.emplace_back();
        for(const auto &leaf_line : leaf_lines){
            Inject_Thin_Line_Contour(*animg,
                                     leaf_line,
                                     DICOM_data.contour_data->ccs.back(),
                                     animg->metadata);
        }

        std::vector<double> peak_separations;
        for(size_t i = 1; i < peaks.samples.size(); ++i){
            const auto d = (peaks.samples[i][0] - peaks.samples[i-1][0]);
            peak_separations.push_back( std::abs( d ) );
        }
        //std::sort(peak_separations.begin(), peak_separations.end());
        const auto avg_peak_sep = Stats::Mean(peak_separations);
        std::cout << "Average peak separation: " << avg_peak_sep << std::endl;
        std::cout << "Median peak separation: " << Stats::Median(peak_separations) << std::endl;
        std::cout << "Peak separations: ";
        for(auto &d : peak_separations) std::cout << d << "  ";
        std::cout << std::endl;

 

        //YgorMathPlottingGnuplot::Shuttle<samples_1D<double>> plot_shtl_1(junction_ortho_profile, "Without smoothing");
        YgorMathPlottingGnuplot::Shuttle<samples_1D<double>> plot_shtl_2(junction_ortho_profile2, "Smoothed and filtered");
        YgorMathPlottingGnuplot::Shuttle<samples_1D<double>> plot_shtl_5(peaks, "Peaks");
        YgorMathPlottingGnuplot::Plot<double>({plot_shtl_2, plot_shtl_5}, "Junction-orthogonal profile", "DICOM position", "Average Pixel Intensity");

        
        // Loop control.
        ++iap_it;
        if(std::regex_match(ImageSelectionStr, regex_first)) break;
    }

    return std::move(DICOM_data);
}
