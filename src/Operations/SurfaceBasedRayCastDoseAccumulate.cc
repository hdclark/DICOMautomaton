//SurfaceBasedRayCastDoseAccumulate.cc - A part of DICOMautomaton 2015-2017. Written by hal clark.

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

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>

#include <CGAL/Cartesian.h>
#include <CGAL/Min_sphere_of_spheres_d.h>

#include <CGAL/Surface_mesh_default_triangulation_3.h>
#include <CGAL/Surface_mesh_default_criteria_3.h>
#include <CGAL/Complex_2_in_triangulation_3.h>
#include <CGAL/make_surface_mesh.h>
#include <CGAL/Implicit_surface_3.h>

#include <CGAL/IO/Complex_2_in_triangulation_3_file_writer.h>
#include <CGAL/IO/output_surface_facets_to_polyhedron.h>

#include <CGAL/Polyhedron_3.h>
#include <CGAL/IO/Polyhedron_iostream.h>

#include <CGAL/subdivision_method_3.h>

#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits.h>
#include <CGAL/boost/graph/graph_traits_Polyhedron_3.h>
#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <boost/optional.hpp>


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
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../Dose_Meld.h"

#include "../YgorImages_Functors/Grouping/Misc_Functors.h"

#include "../YgorImages_Functors/Processing/DCEMRI_AUC_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_S0_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_T1_Map.h"
#include "../YgorImages_Functors/Processing/Partitioned_Image_Voxel_Visitor_Mutator.h"
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
#include "../YgorImages_Functors/Compute/AccumulatePixelDistributions.h"
#include "../YgorImages_Functors/Compute/GenerateSurfaceMask.h"

#include "SurfaceBasedRayCastDoseAccumulate.h"



