//ContourBooleanOperations.cc - A part of DICOMautomaton 2017. Written by hal clark.

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
#include <regex>

#include <getopt.h>           //Needed for 'getopts' argument parsing.
#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <experimental/optional>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathIOOFF.h"    //Needed for WritePointsToOFF(...)
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
#include "../Thread_Pool.h"
#include "../Dose_Meld.h"
#include "../Contour_Boolean_Operations.h"

#include "../YgorImages_Functors/Grouping/Misc_Functors.h"

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
#include "../YgorImages_Functors/Processing/Cross_Second_Derivative.h"
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
#include "../YgorImages_Functors/Compute/AccumulatePixelDistributions.h"
#include "../YgorImages_Functors/Compute/GenerateSurfaceMask.h"

#include "ContourBooleanOperations.h"



std::list<OperationArgDoc> OpArgDocContourBooleanOperations(void){
    std::list<OperationArgDoc> out;


    out.emplace_back();
    out.back().name = "ROILabelRegexA";
    out.back().desc = "A regex matching ROI labels/names that comprise the set of contour polygons 'A'"
                      " as in f(A,B) where f is some Boolean operation." 
                      " The default with match all available ROIs, which is probably not what you want.";
    out.back().default_val = ".*";
    out.back().expected = true;
    out.back().examples = { ".*", ".*[pP]rostate.*", "body", "Gross_Liver",
                            R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                            R"***(left_parotid|right_parotid)***" };

    out.emplace_back();
    out.back().name = "ROILabelRegexB";
    out.back().desc = "A regex matching ROI labels/names that comprise the set of contour polygons 'B'"
                      " as in f(A,B) where f is some Boolean operation." 
                      " The default with match all available ROIs, which is probably not what you want.";
    out.back().default_val = ".*";
    out.back().expected = true;
    out.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                            R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                            R"***(left_parotid|right_parotid)***" };

    out.emplace_back();
    out.back().name = "NormalizedROILabelRegexA";
    out.back().desc = "A regex matching ROI labels/names that comprise the set of contour polygons 'A'"
                      " as in f(A,B) where f is some Boolean operation. "
                      " The regex is applied to normalized ROI labels/names, which are translated using"
                      " a user-provided lexicon (i.e., a dictionary that supports fuzzy matching)."
                      " The default with match all available ROIs, which is probably not what you want.";
    out.back().default_val = ".*";
    out.back().expected = true;
    out.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };

    out.emplace_back();
    out.back().name = "NormalizedROILabelRegexB";
    out.back().desc = "A regex matching ROI labels/names that comprise the set of contour polygons 'B'"
                      " as in f(A,B) where f is some Boolean operation. "
                      " The regex is applied to normalized ROI labels/names, which are translated using"
                      " a user-provided lexicon (i.e., a dictionary that supports fuzzy matching)."
                      " The default with match all available ROIs, which is probably not what you want.";
    out.back().default_val = ".*";
    out.back().expected = true;
    out.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };

    out.emplace_back();
    out.back().name = "Operation";
    out.back().desc = "The Boolean operation (e.g., the function 'f') to perform on the sets of"
                      " contour polygons 'A' and 'B'. 'Symmetric difference' is also known as 'XOR'.";
    out.back().default_val = "join";
    out.back().expected = true;
    out.back().examples = { "intersection", "join", "difference", "symmetric_difference" };

    out.emplace_back();
    out.back().name = "OutputROILabel";
    out.back().desc = "The label to attach to the ROI contour product of f(A,B).";
    out.back().default_val = "Boolean_result";
    out.back().expected = true;
    out.back().examples = { "A+B", "A-B", "AuB", "AnB", "AxB", "A^B", "union", "xor", "combined", "body_without_spinal_cord" };

    return out;
}



