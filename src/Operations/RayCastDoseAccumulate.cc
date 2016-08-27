//RayCastDoseAccumulate.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

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
#include "../Dose_Meld.h"

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

#include "RayCastDoseAccumulate.h"



std::list<OperationArgDoc> OpArgDocRayCastDoseAccumulate(void){
    std::list<OperationArgDoc> out;

    out.emplace_back();
    out.back().name = "DoseLengthMapFileName";
    out.back().desc = "A filename (or full path) for the (dose)*(length traveled through the ROI peel) image map."
                      " The format is TBD. Leave empty to dump to generate a unique temporary file.";  // TODO: specify format.
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "", "/tmp/somefile", "localfile.img", "derivative_data.img" };


    out.emplace_back();
    out.back().name = "LengthMapFileName";
    out.back().desc = "A filename (or full path) for the (length traveled through the ROI peel) image map."
                      " The format is TBD. Leave empty to dump to generate a unique temporary file.";  // TODO: specify format.
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "", "/tmp/somefile", "localfile.img", "derivative_data.img" };


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
    out.back().name = "CylinderRadius";
    out.back().desc = "The radius of the cylinder surrounding contour line segments that defines the 'surface'."
                      " Quantity is in the DICOM coordinate system.";
    out.back().default_val = "3.0";
    out.back().expected = true;
    out.back().examples = { "1.0", "2.0", "0.5", "5.0" };
    
    out.emplace_back();
    out.back().name = "RaydL";
    out.back().desc = "The distance to move a ray each iteration. Should be << img_thickness and << cylinder_radius."
                      " Making too large will invalidate results, causing rays to pass through the surface without"
                      " registering any dose accumulation. Making too small will cause the run-time to grow and may"
                      " eventually lead to truncation or round-off errors. Quantity is in the DICOM coordinate system.";
    out.back().default_val = "0.1";
    out.back().expected = true;
    out.back().examples = { "0.1", "0.05", "0.01", "0.005" };
    
    return out;
}