std::list<OperationArgDoc> OpArgDocSurfaceBasedRayCastDoseAccumulate(void){
    std::list<OperationArgDoc> out;


    out.emplace_back();
    out.back().name = "TotalDoseMapFileName";
    out.back().desc = "A filename (or full path) for the total dose image map (at all ray-surface intersection points)."
                      " The dose for each ray is summed over all ray-surface point intersections."
                      " The format is FITS. This file is always generated."
                      " Leave the argument empty to generate a unique filename.";
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "", "total_dose_map.fits", "/tmp/out.fits" };
    out.back().mimetype = "image/fits";


    out.emplace_back();
    out.back().name = "IntersectionCountMapFileName";
    out.back().desc = "A filename (or full path) for the (number of ray-surface intersections) image map."
                      " Each pixel in this map (and the total dose map) represents a single ray;"
                      " the number of times the ray intersects the surface can be useful for various purposes,"
                      " but most often it will simply be a sanity check for the cross-sectional shape or that "
                      " a specific number of intersections were recorded in regions with geometrical folds."
                      " Pixels will all be within [0,MaxRaySurfaceIntersections]."
                      " The format is FITS. Leave empty to dump to generate a unique filename.";
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "", "intersection_count_map.fits", "/tmp/out.fits" };
    out.back().mimetype = "image/fits";


    out.emplace_back();
    out.back().name = "DepthMapFileName";
    out.back().desc = "A filename (or full path) for the distance (depth) of each ray-surface intersection point from"
                      " the detector. Has DICOM coordinate system units. This image is potentially multi-channel with"
                      " [MaxRaySurfaceIntersections] channels (when MaxRaySurfaceIntersections = 1 there is 1 channel)."
                      " The format is FITS. Leaving empty will result in no file being written.";
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "", "depth_map.fits", "/tmp/out.fits" };
    out.back().mimetype = "image/fits";


    out.emplace_back();
    out.back().name = "RadialDistMapFileName";
    out.back().desc = "A filename (or full path) for the distance of each ray-surface intersection point from the"
                      " line joining reference and target ROI centre-of-masses. This helps quantify position in 3D."
                      " Has DICOM coordinate system units. This image is potentially multi-channel with"
                      " [MaxRaySurfaceIntersections] channels (when MaxRaySurfaceIntersections = 1 there is 1 channel)."
                      " The format is FITS. Leaving empty will result in no file being written.";
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "", "radial_dist_map.fits", "/tmp/out.fits" };
    out.back().mimetype = "image/fits";


    out.emplace_back();
    out.back().name = "ROISurfaceMeshFileName";
    out.back().desc = "A filename (or full path) for the (pre-subdivided) surface mesh that is contructed from the ROI contours."
                      " The format is OFF. This file is mostly useful for inspection of the surface or comparison with contours."
                      " Leaving empty will result in no file being written.";
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "", "/tmp/roi_surface_mesh.off", "roi_surface_mesh.off" };
    out.back().mimetype = "application/off";


    out.emplace_back();
    out.back().name = "SubdividedROISurfaceMeshFileName";
    out.back().desc = "A filename (or full path) for the Loop-subdivided surface mesh that is contructed from the ROI contours."
                      " The format is OFF. This file is mostly useful for inspection of the surface or comparison with contours."
                      " Leaving empty will result in no file being written.";
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "", "/tmp/subdivided_roi_surface_mesh.off", "subdivided_roi_surface_mesh.off" };
    out.back().mimetype = "application/off";


    out.emplace_back();
    out.back().name = "ROICOMCOMLineFileName";
    out.back().desc = "A filename (or full path) for the line segment that connected the centre-of-mass (COM) of"
                      " reference and target ROI. The format is OFF."
                      " This file is mostly useful for inspection of the surface or comparison with contours."
                      " Leaving empty will result in no file being written.";
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "", "/tmp/roi_com_com_line.off", "roi_com_com_line.off" };


    out.emplace_back();
    out.back().name = "NormalizedReferenceROILabelRegex";
    out.back().desc = "A regex matching reference ROI labels/names to consider. The default will match"
                      " all available ROIs, which is non-sensical. The reference ROI is used to orient"
                      " the cleaving plane to trim the grid surface mask.";
    out.back().default_val = ".*";
    out.back().expected = true;
    out.back().examples = { ".*", ".*Prostate.*", "Left Kidney", "Gross Liver" };


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
    out.back().name = "ReferenceROILabelRegex";
    out.back().desc = "A regex matching reference ROI labels/names to consider. The default will match"
                      " all available ROIs, which is non-sensical. The reference ROI is used to orient"
                      " the cleaving plane to trim the grid surface mask.";
    out.back().default_val = ".*";
    out.back().expected = true;
    out.back().examples = { ".*", ".*[pP]rostate.*", "body", "Gross_Liver",
                            R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                            R"***(left_parotid|right_parotid)***" };


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
    out.back().name = "SourceDetectorRows";
    out.back().desc = "The number of rows in the resulting images, which also defines how many rays are used."
                      " (Each pixel in the source image represents a single ray.)"
                      " Setting too fine relative to the surface mask grid or dose grid is futile.";
    out.back().default_val = "1024";
    out.back().expected = true;
    out.back().examples = { "100", "128", "1024", "4096" };
    

    out.emplace_back();
    out.back().name = "SourceDetectorColumns";
    out.back().desc = "The number of columns in the resulting images."
                      " (Each pixel in the source image represents a single ray.)"
                      " Setting too fine relative to the surface mask grid or dose grid is futile.";
    out.back().default_val = "1024";
    out.back().expected = true;
    out.back().examples = { "100", "128", "1024", "4096" };
    

    out.emplace_back();
    out.back().name = "MeshingAngularBound";
    out.back().desc = "The minimum internal angle each triangle facet must have in the surface mesh (in degrees)."
                      " The computation may become unstable if an angle larger than 30 degree is specified."
                      " Note that for intersection purposes triangles with small angles isn't a big deal."
                      " Rather, having a large minimal angle can constrain the surface in strange ways."
                      " Consult the CGAL '3D Surface Mesh Generation' package documentation for additional info.";
    out.back().default_val = "1.0";
    out.back().expected = true;
    out.back().examples = { "1.0", "10.0", "25.0", "30.0" };
    

    out.emplace_back();
    out.back().name = "MeshingFacetSphereRadiusBound";
    out.back().desc = "The maximum radius of facet-bounding spheres, which are centred on each facet (one per facet)"
                      " and grown as large as possible without enclosing any facet vertices."
                      " In a nutshell, this controls the maximum individual facet size."
                      " Units are in DICOM space. Setting too low will cause triangulation to be slow and many facets;"
                      " it is recommended instead to rely on subdivision to create a smooth surface approximation."
                      " Consult the CGAL '3D Surface Mesh Generation' package documentation for additional info.";
    out.back().default_val = "5.0";
    out.back().expected = true;
    out.back().examples = { "1.0", "2.0", "5.0" };
   

    out.emplace_back();
    out.back().name = "MeshingCentreCentreBound";
    out.back().desc = "The maximum facet centre-centre distance between facet circumcentres and facet-bounding spheres,"
                      " which are centred on each facet (one per facet) and grown as large as possible without enclosing"
                      " any facet vertices. In a nutshell, this controls the trade-off between minimizing deviation from"
                      " the (implicit) ROI contour-derived surface and having smooth connectivity between facets."
                      " Units are in DICOM space. Setting too low will cause triangulation to be slow and many facets;"
                      " it is recommended instead to rely on subdivision to create a smooth surface approximation."
                      " Consult the CGAL '3D Surface Mesh Generation' package documentation for additional info.";
    out.back().default_val = "5.0";
    out.back().expected = true;
    out.back().examples = { "1.0", "2.0", "5.0", "10.0" };
   

    out.emplace_back();
    out.back().name = "MeshingSubdivisionIterations";
    out.back().desc = "The number of iterations of Loop's subdivision to apply to the surface mesh."
                      " The aim of subdivision in this context is to have a smooth surface to work with, but too many"
                      " applications will create too many facets. More facets will not lead to more precise results"
                      " beyond a certain (modest) amount of smoothing. If the geometry is relatively spherical already,"
                      " and meshing bounds produce reasonably smooth (but 'blocky') surface meshes, then 2-3"
                      " iterations should suffice. More than 3-4 iterations will almost always be inappropriate.";
    out.back().default_val = "2";
    out.back().expected = true;
    out.back().examples = { "0", "1", "2", "3" };


    out.emplace_back();
    out.back().name = "MaxRaySurfaceIntersections";
    out.back().desc = "The maximum number of ray-surface intersections to accumulate before retiring each ray."
                      " Note that intersections are sorted spatially by their distance to the detector, and those"
                      " closest to the detector are considered first."
                      " If the ROI surface is opaque, setting this value to 1 will emulate visibility."
                      " Setting to 2 will permit rays continue through the ROI and pass through the other side;"
                      " dose will be the accumulation of dose at each ray-surface intersection."
                      " This value should most often be 1 or some very high number (e.g., 1000) to make the surface"
                      " either completely opaque or completely transparent. (A transparent surface may help to"
                      " visualize geometrical 'folds' or other surface details of interest.)";
    out.back().default_val = "1";
    out.back().expected = true;
    out.back().examples = { "1", "4", "1000"};


    out.emplace_back();
    out.back().name = "OnlyGenerateSurface";
    out.back().desc = "Stop processing after writing the surface and subdivided surface meshes."
                      " This option is primarily used for debugging and visualization.";
    out.back().default_val = "false";
    out.back().expected = true;
    out.back().examples = { "true", "false" };

    return out;
}