Drover ContourBooleanOperations(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){
    // This routine performs 2D Boolean operations on user-provided sets of ROIs. The ROIs themselves are planar
    // contours embedded in R^3, but the Boolean operation is performed once for each 2D plane where the selected ROIs
    // reside. This routine can only perform Boolean operations on co-planar contours. This routine can operate on
    // single contours (rather than ROIs composed of several contours) by simply presenting this routine with a single
    // contour to select. 
    //
    // Note that this routine DOES support disconnected ROIs, such as left- and right-parotid contours that have been
    // joined into a single 'parotids' ROI.
    //
    // Note that many Boolean operations can produce contours with holes. This operation currently connects the interior
    // and exterior with a seam so that holes can be represented by a single polygon (rather than a separate hole
    // polygon). It *is* possible to export holes as contours with a negative orientation, but this was not needed when
    // writing.
    //
    // Note that only the common metadata between contours is propagated to the product contours.
    //

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegexA = OptArgs.getValueStr("ROILabelRegexA").value();
    const auto ROILabelRegexB = OptArgs.getValueStr("ROILabelRegexB").value();
    const auto NormalizedROILabelRegexA = OptArgs.getValueStr("NormalizedROILabelRegexA").value();
    const auto NormalizedROILabelRegexB = OptArgs.getValueStr("NormalizedROILabelRegexB").value();

    const auto Operation_str = OptArgs.getValueStr("Operation").value();
    const auto OutputROILabel = OptArgs.getValueStr("OutputROILabel").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto roiregexA = std::regex(ROILabelRegexA, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto roiregexB = std::regex(ROILabelRegexB, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto roinormalizedregexA = std::regex(NormalizedROILabelRegexA, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto roinormalizedregexB = std::regex(NormalizedROILabelRegexB, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto JoinRegex = std::regex("^jo?i?n?$", 
                            std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto IntersectionRegex = std::regex("^in?t?e?r?s?e?c?t?i?o?n?$", 
                            std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto DifferenceRegex = std::regex("^di?f?f?e?r?e?n?c?e?$", 
                            std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto SymmDiffRegex = std::regex("^sy?m?m?e?t?r?i?c?_?d?i?f?f?e?r?e?n?c?e?$", 
                            std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    //Figure out which operation is desired.
    ContourBooleanMethod op = ContourBooleanMethod::join;
    if(false){
    }else if(std::regex_match(Operation_str,JoinRegex)){
        op = ContourBooleanMethod::join;
    }else if(std::regex_match(Operation_str,IntersectionRegex)){
        op = ContourBooleanMethod::intersection;
    }else if(std::regex_match(Operation_str,DifferenceRegex)){
        op = ContourBooleanMethod::difference;
    }else if(std::regex_match(Operation_str,SymmDiffRegex)){
        op = ContourBooleanMethod::symmetric_difference;
    }else{
        throw std::logic_error("Unanticipated Boolean operation request.");
    }

    Explicator X(FilenameLex);


    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    std::list<std::reference_wrapper<contour_collection<double>>> cc_all;
    for(auto & cc : DICOM_data.contour_data->ccs){
        auto base_ptr = reinterpret_cast<contour_collection<double> *>(&cc);
        cc_all.push_back( std::ref(*base_ptr) );
    }

    //Whitelist contours using the provided regex.
    auto cc_A = cc_all;
    cc_A.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("ROIName");
                   const auto ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,roiregexA));
    });
    cc_A.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("NormalizedROIName");
                   const auto ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,roinormalizedregexA));
    });

    auto cc_B = cc_all;
    cc_B.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("ROIName");
                   const auto ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,roiregexB));
    });
    cc_B.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("NormalizedROIName");
                   const auto ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,roinormalizedregexB));
    });

    //This will not necessarily be an error... I should let CGAL handle it.
//    if(cc_A.empty()){
//        throw std::invalid_argument("No ROI contours selected. Cannot continue.");
//    }else if(cc_B.empty()){
//        throw std::invalid_argument("No ReferenceROI contours selected. Cannot continue.");
//    }
 

    //Make a copy of all contours for assessing some information later.
    std::list<std::reference_wrapper<contour_collection<double>>> cc_A_B;
    cc_A_B.insert(cc_A_B.end(), cc_A.begin(), cc_A.end());
    cc_A_B.insert(cc_A_B.end(), cc_B.begin(), cc_B.end());

    // Identify the unique planes represented in sets A and B.
    const auto est_cont_normal = cc_A_B.front().get().contours.front().Estimate_Planar_Normal();
    const auto ucp = Unique_Contour_Planes(cc_A_B, est_cont_normal, /*distance_eps=*/ 0.005);

    const double fallback_spacing = 0.005; // in DICOM units (usually mm).
    const auto cont_sep_range = std::abs(ucp.front().Get_Signed_Distance_To_Point( ucp.back().R_0 ));
    const double est_cont_spacing = (ucp.size() <= 1) ? fallback_spacing : cont_sep_range / static_cast<double>(ucp.size() - 1);
    const double est_cont_thickness = 0.5005 * est_cont_spacing; // Made slightly thicker to avoid gaps.

    //For identifying duplicate vertices later.
    const auto verts_equal_F = [](const vec3<double> &vA, const vec3<double> &vB) -> bool {
        return ( vA.sq_dist(vB) < std::pow(0.01,2.0) );
    };

    // For each plane, pack the shuttles with (only) the relevant contours.
    contour_collection<double> cc_new;
    for(const auto &aplane : ucp){
        std::list<std::reference_wrapper<contour_of_points<double>>> A;
        std::list<std::reference_wrapper<contour_of_points<double>>> B;

        for(auto &cc : cc_A){
            for(auto &cop : cc.get().contours){
                //Ignore contours that are not 'on' the specified plane.
                // We give planes a thickness to help determine coincidence.
                cop.Remove_Sequential_Duplicate_Points(verts_equal_F);
                cop.Remove_Needles(verts_equal_F);
                if(cop.points.empty()) continue;
                const auto dist_to_plane = std::abs(aplane.Get_Signed_Distance_To_Point(cop.points.front()));
                if(dist_to_plane > est_cont_thickness) continue;

                //Pack the contour into the shuttle.
                A.emplace_back(std::ref(cop));
            }
        }
        for(auto &cc : cc_B){
            for(auto &cop : cc.get().contours){
                cop.Remove_Sequential_Duplicate_Points(verts_equal_F);
                cop.Remove_Needles(verts_equal_F);
                if(cop.points.empty()) continue;
                const auto dist_to_plane = std::abs(aplane.Get_Signed_Distance_To_Point(cop.points.front()));
                if(dist_to_plane > est_cont_thickness) continue;

                B.emplace_back(std::ref(cop));
            }
        }

        //Perform the operation.
        //FUNCINFO("About to perform boolean operation. A and B have " << A.size() << " and " << B.size() << " elements, respectively");
        auto cc = ContourBoolean(op, aplane, A, B);

        //Insert any contours created into a holding contour_collection.
        cc_new.contours.splice(cc_new.contours.end(), std::move(cc.contours));
    }

    //Attach the requested metadata.
    cc_new.Insert_Metadata("ROIName", OutputROILabel);
    cc_new.Insert_Metadata("NormalizedROIName", X(OutputROILabel));
    cc_new.Insert_Metadata("ROINumber", "999");
    cc_new.Insert_Metadata("MinimumSeparation", std::to_string(est_cont_spacing));

    // Insert it into the Contour_Data.
    FUNCINFO("Boolean operation created " << cc_new.contours.size() << " contours");
    if(cc_new.contours.empty()){
        FUNCWARN("ROI was not added because it is empty");
    }else{
        DICOM_data.contour_data->ccs.emplace_back(cc_new);
        DICOM_data.contour_data->ccs.back().ROI_number = 999;
        DICOM_data.contour_data->ccs.back().Minimum_Separation = est_cont_spacing;
        DICOM_data.contour_data->ccs.back().Raw_ROI_name = OutputROILabel;
    }


