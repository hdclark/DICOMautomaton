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

#include <CGAL/Subdivision_method_3.h>

#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits.h>
#include <CGAL/boost/graph/graph_traits_Polyhedron_3.h>
#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <boost/optional.hpp>


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
#include "../Thread_Pool.h"
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
#include "../YgorImages_Functors/Compute/GenerateSurfaceMask.h"

#include "SurfaceBasedRayCastDoseAccumulate.h"



std::list<OperationArgDoc> OpArgDocSurfaceBasedRayCastDoseAccumulate(void){
    std::list<OperationArgDoc> out;

/*
    out.emplace_back();
    out.back().name = "DoseMapFileName";
    out.back().desc = "A filename (or full path) for the dose image map."
                      " Note that this file is approximate, and may not be accurate."
                      " There is more information available when you use the length and dose*length maps instead."
                      " However, this file is useful for viewing and eyeballing tuning settings."
                      " The format is TBD. Leave empty to dump to generate a unique temporary file.";  // TODO: specify format.
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "", "/tmp/somefile", "localfile.img", "derivative_data.img" };
*/

    out.emplace_back();
    out.back().name = "TotalDoseMapFileName";
    out.back().desc = "A filename (or full path) for the total dose image map (at all ray-surface intersection points)."
                      " The format is TBD. Leave empty to dump to generate a unique temporary file.";  // TODO: specify format.
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "", "/tmp/somefile", "localfile.img", "derivative_data.img" };


    out.emplace_back();
    out.back().name = "IntersectionCountMapFileName";
    out.back().desc = "A filename (or full path) for the (length traveled through the ROI peel) image map."
                      " The format is TBD. Leave empty to dump to generate a unique temporary file.";  // TODO: specify format.
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "", "/tmp/somefile", "localfile.img", "derivative_data.img" };


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
    out.back().desc = "The number of rows in the resulting images."
                      " Setting too fine relative to the surface mask grid or dose grid is futile.";
    out.back().default_val = "1024";
    out.back().expected = true;
    out.back().examples = { "10", "50", "128", "1024" };
    
    out.emplace_back();
    out.back().name = "SourceDetectorColumns";
    out.back().desc = "The number of columns in the resulting images."
                      " Setting too fine relative to the surface mask grid or dose grid is futile.";
    out.back().default_val = "1024";
    out.back().expected = true;
    out.back().examples = { "10", "50", "128", "1024" };
    
    return out;
}