Drover RayCastDoseAccumulate(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    auto DoseLengthMapFileName = OptArgs.getValueStr("DoseLengthMapFileName");
    auto LengthMapFileName = OptArgs.getValueStr("LengthMapFileName");
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto CylinderRadiusStr = OptArgs.getValueStr("CylinderRadius").value();
    const auto RaydLStr = OptArgs.getValueStr("RaydL").value();
    //-----------------------------------------------------------------------------------------------------------------
    const auto theregex = std::regex(ROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto thenormalizedregex = std::regex(NormalizedROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto CylinderRadius = std::stod(CylinderRadiusStr);
    const auto RaydL = std::stod(RaydLStr);

    Explicator X(FilenameLex);

    //Ensure the Ray dL is sufficiently small. We enforce that ray cannot step over the cylinder in a single iteration
    // for 95% of the width of the cylinder. So if the rays are oncoming and directed at the cylinder perpendicularly,
    // but randomly distributed over the width of the cylinder, then rays will only be able to step over the cylinder
    // without the code 'noticing' 5 times out of every 100 rays. See figure.
    //
    //                  !                       !  
    //                 \!/                     \!/  
    //       -          x                       !                  
    //       |          ! o  o                  x   o  o          -
    //    dL |         o!       o               !o        o       |
    //       |        o !        o              o          o      |   cylinder
    //       -        o x        o              o          o      | diameter/width
    //                 o!       o               xo        o       |
    //                  ! o  o                  !   o  o          -  
    //                  !                       !
    //                  x                       !
    //                  !                       x
    //                 \!/                     \!/
    //                  !                       !
    //
    //              Intersecting Ray       Non-intersecting Ray
    //
    // Note: Glancing rays will be *systematically* lost, but not all glancing rays will be lost. Only some. The amount
    //       lost will depend on the distance from the centre. The probability of a specific ray being lost depends on
    //       the phase relative to the centre.
    //
    // Note: In practice, fewer rays will be lost than predicted if they travel obliquely (not perpendicular) to the
    //       cylinders. The choice of a reasonable (or optimal) ray dL will somewhat depend on the estimated average
    //       obliquity. If there is a lot of obliquity (say, many cylinders are at 45 degrees) then the ray dL can be
    //       relaxed at the same RELATIVE error rate will be achieved. If cylinders are randomly oriented, then the same
    //       is true -- but if you want to control the ABSOLUTE error rate then you are forced to keep ray dL small
    //       enough to handle the perpendicular case. For this reason, we assume the work-case scenario (rays and
    //       cylinders are perpendicular) and hope to get a better-than expected error rate. (But it should not be worse
    //       than we predict!)
    //       
    // Note: Here is how the relationship behaves:
    //       gnuplot> plot 2.0*sqrt(1.0 - x) with impulse ls -1
    //                                                                                                    
    //                                         minRaydL / cyl_radius                                      
    //  0.9 +-+---------------+-----------------+-----------------+-----------------+---------------+-+   
    //      +||+++++          +                 +                 +                 +                 +   
    //      ||||||||++++++                          (minRaydL/cyl_radius) = 2.0*sqrt(1.0 - x) +-----+ |   
    //  0.8 +-+|||||||||||++++                                                                      +-+   
    //      ||||||||||||||||||++++++                                                                  |   
    //      ||||||||||||||||||||||||++++                                                              |   
    //  0.7 +-+|||||||||||||||||||||||||+++++                                                       +-+   
    //      |||||||||||||||||||||||||||||||||+++                                                      |   
    //  0.6 +-+|||||||||||||||||||||||||||||||||+++++                                               +-+   
    //      |||||||||||||||||||||||||||||||||||||||||++++                                             |   
    //      |||||||||||||||||||||||||||||||||||||||||||||+++                                          |   
    //  0.5 +-+|||||||||||||||||||||||||||||||||||||||||||||+++                                     +-+   
    //      |||||||||||||||||||||||||||||||||||||||||||||||||||++++                                   |   
    //      |||||||||||||||||||||||||||||||||||||||||||||||||||||||++                                 |   
    //  0.4 +-+||||||||||||||||||||||||||||||||||||||||||||||||||||||+++                            +-+   
    //      ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||++                            |   
    //      ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||++                          |   
    //  0.3 +-+|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||++                      +-+   
    //      ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||+                       |   
    //  0.2 +-+||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||++                   +-+   
    //      |||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||+                    |   
    //      ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||+                   |   
    //  0.1 +-+||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||+                +-+   
    //      ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||                  |   
    //      +|||||||||||||||||+|||||||||||||||||+|||||||||||||||||+|||||||||||||||||+                 +   
    //    0 +-+---------------+-----------------+-----------------+-----------------+---------------+-+   
    //     0.8               0.85              0.9               0.95               1                1.05 
    //                                    Fraction of rays caught/noticed
    //
    //
    const double catch_frac = 0.95; // Catch 95% of randomly distributed rays incident perpendicularly.
    const double minRaydL = 2.0 * CylinderRadius * std::sqrt(1.0 - catch_frac);
    if(RaydL < minRaydL){
        throw std::runtime_error("Ray dL is too small. Are you sure this is OK? (edit me if so).");
    } 


    //Merge the dose arrays if necessary.
    if(DICOM_data.dose_data.empty()){
        throw std::invalid_argument("This routine requires at least one dose image array. Cannot continue");
    }
    DICOM_data.dose_data = Meld_Dose_Data(DICOM_data.dose_data);
    if(DICOM_data.dose_data.size() != 1){
        throw std::invalid_argument("Unable to meld doses into a single dose array. Cannot continue.");
    }

    //auto img_arr_ptr = DICOM_data.image_data.front();
    auto img_arr_ptr = DICOM_data.dose_data.front();

    if(img_arr_ptr == nullptr){
        throw std::runtime_error("Encountered a nullptr when expecting a valid Image_Array or Dose_Array ptr.");
    }else if(img_arr_ptr->imagecoll.images.empty()){
        throw std::runtime_error("Encountered a Image_Array or Dose_Array with valid images -- no images found.");
    }


    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    std::list<std::reference_wrapper<contour_collection<double>>> cc_all;
    for(auto & cc : DICOM_data.contour_data->ccs){
        auto base_ptr = reinterpret_cast<contour_collection<double> *>(&cc);
        cc_all.push_back( std::ref(*base_ptr) );
    }

    //Whitelist contours using the provided regex.
    auto cc_ROIs = cc_all;
    cc_ROIs.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("ROIName");
                   const auto ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,theregex));
    });
    cc_ROIs.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("NormalizedROIName");
                   const auto ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,thenormalizedregex));
    });

    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    //Pre-compute the line segments and spheres we will use to define the surface boundary. 
    //
    // NOTE: We should be using a spatial indexing data structure, like R*-tree. (Does it support cylinders? Can it be
    // made to support cylinders?) TODO.
    std::vector<line_segment<double>> cylinders; // Radii are all the same: CylinderRadius.
    std::vector<vec3<double>> spheres; // Centres of the spheres. The radii are the same as the cylinder radii.

    for(const auto &cc_opt : cc_ROIs){
        for(const auto &cop : cc_opt.get().contours){
            if(cop.empty()){
                continue;
            }else if(cop.size() == 1){
                spheres.emplace_back( cop.points.back() );
            }else if(cop.size() == 2){
                spheres.emplace_back( cop.points.front() );
                spheres.emplace_back( cop.points.back() );
                if(cop.closed) cylinders.emplace_back(cop.points.front(), cop.points.back());
            }else{
                for(const auto &v : cop.points) spheres.emplace_back(v);

                auto itA = com.points.begin();
                auto itB = std::next(itA);
                for( ; itB != com.points.end(); ++itA, ++itB){
                     cylinders.emplace_back(*itB, *itA); //Orientation doesn't matter.
                }
                if(cop.closed) cylinders.emplace_back(cop.points.front(), cop.points.back());
            }
        }
    }

    //Trim geometry above some user-specified plane.
    // ... ideal? necessary? cumbersome? ...


    //Find an appropriate unit vector which will define the orientation of a plane parallel to the detector and source grids.
    // Rays will travel perpendicular to this plane, and it therefore controls.
    const auto GridNormal = vec3<double>(0.0, 0.0, 1.0).unit();
    const plane<double> GridPlane(GridNormal, vec3<double>(0.0, 0.0, 0.0));

    //Find two more directions (unit vectors) with which to align the bounding box.
    // Note: because we want to be able to compare images from different scans, we use a deterministic 
    // technique for generating two orthogonal directions involving the cardinal directions and Gram-Schmidt
    // orthogonalization.
    vec3<double> GridX = GridNormal.rotate_around_z(M_PI * 0.5); // Try Z. Will often be idempotent.
    if(GridX.Dot(GridNormal) > 0.25){
        GridX = GridNormal.rotate_around_y(M_PI * 0.5);  //Should always work since GridNormal is parallel to Z.
    }
    vec3<double> GridY = GridNormal.Cross(GridX);
    if(!GridNormal.GramSchmidt_orthogonalize(GridX, GridY)){
        throw std::runtime_error("Unable to find grid orientation vectors.");
    }
    GridX = GridX.unit();
    GridY = GridY.unit();


    //Find an appropriate bounding box encompassing the ROI surface + a margin.
    double grid_x_min = std::numeric_limits<double>::quiet_NaN();
    double grid_x_max = std::numeric_limits<double>::quiet_NaN();
    double grid_y_min = std::numeric_limits<double>::quiet_NaN();
    double grid_y_max = std::numeric_limits<double>::quiet_NaN();
    double grid_z_min = std::numeric_limits<double>::quiet_NaN();
    double grid_z_max = std::numeric_limits<double>::quiet_NaN();
    double grid_margin = 2.0*CylinderRadius;

    //Make three planes defined by GridNormal, GridX, and GridY.
    // (The planes intersect the origin so it is easier to compute offsets later.)

    // ... 

    for(const auto &asphere : spheres){

        //Compute the distance to each plane.


        //Score the minimum and maximum distances.


    }
    
    //Using the minimum and maximum distances along z, place planes at the top and bottom + a margin.

    // ...

    //Using the minimum and maximum distances along x and y, define the boundaries of the image.
    // (The images lie on the upper and lower Z planes).

    // ...


    //Now ready to ray cast. Loop over integer pixel coordinates. Start and finish are image pixels.
    // The top image can be the length image.

    // ... for(long int pc = 0; pc < image.max_coord){
    // ...     ray = ...
    // ...     while(dist(ray, lower_plane) > 0){
    // ...         ray.pos += GridNormal;
    // ...         accumulated_dose = 0;
    // ...         accumulated_length = 0; // surface length.
    // ...         if(within_spheres || within_cylinders) dose += image.pixel_at_position();
    // ...     }
    // ... }


    // Save the images to file.

    // ... 