/*

//// Stuff salvaged from another operation which may not be useful ////

    //Pre-compute least-squares planes and contour projections onto those planes (in case of non-planar contours).
    // This substantially speeds up the computation.
    std::vector< std::pair< plane<double>, contour_of_points<double> > > projected_contours;
    for(const auto & cc_ref : cc_A){
        const auto cc = cc_ref.get();
        for(const auto & c : cc.contours){    
            const auto c_plane = c.Least_Squares_Best_Fit_Plane(est_cont_normal);
            projected_contours.emplace_back( std::make_pair(c_plane, c.Project_Onto_Plane_Orthogonally(c_plane)) );
        }
    }
    for(const auto & cc_ref : cc_B){
        const auto cc = cc_ref.get();
        for(const auto & c : cc.contours){    
            const auto c_plane = c.Least_Squares_Best_Fit_Plane(est_cont_normal);
            projected_contours.emplace_back( std::make_pair(c_plane, c.Project_Onto_Plane_Orthogonally(c_plane)) );
        }
    }
   


    //Figure out what z-margin is needed so the extra two images do not interfere with the grid lining up with the
    // contours. (Want exactly one contour plane per image.) So the margin should be large enough so the empty
    // images have no contours inside them, but small enough so that it doesn't affect the location of contours in the
    // other image slices. The ideal is if each image slice has the same thickness so contours are all separated by some
    // constant separation -- in this case we make the margin exactly as big as if two images were also included.
    double z_margin = 0.0;
    if(ucp.size() > 1){
        //Compute the total distance between the (centre of the) top and (centre of the) bottom planes.
        // (Note: the images associated with these contours will usually extend further. This is dealt with below.)
        const auto total_sep =  std::abs(ucp.front().Get_Signed_Distance_To_Point(ucp.back().R_0));
        const auto sep_per_plane = total_sep / static_cast<double>(ucp.size()-1);

        //Add TOTAL zmargin of 1*sep_per_plane each for 2 extra images, and 0.5*sep_per_plane for each of the images
        // which will stick out beyond the contour planes. (The margin is added at the top and the bottom.)
        z_margin = sep_per_plane * 1.5;
    }else{
        FUNCWARN("Only a single contour plane was detected. Guessing its thickness.."); 
        z_margin = 5.0;
    }

    //{
    //    asio_thread_pool tp;
    //    std::mutex printer; // Who gets to print to the console and iterate the counter.
    //    long int completed = 0;
    //
    //    for(long int row = 0; row < SourceDetectorRows; ++row){
    //        tp.submit_task([&,row](void) -> void {
    //            for(long int col = 0; col < SourceDetectorColumns; ++col){
    //
    //            {
    //                std::lock_guard<std::mutex> lock(printer);
    //                ++completed;
    //                FUNCINFO("Completed " << completed << " of " << SourceDetectorRows 
    //                      << " --> " << static_cast<int>(1000.0*(completed)/SourceDetectorRows)/10.0 << "\% done");
    //            }
    //        });
    //    }
    //} // Complete tasks and terminate thread pool.
*/

    return std::move(DICOM_data);
}
