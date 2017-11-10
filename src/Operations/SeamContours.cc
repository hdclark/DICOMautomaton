//SeamContours.cc - A part of DICOMautomaton 2017. Written by hal clark.

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

#include "SeamContours.h"



std::list<OperationArgDoc> OpArgDocSeamContours(void){
    std::list<OperationArgDoc> out;

    return out;
}



Drover SeamContours(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){
    // This routine converts contours that represent 'outer' and 'inner' via contour orientation into contours that are
    // uniformly outer but have a zero-area seam connecting the inner and outer portions.
    //
    // Note: This routine currently operates on all available ROIs.
    //
    // Note: This routine operates on one contour_collection at a time. It will combine contours that are in the same
    // contour_collection and overlap, even if they have different ROINames. Consider making a complementary routine
    // that partitions contours into ROIs based on ROIName (or other metadata) if more rigorous enforcement is needed.
    //
    // Note: This routine actually computes the XOR Boolean of contours that overlap. So if contours partially overlap,
    // this routine will treat the overlapping parts as if they are holes, and the non-overlapping parts as if they
    // represent the ROI. This behaviour may be surprising in some cases.
    //
    // Note: This routine will also treat overlapping contours with like orientation as if the smaller contour were a
    // hole of the larger contour.
    // 
    // Note: This routine will ignore contour orientation if there is only a single contour. More specifically, for a
    // given ROI label, planes with a single contour will be unaltered.
    //
    // Note: Only the common metadata between outer and inner contours is propagated to the seamed contours.
    //
    // Note: This routine will NOT combine disconnected contours with a seam. Disconnected contours will remain
    // disconnected.
    //
    if(DICOM_data.contour_data == nullptr) return std::move(DICOM_data);

    //For identifying duplicate vertices later.
    const auto verts_equal_F = [](const vec3<double> &vA, const vec3<double> &vB) -> bool {
        return ( vA.sq_dist(vB) < std::pow(0.01,2.0) );
    };
    
    auto cd = DICOM_data.contour_data->Duplicate();
    if(cd == nullptr) throw std::logic_error("Unable to duplicate contours. Cannot continue.");

    for(auto &cc : cd->ccs){
        if(cc.contours.empty()) continue;
        if(cc.contours.front().points.empty()){
            throw std::logic_error("Planar normal estimation technique failed. Consider searching more contours.");
        }
 
        // Identify the unique planes.
        std::list<std::reference_wrapper<contour_collection<double>>> cc_ref;
        {
            auto base_ptr = reinterpret_cast<contour_collection<double> *>(&cc);
            cc_ref.push_back( std::ref(*base_ptr) );
        }
        const auto est_cont_normal = cc.contours.front().Estimate_Planar_Normal();
        const auto ucp = Unique_Contour_Planes(cc_ref, est_cont_normal, /*distance_eps=*/ 0.005);

        const double fallback_spacing = 0.005; // in DICOM units (usually mm).
        const auto cont_sep_range = std::abs(ucp.front().Get_Signed_Distance_To_Point( ucp.back().R_0 ));
        const double est_cont_spacing = (ucp.size() <= 1) ? fallback_spacing : cont_sep_range / static_cast<double>(ucp.size() - 1);
        const double est_cont_thickness = 0.5005 * est_cont_spacing; // Made slightly thicker to avoid gaps.

        
        // For each plane, pack the shuttles with (only) the relevant contours.
        contour_collection<double> cc_new;
        for(const auto &aplane : ucp){
            std::list<std::reference_wrapper<contour_of_points<double>>> copl;
            std::set<std::string> ROINames;
            for(auto &cop : cc.contours){
                //Ignore contours that are not 'on' the specified plane.
                // We give planes a thickness to help determine coincidence.
                cop.Remove_Sequential_Duplicate_Points(verts_equal_F);
                cop.Remove_Needles(verts_equal_F);
                if(cop.points.size() < 3) continue;
                const auto dist_to_plane = std::abs(aplane.Get_Signed_Distance_To_Point(cop.points.front()));
                if(dist_to_plane > est_cont_thickness) continue;

                //Pack the contour into the shuttle.
                copl.emplace_back(std::ref(cop));
                ROINames.insert( cop.metadata["ROIName"] );
            }

            if(false){
            }else if(copl.empty()){
                throw std::logic_error("Found no contours incident on plane previously found to house contours.");
            }else if(copl.size() == 1){ // No Boolean operation needed. Copy as-is.
                cc_new.contours.emplace_back( copl.front().get() );
            }else{ // Possible overlap. Let CGAL work it out...
                const auto construct_op = ContourBooleanMethod::symmetric_difference;
                const auto op = ContourBooleanMethod::noop;
                auto cc_out = ContourBoolean(aplane, copl, {}, op, construct_op);

                if(ROINames.size() != 1){ //Warn if ROINames vary.
                    std::stringstream ss;
                    ss << "Seamed contours that had different ROI names (";
                    ss << *ROINames.begin();
                    for(auto it = std::next(ROINames.begin()); it != ROINames.end(); ++it){
                        ss << ", " << *it;
                    }
                    ss << "). Was this intentional?";
                    FUNCWARN(ss.str());
                    // Implementation Note:
                    // This will happen if a contour collection has contours from more than one ROI.
                    // When I implemented this, I found that there was always 1 ROI per contour_collection.
                    // If this needs to be more rigourous enforced, consider both making this an error and 
                    // providing a separate operation for partitioning contours using ROINames.
                }
                cc_new.contours.splice(cc_new.contours.end(), std::move(cc_out.contours));
            }
        }

        // Replace the existing cc.
        cc.contours.clear();
        cc.contours.splice( cc.contours.end(), std::move(cc_new.contours) );
    }

    DICOM_data.contour_data = std::move(cd);
    return std::move(DICOM_data);
}