Drover SurfaceBasedRayCastDoseAccumulate(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    auto TotalDoseMapFileName = OptArgs.getValueStr("TotalDoseMapFileName").value();
    auto IntersectionCountMapFileName = OptArgs.getValueStr("IntersectionCountMapFileName").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ReferenceROILabelRegex = OptArgs.getValueStr("ReferenceROILabelRegex").value();
    const auto NormalizedReferenceROILabelRegex = OptArgs.getValueStr("NormalizedReferenceROILabelRegex").value();
    const auto SourceDetectorRows = std::stol(OptArgs.getValueStr("SourceDetectorRows").value());
    const auto SourceDetectorColumns = std::stol(OptArgs.getValueStr("SourceDetectorColumns").value());

    //-----------------------------------------------------------------------------------------------------------------
    const auto roiregex = std::regex(ROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto roinormalizedregex = std::regex(NormalizedROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto refregex = std::regex(ReferenceROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto refnormalizedregex = std::regex(NormalizedReferenceROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    Explicator X(FilenameLex);

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
        typedef double FT;
        typedef CGAL::Cartesian<FT> K;
        typedef CGAL::Tag_true UseSqrts;
        typedef CGAL::Min_sphere_of_spheres_d_traits_3<K,FT,UseSqrts> Traits;
        typedef CGAL::Min_sphere_of_spheres_d<Traits> Min_sphere;
        typedef K::Point_3 Point;
        typedef Traits::Sphere Sphere;

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
   
    // default triangulation for Surface_mesher
    typedef CGAL::Surface_mesh_default_triangulation_3 Tr;

    // c2t3
    typedef CGAL::Complex_2_in_triangulation_3<Tr> C2t3;
    typedef Tr::Geom_traits GT;
    typedef GT::Sphere_3 Sphere_3;
    typedef GT::Point_3 Point_3;
    typedef GT::FT FT;


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
    Surface_3 surface(surface_oracle, cgal_bounding_sphere);

    // defining meshing criteria
    CGAL::Surface_mesh_default_criteria_3<Tr> criteria(1.0,  // angular bound
                                                       5.0,   // radius bound
                                                       5.0);  // distance bound
    // meshing surface
    CGAL::make_surface_mesh(c2t3, surface, criteria, CGAL::Manifold_tag());
    FUNCINFO("The triangulated surface has " << tr.number_of_vertices() << " vertices");

    {
        std::ofstream out("/tmp/thesurface.off");
        CGAL::output_surface_facets_to_off(out,c2t3);
    }

    //Convert to a polyhedron.
    typedef CGAL::Exact_predicates_inexact_constructions_kernel Kernel;
    typedef CGAL::Polyhedron_3<Kernel> Polyhedron;
    //typedef Polyhedron::Halfedge_handle Halfedge_handle;
    Polyhedron polyhedron;

    if(!CGAL::output_surface_facets_to_polyhedron(c2t3, polyhedron)){
        throw std::runtime_error("Could not convert surface mesh to a polyhedron representation. Is it manifold & orientable?");
    }

    //Perform surface subdivision.
    CGAL::Subdivision_method_3::Loop_subdivision(polyhedron,2);
    FUNCINFO("The subdivided triangulated surface has " << polyhedron.size_of_vertices() << " vertices"
             " and " << polyhedron.size_of_facets() << " faces");

    {
        std::ofstream out("/tmp/thesubdividedsurface.off");
        out << polyhedron;
    }
    if(!polyhedron.is_pure_triangle()) throw std::runtime_error("Mesh is not purely triangular.");

    // ============================================== Trim Facets ==================================================
    // We need to trim facets that are irrelevant. These facets are defined by their proximity to the prostate.

    // ...

    // =============================== Construct an AABB Tree for Spatial Lookups ==================================

    //typedef CGAL::Exact_predicates_inexact_constructions_kernel Kernel;
    typedef Kernel::Point_3 Point;
    typedef Kernel::Plane_3 Plane;
    typedef Kernel::Vector_3 Vector;
    typedef Kernel::Segment_3 Segment;
    typedef Kernel::Ray_3 Ray;
    typedef CGAL::AABB_face_graph_triangle_primitive<Polyhedron> Triangle_Primitive;
    typedef CGAL::AABB_traits<Kernel, Triangle_Primitive> Traits;
    typedef CGAL::AABB_tree<Traits> AABB_Tree;
    typedef boost::optional< AABB_Tree::Intersection_and_primitive_id<Segment>::Type > Segment_intersection;
    typedef boost::optional< AABB_Tree::Intersection_and_primitive_id<Plane>::Type > Plane_intersection;
    typedef AABB_Tree::Primitive_id Primitive_id;


    //Construct the tree.
    AABB_Tree tree(faces(polyhedron).first, faces(polyhedron).second, polyhedron);
    // The tree is now ready to query.







    // constructs segment query
    Point a(-1000.0, -10000.0, -10000.0);
    Point b(bounding_sphere_center.x, bounding_sphere_center.y, bounding_sphere_center.z);
    Segment segment_query(a,b);

    // tests intersections with segment query
    if(tree.do_intersect(segment_query)){
        std::cout << "intersection(s)" << std::endl;
    }else{
        std::cout << "no intersection" << std::endl;
    }

    // computes # of intersections with segment query
    std::cout << tree.number_of_intersected_primitives(segment_query) << " intersection(s)" << std::endl;

    // computes first encountered intersection with segment query
    // (generally a point)
    Segment_intersection intersection = tree.any_intersection(segment_query);
    if(intersection){
        // gets intersection object
      const Point* p = boost::get<Point>(&(intersection->first));
      if(p){
        std::cout << "intersection object is a point " << *p << std::endl;
      }
    }

    // computes all intersections with segment query (as pairs object - primitive_id)
    std::list<Segment_intersection> intersections;
    tree.all_intersections(segment_query, std::back_inserter(intersections));


    // computes all intersected primitives with segment query as primitive ids
    std::list<Primitive_id> primitives;
    tree.all_intersected_primitives(segment_query, std::back_inserter(primitives));






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
    auto sd_image_collection = Contiguously_Grid_Volume<float,double>(
             cc_ROIs, 
             sdgrid_x_margin, sdgrid_y_margin, sdgrid_z_margin,
             SourceDetectorRows, SourceDetectorColumns, /*number_of_channels=*/ 1, 100*NumberOfImages, 
             SDGridX, SDGridY, SDGridZ,
             /*pixel_fill=*/ std::numeric_limits<double>::quiet_NaN(),
             /*only_top_and_bottom=*/ true);

    planar_image<float, double> *DetectImg = &(sd_image_collection.images.front());
    DetectImg->metadata["Description"] = "Total Dose Map";
    planar_image<float, double> *SourceImg = &(sd_image_collection.images.back());
    SourceImg->metadata["Description"] = "Intersection Count Map (number of ray-surface intersectons)";


    // ============================================== Ray-cast ==============================================

    //Now ready to ray cast. Loop over integer pixel coordinates. Start and finish are image pixels.
    // The top image can be the length image.
    {
        asio_thread_pool tp;
        std::mutex printer; // Who gets to print to the console and iterate the counter.
        long int completed = 0;

        //Perform a virtual cleave on the surface by advancing ("boosting") all intersecting rays to the cleaving plane.
        const double cleaved_gap_dist = std::abs(ROICleaving.Get_Signed_Distance_To_Point(ROI_centroid));

        for(long int row = 0; row < SourceDetectorRows; ++row){
            tp.submit_task([&,row](void) -> void {
                for(long int col = 0; col < SourceDetectorColumns; ++col){

                    //Construct a line segment between the source (or offset source where a virtual cleave was performed) and detector. 
                    double accumulated_counts = 0.0;      //The number of ray-surface intersections.
                    double accumulated_totaldose = 0.0;   //The total accumulated dose from all intersections.
                    const vec3<double> ray_end = DetectImg->position(row, col);
                    const vec3<double> ray_naive = SourceImg->position(row, col); // The naive starting position, without boosting.
                    const vec3<double> ray_dir = (ray_end - ray_naive).unit();
                    const vec3<double> ray_start = ray_naive + ray_dir * cleaved_gap_dist;

                    //Compute the point intersections. "Glancing" intersections are ignored.
                    Segment line_segment( Point(ray_start.x, ray_start.y, ray_start.z),
                                          Point(ray_end.x,   ray_end.y,   ray_end.z)   );

                    //Fast check for intersections.
                    if(tree.do_intersect(line_segment)){

                        //Enumerate all intersections. Note that some may be line segment "glances."
                        std::list<Segment_intersection> intersections;
                        tree.all_intersections(line_segment, std::back_inserter(intersections));

                        //Cycle through the intersections.
                        for(const auto & intersection : intersections){
                            if(intersection){
                                const Point* p = boost::get<Point>(&(intersection->first));
                                if(p){
                                    //Convert from CGAL vector to Ygor vector.
                                    const vec3<double> P(static_cast<double>(p->x()),
                                                         static_cast<double>(p->y()),
                                                         static_cast<double>(p->z()));

                                    if(!ROICleaving.Is_Point_Above_Plane(P)){
                                        //Find the dose at the intersection point.
                                        auto encompass_imgs = dose_arr_ptr->imagecoll.get_images_which_encompass_point( P );
                                        for(const auto &enc_img : encompass_imgs){
                                            const auto pix_val = enc_img->value(P, 0);
                                            accumulated_totaldose += pix_val;
                                        }
                                        accumulated_counts += 1.0;
                                    
//                {
//                    std::lock_guard<std::mutex> lock(printer);
//                    FUNCINFO("Intersection point: R,C = " << row << ", " << col << " ; P = " << P << " ; Count = " <<
//                    accumulated_counts << " ; Dose = " << accumulated_totaldose);
//                }
                                    }
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
    if(!WriteToFITS(*SourceImg, IntersectionCountMapFileName)){
        throw std::runtime_error("Unable to write FITS file for intersection count map.");
    }
    if(!WriteToFITS(*DetectImg, TotalDoseMapFileName)){
        throw std::runtime_error("Unable to write FITS file for total dose map.");
    }

    // Insert the image maps as images for later processing and/or viewing, if desired.
    DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>() );
    DICOM_data.image_data.back()->imagecoll = sd_image_collection;

    return std::move(DICOM_data);
}