Drover SurfaceBasedRayCastDoseAccumulate(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){
    //
    // This routine uses rays (actually: line segments) to estimate point-dose on the surface of an ROI. The ROI is 
    // approximated by surface mesh and rays are passed through. Dose is interpolated at the intersection points and
    // intersecting lines (i.e., where the ray 'glances' the surface) are discarded. The surface reconstruction can be
    // tweaked, but appear to reasonably approximate the ROI contours; both can be output to compare visually.
    //
    // Though it is not required by the implementation, only the ray-surface intersection nearest to the detector is
    // considered. All other intersections (i.e., on the far side of the surface mesh) are ignored.
    //
    // This routine is fairly fast compared to the slow grid-based counterpart previously implemented. The speedup comes
    // from use of an AABB-tree to accelerate intersection queries and avoid having to 'walk' rays step-by-step through
    // over/through the geometry. 
    //

    //---------------------------------------------- User Parameters --------------------------------------------------
    auto TotalDoseMapFileName = OptArgs.getValueStr("TotalDoseMapFileName").value();
    auto IntersectionCountMapFileName = OptArgs.getValueStr("IntersectionCountMapFileName").value();
    auto DepthMapFileName = OptArgs.getValueStr("DepthMapFileName").value();
    auto RadialDistMapFileName = OptArgs.getValueStr("RadialDistMapFileName").value();

    auto ROISurfaceMeshFileName = OptArgs.getValueStr("ROISurfaceMeshFileName").value();
    auto SubdividedROISurfaceMeshFileName = OptArgs.getValueStr("SubdividedROISurfaceMeshFileName").value();
    auto ROICOMCOMLineFileName = OptArgs.getValueStr("ROICOMCOMLineFileName").value();

    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ReferenceROILabelRegex = OptArgs.getValueStr("ReferenceROILabelRegex").value();
    const auto NormalizedReferenceROILabelRegex = OptArgs.getValueStr("NormalizedReferenceROILabelRegex").value();

    const auto SourceDetectorRows = std::stol(OptArgs.getValueStr("SourceDetectorRows").value());
    const auto SourceDetectorColumns = std::stol(OptArgs.getValueStr("SourceDetectorColumns").value());

    const auto MeshingAngularBound = std::stod(OptArgs.getValueStr("MeshingAngularBound").value());
    const auto MeshingFacetSphereRadiusBound = std::stod(OptArgs.getValueStr("MeshingFacetSphereRadiusBound").value());
    const auto MeshingCentreCentreBound = std::stod(OptArgs.getValueStr("MeshingCentreCentreBound").value());

    const auto MeshingSubdivisionIterations = std::stol(OptArgs.getValueStr("MeshingSubdivisionIterations").value());
    const auto MaxRaySurfaceIntersections = std::stol(OptArgs.getValueStr("MaxRaySurfaceIntersections").value());

    const auto OnlyGenerateSurfaceStr = OptArgs.getValueStr("OnlyGenerateSurface").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto roiregex = std::regex(ROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto roinormalizedregex = std::regex(NormalizedROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto refregex = std::regex(ReferenceROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto refnormalizedregex = std::regex(NormalizedReferenceROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto TrueRegex = std::regex("^tr?u?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    Explicator X(FilenameLex);


    //Boolean options.
    const auto OnlyGenerateSurface = std::regex_match(OnlyGenerateSurfaceStr, TrueRegex);

    //Merge the dose arrays if necessary.
    if(DICOM_data.dose_data.empty()){
        throw std::invalid_argument("This routine requires at least one dose image array. Cannot continue");
    }
    DICOM_data.dose_data = Meld_Dose_Data(DICOM_data.dose_data);
    if(DICOM_data.dose_data.size() != 1){
        throw std::invalid_argument("Unable to meld doses into a single dose array. Cannot continue.");
    }

    //Look for dose data.
    //auto dose_arr_ptr = DICOM_data.image_data.front();
    auto dose_arr_ptr = DICOM_data.dose_data.front();
    if(dose_arr_ptr == nullptr){
        throw std::runtime_error("Encountered a nullptr when expecting a valid Image_Array or Dose_Array ptr.");
    }else if(dose_arr_ptr->imagecoll.images.empty()){
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
                   return !(std::regex_match(ROIName,roiregex));
    });
    cc_ROIs.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("NormalizedROIName");
                   const auto ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,roinormalizedregex));
    });

    auto cc_Refs = cc_all;
    cc_Refs.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("ROIName");
                   const auto ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,refregex));
    });
    cc_Refs.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("NormalizedROIName");
                   const auto ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,refnormalizedregex));
    });

    if(cc_ROIs.empty()){
        throw std::invalid_argument("No ROI contours selected. Cannot continue.");
    }else if(cc_Refs.empty()){
        throw std::invalid_argument("No ReferenceROI contours selected. Cannot continue.");
    }


    // ====================================== Generate a Surface from ROIs only =======================================
    //This sub-routine assumes the ROI contours are 'cylindrically' extruded 2D polygons. ROIs do not communicate or
    // interpolate and are completely ignorant of one another (except to determine their extrusion distance or
    // "thickness"). The surfaces of adjacent contours are planar ("top" and "bottom") and the polygon line segments 
    // define rectangular, orthogonal "side" facets. 
    //
    // This routine is designed to be used with a subdivision routine to smooth/round the sharp ridges.
    //
    // NOTE: This routine assumes all ROIs are co-planar.
    //

    //Figure out plane alignment and work out spacing. (Spacing is twice the thickness.)
    const auto est_cont_normal = cc_ROIs.front().get().contours.front().Estimate_Planar_Normal();
    const auto ucp = Unique_Contour_Planes(cc_ROIs, est_cont_normal, /*distance_eps=*/ 0.005);
    
    const double fallback_spacing = 2.5; // in DICOM units.
    const auto cont_sep_range = std::abs(ucp.front().Get_Signed_Distance_To_Point( ucp.back().R_0 ));
    const double est_cont_spacing = (ucp.size() <= 1) ? fallback_spacing : cont_sep_range / static_cast<double>(ucp.size() - 1);
    const double est_cont_thickness = 0.5005 * est_cont_spacing; // Made slightly thicker to avoid gaps.


    //Construct a sphere surrounding the vertices to bound the surface.
    vec3<double> bounding_sphere_center;
    double bounding_sphere_radius; 
    const double extra_space = 1.0; //Extra space around each point, in DICOM coords.
    {
        using FT = double;
        using K = CGAL::Cartesian<FT>;
        using UseSqrts = CGAL::Tag_true;
        typedef CGAL::Min_sphere_of_spheres_d_traits_3<K,FT,UseSqrts> Traits;
        using Min_sphere = CGAL::Min_sphere_of_spheres_d<Traits>;
        using Point = K::Point_3;
        using Sphere = Traits::Sphere;

        std::vector<Sphere> spheres;
        for(const auto & cc_ref : cc_ROIs){
            const auto cc = cc_ref.get();
            for(const auto & c : cc.contours){    
                for(const auto & p : c.points){
                    Point cgal_p( p.x, p.y, p.z );
                    spheres.emplace_back(cgal_p,extra_space);
                }
            }
        }

        Min_sphere ms(spheres.begin(),spheres.end());
        if(!ms.is_valid()) throw std::runtime_error("Unable to compute a bounding sphere. Cannot continue.");

        auto center_coord_it = ms.center_cartesian_begin();
        bounding_sphere_center.x = *(center_coord_it++);
        bounding_sphere_center.y = *(center_coord_it++);
        bounding_sphere_center.z = *(center_coord_it);
        bounding_sphere_radius = ms.radius();

        //Ensure the sphere's centre is somewhere in the ROI by translating to some point within a contour and growing the sphere.
        vec3<double> nearest_incontour_p = cc_ROIs.front().get().contours.front().First_N_Point_Avg(3);
        for(const auto & cc_ref : cc_ROIs){
            const auto cc = cc_ref.get();
            for(const auto & c : cc.contours){ 
                const auto incontour_p = c.Get_Point_Within_Contour();
                if(incontour_p.sq_dist(bounding_sphere_center) < nearest_incontour_p.sq_dist(bounding_sphere_center)){
                    nearest_incontour_p = incontour_p;
                }
            }
        }
        bounding_sphere_radius += nearest_incontour_p.distance(bounding_sphere_center); //A superset enclosing sphere.
        bounding_sphere_center = nearest_incontour_p;
    }
    FUNCINFO("Finished computing bounding sphere for selected ROIs; centre, radius = " << bounding_sphere_center << ", " << bounding_sphere_radius);

    //Pre-compute least-squares planes and contour projections onto those planes (in case of non-planar contours).
    // This substantially speeds up the computation.
    std::vector< std::pair< plane<double>, contour_of_points<double> > > projected_contours;
    for(const auto & cc_ref : cc_ROIs){
        const auto cc = cc_ref.get();
        for(const auto & c : cc.contours){    
            const auto c_plane = c.Least_Squares_Best_Fit_Plane(est_cont_normal);
            projected_contours.emplace_back( std::make_pair(c_plane, c.Project_Onto_Plane_Orthogonally(c_plane)) );
        }
    }
   
    using Tr = CGAL::Surface_mesh_default_triangulation_3;
    using C2t3 = CGAL::Complex_2_in_triangulation_3<Tr>;
    using GT = Tr::Geom_traits;
    using Sphere_3 = GT::Sphere_3;
    using Point_3 = GT::Point_3;
    using FT = GT::FT;

    auto surface_oracle = [&](Point_3 p) -> FT {
        //This routine is an 'oracle' that reports if a given point is inside or outside the surface to be triangulated.
        // The inside of the surface returns negative numbers and the outside returns positive numbers. The surface is
        // defined by the isosurface where this function is zero.
        //
        // Note that this oracle provides only binary information (in or out).
        //
        const vec3<double> P(static_cast<double>(p.x()), static_cast<double>(p.y()), static_cast<double>(p.z()));

        for(const auto & pc : projected_contours){
            //Check if the point is bounded by the top and bottom planes.
            const auto within_planar_bounds = ( std::abs(pc.first.Get_Signed_Distance_To_Point(P)) < est_cont_thickness);
            if(!within_planar_bounds) continue;

            //Check if the point is within the polygon bounds when projected onto its plane.
            const auto proj_P = pc.first.Project_Onto_Plane_Orthogonally(P);
            const auto already_proj = true;
            const auto within_poly = pc.second.Is_Point_In_Polygon_Projected_Orthogonally(pc.first,proj_P,already_proj);
            if(!within_poly) continue;

            return static_cast<FT>(-1.0);
        }
        return static_cast<FT>(1.0);
    };

    const Point_3 cgal_bounding_sphere_center(bounding_sphere_center.x, bounding_sphere_center.y, bounding_sphere_center.z );
    const Sphere_3 cgal_bounding_sphere( cgal_bounding_sphere_center, std::pow(bounding_sphere_radius,2.0) );
 
    //Verify the center of the sphere is internal to the surface, as is required by the surface mesher.
    if(surface_oracle(cgal_bounding_sphere_center) >= 0.0){
        throw std::runtime_error("Bounding sphere's centre is not within the surface."
                                 " The sphere will need to be tweaked to place the centre within the surface but"
                                 " still bound all vertices.");
    }

    typedef CGAL::Implicit_surface_3<GT, decltype(surface_oracle)> Surface_3;

    Tr tr;            // 3D-Delaunay triangulation
    C2t3 c2t3(tr);    // 2D-complex in 3D-Delaunay triangulation

    // defining the surface
    const auto surface_error_bound = static_cast<FT>(1.0e-5); //Relative to bounding volume.
    Surface_3 surface(surface_oracle, cgal_bounding_sphere, surface_error_bound);

    // defining meshing criteria
    CGAL::Surface_mesh_default_criteria_3<Tr> criteria(MeshingAngularBound,
                                                       MeshingFacetSphereRadiusBound,
                                                       MeshingCentreCentreBound);
    // meshing surface
    CGAL::make_surface_mesh(c2t3, surface, criteria, CGAL::Manifold_tag());
    FUNCINFO("The triangulated surface has " << tr.number_of_vertices() << " vertices");

    if(!ROISurfaceMeshFileName.empty()){
        std::ofstream out(ROISurfaceMeshFileName);
        CGAL::output_surface_facets_to_off(out,c2t3);
    }

    //Convert to a polyhedron.
    using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
    using Polyhedron = CGAL::Polyhedron_3<Kernel>;
    //typedef Polyhedron::Halfedge_handle Halfedge_handle;
    Polyhedron polyhedron;

    if(!CGAL::output_surface_facets_to_polyhedron(c2t3, polyhedron)){
        throw std::runtime_error("Could not convert surface mesh to a polyhedron representation. Is it manifold & orientable?");
    }

    //Perform surface subdivision.
    CGAL::Subdivision_method_3::Loop_subdivision(polyhedron,MeshingSubdivisionIterations);
    FUNCINFO("The subdivided triangulated surface has " << polyhedron.size_of_vertices() << " vertices"
             " and " << polyhedron.size_of_facets() << " faces");

    if(!SubdividedROISurfaceMeshFileName.empty()){
        std::ofstream out(SubdividedROISurfaceMeshFileName);
        out << polyhedron;
    }
    if(!polyhedron.is_pure_triangle()) throw std::runtime_error("Mesh is not purely triangular.");

    if(OnlyGenerateSurface) return DICOM_data;

    // =============================== Construct an AABB Tree for Spatial Lookups ==================================

    //typedef CGAL::Exact_predicates_inexact_constructions_kernel Kernel;
    using Point = Kernel::Point_3;
    using Segment = Kernel::Segment_3;
    using Triangle_Primitive = CGAL::AABB_face_graph_triangle_primitive<Polyhedron>;
    typedef CGAL::AABB_traits<Kernel, Triangle_Primitive> Traits;
    using AABB_Tree = CGAL::AABB_tree<Traits>;
    using Segment_intersection = boost::optional<AABB_Tree::Intersection_and_primitive_id<Segment>::Type>;

    //Construct the tree.
    AABB_Tree tree(faces(polyhedron).first, faces(polyhedron).second, polyhedron);


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

    //Figure out what a reasonable x-margin and y-margin are. 
    //
    // NOTE: Could also use (median? maximum?) distance from centroid to vertex.
    double x_margin = z_margin;
    double y_margin = z_margin;


    //Compute centroids for the ROI and Reference ROI volumes.
    vec3<double> ROI_centroid;
    vec3<double> Ref_centroid;
    {
        contour_collection<double> cc_ROIs_All;
        for(const auto &cc_ref : cc_ROIs){
            for(const auto &c : cc_ref.get().contours) cc_ROIs_All.contours.push_back(c);
        }

        contour_collection<double> cc_Refs_All;
        for(const auto &cc_ref : cc_Refs){ // Different references. One in the c++ sense, another in the 'reference textbook' sense.
            for(const auto &c : cc_ref.get().contours) cc_Refs_All.contours.push_back(c);
        }

        ROI_centroid = cc_ROIs_All.Centroid();
        Ref_centroid = cc_Refs_All.Centroid();
    }

    //Create a plane at the Bladder's centroid aligned with the ROI (bladder) that faces away from the referenceROI
    // (prostate).
    const plane<double> ROICleaving( (ROI_centroid - Ref_centroid).unit(), ROI_centroid );
    const line<double> COM_COM_line(Ref_centroid, ROI_centroid); 

    if(!ROICOMCOMLineFileName.empty()){
        const line_segment<double> ls(Ref_centroid, ROI_centroid);
        if(!WriteLineSegmentToOFF(ls, ROICOMCOMLineFileName, "Reference ROI and target ROI COM-COM line segment.")){
            throw std::runtime_error("Unable to write COM-COM line segment to file. (Is there an existing file?)");
        }
    }

    // ============================================== Source, Detector creation  ==============================================
    //Create source and detector images. 
    //
    // NOTE: They do not need to be aligned with the geometry, contours, or grid. But leave a big margin so you
    //       can ensure you're getting all the surface available.

    //const auto SDGridZ = vec3<double>(0.0, 1.0, 1.0).unit();
    const auto SDGridZ = ROICleaving.N_0.unit();
    vec3<double> SDGridY = vec3<double>(1.0, 0.0, 0.0);
    if(SDGridY.Dot(SDGridZ) > 0.25){
        SDGridY = SDGridZ.rotate_around_x(M_PI * 0.5);
    }
    vec3<double> SDGridX = SDGridZ.Cross(SDGridY);
    if(!SDGridZ.GramSchmidt_orthogonalize(SDGridY, SDGridX)){
        throw std::runtime_error("Unable to find grid orientation vectors.");
    }
    SDGridX = SDGridX.unit();
    SDGridY = SDGridY.unit();

    //Hope that using a margin twice the grid margin will capture all jutting surface.
    double sdgrid_x_margin = 2.0*x_margin;
    double sdgrid_y_margin = 2.0*y_margin;
    double sdgrid_z_margin = 2.0*z_margin;

    //Generate a grid volume bounding the ROI(s). We ask for many images in order to compress the pxl_dz taken by each.
    // Only two are actually allocated.
    const auto NumberOfImages = (ucp.size() + 2);
    auto sd_image_collection = Symmetrically_Contiguously_Grid_Volume<float,double>(
             cc_ROIs, 
             sdgrid_x_margin, sdgrid_y_margin, sdgrid_z_margin,
             SourceDetectorRows, SourceDetectorColumns, /*number_of_channels=*/ 1, 100*NumberOfImages, 
             COM_COM_line, SDGridX, SDGridY,
             /*pixel_fill=*/ std::numeric_limits<double>::quiet_NaN(),
             /*only_top_and_bottom=*/ true);

    //Generate two additional image maps for ray-surface intersection coordinates. These images are potentially multi-channel.
    // Reinitialize them ASAP.
    sd_image_collection.images.emplace_back( sd_image_collection.images.front() );
    sd_image_collection.images.back().init_buffer(SourceDetectorRows, SourceDetectorColumns, MaxRaySurfaceIntersections);
    sd_image_collection.images.emplace_back( sd_image_collection.images.front() );
    sd_image_collection.images.back().init_buffer(SourceDetectorRows, SourceDetectorColumns, MaxRaySurfaceIntersections);

    //Get handles for each image.
    planar_image<float, double> *DetectImg     = &(*std::next(sd_image_collection.images.begin(),0));
    planar_image<float, double> *SourceImg     = &(*std::next(sd_image_collection.images.begin(),1));
    planar_image<float, double> *DepthImg      = &(*std::next(sd_image_collection.images.begin(),2));
    planar_image<float, double> *RadialDistImg = &(*std::next(sd_image_collection.images.begin(),3));

    DetectImg->metadata["Description"]     = "Total Dose Map";
    SourceImg->metadata["Description"]     = "Intersection Count Map (number of Ray-Surface Intersectons)";
    DepthImg->metadata["Description"]      = "Ray-surface Depth Intersection Map";
    RadialDistImg->metadata["Description"] = "Radial Distance from COM-COM line to Ray-Surface Intersection";

    const auto detector_plane = DetectImg->image_plane();


    // ============================================== Ray-cast ==============================================

    //Now ready to ray cast. Loop over integer pixel coordinates. Start and finish are image pixels.
    // The top image can be the length image.
    {
        asio_thread_pool tp;
        std::mutex printer; // Who gets to print to the console and iterate the counter.
        long int completed = 0;

        for(long int row = 0; row < SourceDetectorRows; ++row){
            tp.submit_task([&,row](void) -> void {
                for(long int col = 0; col < SourceDetectorColumns; ++col){

                    //Construct a line segment between the source and detector. 
                    long int accumulated_counts = 0;      //The number of ray-surface intersections.
                    double accumulated_totaldose = 0.0;   //The total accumulated dose from all intersections.
                    const vec3<double> ray_start = SourceImg->position(row, col); // The naive starting position, without boosting.
                    const vec3<double> ray_end = DetectImg->position(row, col);

                    Segment line_segment( Point(ray_start.x, ray_start.y, ray_start.z),
                                          Point(ray_end.x,   ray_end.y,   ray_end.z)   );

                    //Fast check for intersections.
                    if(tree.do_intersect(line_segment)){

                        //Enumerate all intersections. Note that some may be line segment "glances."
                        std::list<Segment_intersection> intersections;
                        tree.all_intersections(line_segment, std::back_inserter(intersections));

                        //Sort by distance from the detector so the first intersection is closest to the detector.
                        intersections.sort([&](const Segment_intersection &A, const Segment_intersection &B) -> bool {
                            const Point *pA = boost::get<Point>(&(A->first));
                            const Point *pB = boost::get<Point>(&(B->first));
                            if( (pA) && (pB) ){ // Both valid points.
                                const vec3<double> PA(static_cast<double>(pA->x()),
                                                      static_cast<double>(pA->y()),
                                                      static_cast<double>(pA->z()));
                                const vec3<double> PB(static_cast<double>(pB->x()),
                                                      static_cast<double>(pB->y()),
                                                      static_cast<double>(pB->z()));
                                return std::abs( detector_plane.Get_Signed_Distance_To_Point(PA) ) 
                                          < std::abs( detector_plane.Get_Signed_Distance_To_Point(PB) );
                            }else if((pA) && !(pB)){
                                return true;
                            }else if(!(pA) && (pB)){
                                return false;
                            }
                            return false; //Both non-points.

                        });

                        //Cycle through the intersections stopping after the point nearest the detector is located.
                        for(const auto & intersection : intersections){
                            if(intersection){
                                const Point* p = boost::get<Point>(&(intersection->first));
                                if(p){
                                    //Convert from CGAL vector to Ygor vector.
                                    const vec3<double> P(static_cast<double>(p->x()),
                                                         static_cast<double>(p->y()),
                                                         static_cast<double>(p->z()));

                                    //Compute the distance to the detector.
                                    const auto P_src_dist = std::abs( detector_plane.Get_Signed_Distance_To_Point(P) );
                                    DepthImg->reference(row, col, accumulated_counts) = static_cast<float>( P_src_dist );

                                    //Compute the distance to the COM-COM line (between target ROI and reference ROI).
                                    const auto P_rad_dist = COM_COM_line.Distance_To_Point(P);
                                    RadialDistImg->reference(row, col, accumulated_counts) = static_cast<float>( P_rad_dist );

                                    //Find the dose at the intersection point.
                                    const auto interp_val = dose_arr_ptr->imagecoll.trilinearly_interpolate(P,0);

                                    accumulated_totaldose += interp_val;
                                    ++accumulated_counts;

                                    //Terminate the loop after desired number of intersections.
                                    if(accumulated_counts >= MaxRaySurfaceIntersections) break;
                                }
                            }
                        }
                    }

                    //Deposit the dose in the images.
                    SourceImg->reference(row, col, 0) = static_cast<float>(accumulated_counts);
                    DetectImg->reference(row, col, 0) = static_cast<float>(accumulated_totaldose);
                }

                {
                    std::lock_guard<std::mutex> lock(printer);
                    ++completed;
                    FUNCINFO("Completed " << completed << " of " << SourceDetectorRows 
                          << " --> " << static_cast<int>(1000.0*(completed)/SourceDetectorRows)/10.0 << "\% done");
                }
            });
        }
    } // Complete tasks and terminate thread pool.


    // Save image maps to file.
    if(TotalDoseMapFileName.empty()){
        TotalDoseMapFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_surfaceraycast_totaldose_", 6, ".fits");
    }
    if(IntersectionCountMapFileName.empty()){
        IntersectionCountMapFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_surfaceraycast_intersection_count_", 6, ".fits");
    }


    if(!WriteToFITS(*SourceImg, IntersectionCountMapFileName)){
        throw std::runtime_error("Unable to write FITS file for intersection count map.");
    }
    if(!WriteToFITS(*DetectImg, TotalDoseMapFileName)){
        throw std::runtime_error("Unable to write FITS file for total dose map.");
    }
    if(!DepthMapFileName.empty() && !WriteToFITS(*DepthImg, DepthMapFileName)){
        throw std::runtime_error("Unable to write FITS file for depth map.");
    }
    if(!RadialDistMapFileName.empty() && !WriteToFITS(*RadialDistImg, RadialDistMapFileName)){
        throw std::runtime_error("Unable to write FITS file for radial distance map.");
    }

    // Insert the image maps as images for later processing and/or viewing, if desired.
    DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>() );
    DICOM_data.image_data.back()->imagecoll = sd_image_collection;

    return DICOM_data;
}