// -------------------------------------------------------

 


    //Get the plane in which the contours are defined on.
    //
    // This is done by taking the cross product of the first few points of the first the first contour with three or
    // more points. We assume that all contours are defined by the same plane. What we are after is the normal that
    // defines the plane. We use this contour-only approach to avoid having to load CT image data.
    cc_ROIs.front().get().contours.front().Reorient_Counter_Clockwise();
    const auto planar_normal = cc_ROIs.front().get().contours.front().Estimate_Planar_Normal();

    const auto patient_ID = cc_ROIs.front().get().contours.front().GetMetadataValueAs<std::string>("PatientID").value();

    //Perform the sub-segmentation bisection, finding the two planes that flank the user's selection.
    const double acceptable_deviation = 0.01; //Deviation from desired_total_area_fraction_above_plane.
    const size_t max_iters = 50; //If the tolerance cannot be reached after this many iters, report the current plane as-is.

    std::list<contour_collection<double>> cc_selection;
    for(const auto &cc_opt : cc_ROIs){
        size_t iters_taken = 0;
        double final_area_frac = 0.0;

        //Find the lower plane.
        plane<double> lower_plane;
        cc_opt.get().Total_Area_Bisection_Along_Plane(planar_normal,
                                                      SelectionLower,
                                                      acceptable_deviation,
                                                      max_iters,
                                                      &lower_plane,
                                                      &iters_taken,
                                                      &final_area_frac);
        FUNCINFO("Lower planar extent: using bisection, the fraction of planar area"
                 " above the final lower plane was " << final_area_frac << ". Iterations taken: " << iters_taken);

        //Find the upper plane.
        plane<double> upper_plane;
        cc_opt.get().Total_Area_Bisection_Along_Plane(planar_normal,
                                                      SelectionUpper,
                                                      acceptable_deviation,
                                                      max_iters,
                                                      &upper_plane,
                                                      &iters_taken,
                                                      &final_area_frac);
        FUNCINFO("Upper planar extent: using bisection, the fraction of planar area"
                 " above the final upper plane was " << final_area_frac << ". Iterations taken: " << iters_taken);

        //Now perform the sub-segmentation, disregarding the contours outside the selection planes.
        auto split1 = cc_opt.get().Split_Along_Plane(lower_plane);
        auto split2 = split1.back().Split_Along_Plane(upper_plane);

        if(false) for(auto it = split2.begin(); it != split2.end(); ++it){ it->Plot(); }
        cc_selection.push_back( split2.front() );
    }
    if(cc_selection.empty()){
        FUNCWARN("Selection contains no contours. Try adjusting your criteria.");
    }

    //Generate lists of reference wrappers to the split contours.
    std::list<std::reference_wrapper<contour_collection<double>>> cc_selection_refs;
    for(auto &cc : cc_selection) cc_selection_refs.push_back( std::ref(cc) );

    //Accumulate the voxel intensity distributions.
    AccumulatePixelDistributionsUserData ud;
    if(!img_arr_ptr->imagecoll.Compute_Images( AccumulatePixelDistributions, { },
                                           cc_selection_refs, &ud )){
        throw std::runtime_error("Unable to accumulate pixel distributions.");
    }

    //Report the findings.
    if(DerivativeDataFileName.empty()){
        DerivativeDataFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_subsegment_vanluijk_derivatives_", 6, ".csv");
    }
    std::fstream FO_deriv(DerivativeDataFileName, std::fstream::out | std::fstream::app);
    if(!FO_deriv){
        throw std::runtime_error("Unable to open file for reporting derivative data. Cannot continue.");
    }
    for(const auto &av : ud.accumulated_voxels){
        const auto lROIname = av.first;
        const auto MeanDose = Stats::Mean( av.second );
        const auto MedianDose = Stats::Median( av.second );

        FO_deriv  << "PatientID='" << patient_ID << "',"
                  << "NormalizedROIname='" << X(lROIname) << "',"
                  << "ROIname='" << lROIname << "',"
                  << "MeanDose=" << MeanDose << ","
                  << "MedianDose=" << MedianDose << ","
                  << "VoxelCount=" << av.second.size() << std::endl;
    }
    FO_deriv.close();
 

    if(DistributionDataFileName.empty()){
        DistributionDataFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_subsegment_vanluijk_distributions_", 6, ".data");
    }
    std::fstream FO_dist(DistributionDataFileName, std::fstream::out | std::fstream::app);
    if(!FO_dist){
        throw std::runtime_error("Unable to open file for reporting distribution data. Cannot continue.");
    }

    for(const auto &av : ud.accumulated_voxels){
        const auto lROIname = av.first;
        FO_dist << "PatientID='" << patient_ID << "' "
                << "NormalizedROIname='" << X(lROIname) << "' "
                << "ROIname='" << lROIname << "' " << std::endl;
        for(const auto &d : av.second) FO_dist << d << " ";
        FO_dist << std::endl;
    }
    FO_dist.close();


    return std::move(DICOM_data);
}
