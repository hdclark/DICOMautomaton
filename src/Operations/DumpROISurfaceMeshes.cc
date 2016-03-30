//DumpROISurfaceMeshes.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

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

#include <CGAL/trace.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polyhedron_3.h>
#include <CGAL/IO/Polyhedron_iostream.h>
#include <CGAL/Surface_mesh_default_triangulation_3.h>
#include <CGAL/make_surface_mesh.h>
#include <CGAL/Implicit_surface_3.h>
#include <CGAL/IO/output_surface_facets_to_polyhedron.h>
#include <CGAL/Poisson_reconstruction_function.h>
#include <CGAL/Point_with_normal_3.h>
#include <CGAL/property_map.h>
#include <CGAL/IO/read_xyz_points.h>
#include <CGAL/compute_average_spacing.h>
#include <CGAL/edge_aware_upsample_point_set.h>

#include <CGAL/pca_estimate_normals.h>
#include <CGAL/jet_estimate_normals.h>
#include <CGAL/mst_orient_normals.h>

#include <CGAL/IO/write_xyz_points.h>

#include <CGAL/Subdivision_method_3.h>
#include <CGAL/centroid.h>

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
#include "../YgorImages_Functors/Processing/Liver_Pharmacokinetic_Model_5Param_Linear.h"
#include "../YgorImages_Functors/Processing/Liver_Pharmacokinetic_Model_5Param_Cheby.h"
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

#include "DumpROISurfaceMeshes.h"


//CGAL quantities.

typedef CGAL::Exact_predicates_inexact_constructions_kernel Kernel;
typedef Kernel::FT FT;
typedef Kernel::Point_3 Point;
typedef Kernel::Vector_3 Vector;
typedef Kernel::Triangle_3 Triangle;
//typedef CGAL::Point_with_normal_3<Kernel> Point_with_normal;
typedef std::pair<Point, Vector> Point_with_normal;
typedef std::list<Point_with_normal> PointList;
typedef Kernel::Sphere_3 Sphere;
typedef CGAL::Polyhedron_3<Kernel> Polyhedron;
typedef CGAL::Poisson_reconstruction_function<Kernel> Poisson_reconstruction_function;
typedef CGAL::Surface_mesh_default_triangulation_3 STr;
typedef CGAL::Surface_mesh_complex_2_in_triangulation_3<STr> C2t3;
typedef CGAL::Implicit_surface_3<Kernel, Poisson_reconstruction_function> Surface_3;


/*

// function for writing the reconstruction output in the off format
static void dump_reconstruction(const Reconstruction &reconstruct, std::string name){
  std::ofstream output(name.c_str());
  output << "OFF " << reconstruct.number_of_points() << " "
         << reconstruct.number_of_triangles() << " 0\n";
  std::copy(reconstruct.points_begin(),
            reconstruct.points_end(),
            std::ostream_iterator<Point>(output,"\n"));
  for( Reconstruction::Triple_const_iterator it = reconstruct.surface_begin(); it != reconstruct.surface_end(); ++it )
      output << "3 " << *it << std::endl;

  return;
}
*/



std::list<OperationArgDoc> OpArgDocDumpROISurfaceMeshes(void){
    std::list<OperationArgDoc> out;

    out.emplace_back();
    out.back().name = "OutDir";
    out.back().desc = "The directory in which to dump surface mesh files."
                      " It will be created if it doesn't exist.";
    out.back().default_val = "/tmp/";
    out.back().expected = true;
    out.back().examples = { "./", "../somedir/", "/path/to/some/destination" };


    out.emplace_back();
    out.back().name = "ROILabelRegex";
    out.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses grep syntax.";
    out.back().default_val = ".*";
    out.back().expected = true;
    out.back().examples = { ".*", ".*body.*", "body", "Gross_Liver", 
                            R"***(.*parotid.*|.*sub.*mand.*)***", 
                            R"***(left_parotid|right_parotid|eyes)***" };


    return out;
}

Drover DumpROISurfaceMeshes(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //This operation generates surface meshes from contour volumes. Output is written to file(s) for viewing with an
    // external viewer (e.g., meshlab).
    // 

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto OutDir = OptArgs.getValueStr("OutDir").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    //-----------------------------------------------------------------------------------------------------------------
    //const auto theregex = std::regex(ROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::grep);
    const auto theregex = std::regex(ROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    bool use_pca_normal_estimation = true;
    bool use_jet_normal_estimation = false;


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


    //Gather all contours for each volume of interest.
    typedef std::tuple<std::string,std::string,std::string> key_t; //PatientID, ROIName, NormalizedROIName.
    typedef std::list<std::reference_wrapper< contour_of_points<double>>> val_t; 

    std::map<key_t,val_t> ROIs;
    if(DICOM_data.contour_data != nullptr){
        //for(auto & cc : DICOM_data.contour_data->ccs){
        //    for(auto & c : cc.contours){
        for(auto & cc_refw : cc_ROIs){
            for(auto & c : cc_refw.get().contours){
                key_t key = std::tie(c.metadata["PatientID"], c.metadata["ROIName"], c.metadata["NormalizedROIName"]);
                ROIs[key].push_back( std::ref(c) );
            }
        }
    }

    //For each volume of interest, generate a surface mesh.
    //
    // TODO: will this handle unconnected organs, e.g., left+right parotids, in the correct way?
    //
    for(auto & anROI : ROIs){
        const auto PatientID = std::get<0>(anROI.first);
        const auto ROIName   = std::get<1>(anROI.first);

        FUNCINFO("Generating surface for structure '" << ROIName << "'");

        const auto OutFile = OutDir + "/ROI_Surface_Mesh_" + PatientID + "_" + ROIName + ".off";

        std::list<Point_with_normal> points;
        for(auto & c : anROI.second){
            //const auto subdiv_c = c.get().Subdivide_Midway().Subdivide_Midway();
            const auto subdiv_c = c.get();


            //Assuming the contour is planar, determine the normal.
            const auto centroid = subdiv_c.Centroid();
            const auto pA = *(subdiv_c.points.begin());
            const auto pB = *(++subdiv_c.points.begin());
            const auto N = (pB-centroid).Cross(pA-centroid).unit();

            //Assume or determine some thickness for the contour. (Should be distance to nearest-neighbour or linear
            // voxel size of some sort).
            const double thickness = 3.0;

            //Lay down some points over the volume defined by the extruded contour 'disc'.
            //for(double thick = -thickness * 0.5 ; thick <= thickness * 0.5; thick += thickness * 0.25 ){
            for(double thick = 0.0 ; thick <= 0.0; thick += 1.0 ){
                for(const auto &p : subdiv_c.points){
                    auto pp = p + N * thick;
                    points.emplace_back( std::make_pair( Point(pp.x, pp.y, pp.z), CGAL::NULL_VECTOR ) );
                }
            }
        }


        // Estimates normals direction.
        // Note: pca_estimate_normals() requires an iterator over points
        // as well as property maps to access each point's position and normal.
        const int nb_neighbors = 6*5; // K-nearest neighbors = 3 rings

        if(use_pca_normal_estimation){
            CGAL::pca_estimate_normals(points.begin(), points.end(),
                                       CGAL::First_of_pair_property_map<Point_with_normal>(),
                                       CGAL::Second_of_pair_property_map<Point_with_normal>(),
                                       nb_neighbors);
                                       //CGAL::Identity_property_map<Point_with_normal>(),
                                       //CGAL::make_normal_of_point_with_normal_pmap(PointList::value_type()),

        }else if(use_jet_normal_estimation){
            CGAL::jet_estimate_normals(points.begin(), points.end(),
                                       CGAL::First_of_pair_property_map<Point_with_normal>(),
                                       CGAL::Second_of_pair_property_map<Point_with_normal>(),
                                       nb_neighbors);
                                       //CGAL::Identity_property_map<Point_with_normal>(),
                                       //CGAL::make_normal_of_point_with_normal_pmap(PointList::value_type()),
        }else{
            throw std::runtime_error("No normal estimation method specified");
        }

        // Orients normals.
        // Note: mst_orient_normals() requires an iterator over points
        // as well as property maps to access each point's position and normal.
        std::list<Point_with_normal>::iterator unoriented_points_begin =
            CGAL::mst_orient_normals(points.begin(), points.end(),
                                     CGAL::First_of_pair_property_map<Point_with_normal>(),
                                     CGAL::Second_of_pair_property_map<Point_with_normal>(),
                                     nb_neighbors);
                                     //CGAL::Identity_property_map<Point_with_normal>(),
                                     //CGAL::make_normal_of_point_with_normal_pmap(PointList::value_type()),

        // Optional: delete points with an unoriented normal
        // if you plan to call a reconstruction algorithm that expects oriented normals.
        points.erase(unoriented_points_begin, points.end());
 
        // Saves point set for comparison.
        {
            std::ofstream out(OutFile + ".raw_contour_points.xyz");  
            if (!out || !CGAL::write_xyz_points_and_normals(out, points.begin(), points.end(), 
                                 CGAL::First_of_pair_property_map<Point_with_normal>(),
                                 CGAL::Second_of_pair_property_map<Point_with_normal>())){
                throw std::runtime_error("Unable to write point set file");
            } 
        }
  
        //Algorithm parameters
        const double sharpness_angle = 25;   // control sharpness of the result.
        const double edge_sensitivity = 0;    // higher values will sample more points near the edges          
        const double neighbor_radius = 0.0;  // initial size of neighborhood.
        const unsigned int number_of_output_points = points.size() * 2;
        //Run algorithm 
        CGAL::edge_aware_upsample_point_set(
                 points.begin(), 
                 points.end(), 
                 std::back_inserter(points),
                 CGAL::First_of_pair_property_map<Point_with_normal>(),
                 CGAL::Second_of_pair_property_map<Point_with_normal>(),
                 //CGAL::Identity_property_map<Point_with_normal>(),
                 //CGAL::make_normal_of_point_with_normal_pmap(PointList::value_type()),
                 sharpness_angle, 
                 edge_sensitivity,
                 neighbor_radius,
                 number_of_output_points);
  
        // Saves point set for comparison.
        {
            std::ofstream out(OutFile + ".after_upsampling.xyz");  
            if (!out || !CGAL::write_xyz_points_and_normals(out, points.begin(), points.end(), 
                                 CGAL::First_of_pair_property_map<Point_with_normal>(),
                                 CGAL::Second_of_pair_property_map<Point_with_normal>())){
                throw std::runtime_error("Unable to write point set file");
            } 
        }
  

        // Poisson options
        FT sm_angle = 5.0; // Min triangle angle in degrees.
        FT sm_radius = 50; // Max triangle size w.r.t. point set average spacing.
        FT sm_distance = 0.375; // Surface Approximation error w.r.t. point set average spacing.
        // Reads the point set file in points[].

        // Creates implicit function from the read points using the default solver.
        // Note: this method requires an iterator over points
        // + property maps to access each point's position and normal.
        // The position property map can be omitted here as we use iterators over Point_3 elements.
        Poisson_reconstruction_function function(points.begin(), points.end(),
                                                 CGAL::First_of_pair_property_map<Point_with_normal>(),
                                                 CGAL::Second_of_pair_property_map<Point_with_normal>() );
                                                 //CGAL::make_normal_of_point_with_normal_pmap(PointList::value_type()) );
                                                 
        // Computes the Poisson indicator function at each vertex of the triangulation.
        if( !function.compute_implicit_function() ){
            throw std::runtime_error("Could not compute implicit surface function"); 
        }

        // Computes average spacing
        FT average_spacing = CGAL::compute_average_spacing(points.begin(), points.end(),
                                                           CGAL::First_of_pair_property_map<Point_with_normal>(),
                                                           6); // 6 knn = 1 ring.

        // Gets one point inside the implicit surface and computes implicit function bounding sphere radius.
        Point inner_point = function.get_inner_point();
        Sphere bsphere = function.bounding_sphere();
        FT radius = std::sqrt(bsphere.squared_radius());
        // Defines the implicit surface: requires defining a conservative bounding sphere centered at inner point.
        FT sm_sphere_radius = 5.0 * radius;
        FT sm_dichotomy_error = sm_distance*average_spacing/1000.0; // Dichotomy error must be << sm_distance
        Surface_3 surface(function,
                          Sphere(inner_point,sm_sphere_radius*sm_sphere_radius),
                          sm_dichotomy_error/sm_sphere_radius);
        // Defines surface mesh generation criteria
        CGAL::Surface_mesh_default_criteria_3<STr> criteria(sm_angle,  // Min triangle angle (degrees)
                                                            sm_radius*average_spacing,  // Max triangle size
                                                            sm_distance*average_spacing); // Approximation error

        // Generates surface mesh with manifold option
        STr tr; // 3D Delaunay triangulation for surface mesh generation
        C2t3 c2t3(tr); // 2D complex in 3D Delaunay triangulation
        CGAL::make_surface_mesh(c2t3,                                 // reconstructed mesh
                                surface,                              // implicit surface
                                criteria,                             // meshing criteria
                                CGAL::Manifold_with_boundary_tag());  // require manifold mesh
        if(tr.number_of_vertices() == 0) throw std::runtime_error("No vertices in generated mesh");

        // Generate a Polyhedron mesh from the surface facets. Should work because manifold requested.
        Polyhedron output_mesh;
        CGAL::output_surface_facets_to_polyhedron(c2t3, output_mesh);

        // Subdivide the surface.
        const int subdiv_iters = 2;
        //Subdivision_method_3::CatmullClark_subdivision(output_mesh,subdiv_iters);
        CGAL::Subdivision_method_3::Loop_subdivision(output_mesh,subdiv_iters);


        // Compute the barycentre for faces of the polyhedron.
        std::vector<Triangle> triangles;
        triangles.reserve( output_mesh.size_of_facets() );

        for(auto facet_it = output_mesh.facets_begin(); facet_it != output_mesh.facets_end(); ++facet_it){
            if(!facet_it->is_triangle()){
                throw std::runtime_error("Encountered a non-triangle facet");
                //We can probably handle this by checking if the facet is a quad via ->is_quad() or using a
                // Halfedge_facet_circulator. Of course then we will probably have to compute the centroid ourselves via
                // the CGAL weighted 3D point centroid calculator by weighting the barycentre of each facet with the
                // facet's area.
                //
                //However, currently I think we can assume we only have triangles.
            }
 
            auto halfedge_handle = facet_it->halfedge();
            auto p1 = halfedge_handle->vertex()->point();
            auto p2 = halfedge_handle->next()->vertex()->point();
            auto p3 = halfedge_handle->next()->next()->vertex()->point();
            triangles.push_back(Triangle(p1,p2,p3)); 
        }
        auto centroid = CGAL::centroid(triangles.begin(), triangles.end(), CGAL::Dimension_tag<2>());

        {
            std::vector<Point> centroid_v;
            centroid_v.push_back(centroid);

            std::ofstream out(OutFile + ".centroid.xyz");  
            if (!out || !CGAL::write_xyz_points(out, centroid_v.begin(), centroid_v.end() )){
                throw std::runtime_error("Unable to write surface centroid");
            } 
        }
  

        // Apply an Affine transformation to the vertices of a polyhedron.
        //std::transform( output_mesh.points_begin(), output_mesh.points_end(), output_mesh.points_begin(), A);


        // Generate a Nef-Polyhedron.
        // Nef_polyhedron_3 (Polyhedron &P).
        // http://doc.cgal.org/latest/Nef_3/classCGAL_1_1Nef__polyhedron__3.html#a4f7d5c63440b89153cb971e022e0ef76


        // Save reconstructed surface mesh
        std::ofstream out(OutFile);
        out << output_mesh;
        //dump_reconstruction(reconstruct, OutFile);

    }


/*
    Explicator X(FilenameLex);
    for(auto & NameCount : NameCounts){
        //Simply dump the suspected mapping.
        //std::cout << "PatientID='" << std::get<0>(NameCount.first) << "'\t"
        //          << "ROIName='" << std::get<1>(NameCount.first) << "'\t"
        //          << "NormalizedROIName='" << std::get<2>(NameCount.first) << "'\t"
        //          << "Contours='" << NameCount.second << "'"
        //          << std::endl;

        //Print out the best few guesses for each raw contour name.
        const auto ROIName = std::get<1>(NameCount.first);
        X(ROIName);
        std::unique_ptr<std::map<std::string,float>> res(std::move(X.Get_Last_Results()));
        std::vector<std::pair<std::string,float>> ordered_res(res->begin(), res->end());
        std::sort(ordered_res.begin(), ordered_res.end(), 
                  [](const std::pair<std::string,float> &L, const std::pair<std::string,float> &R) -> bool {
                      return L.second > R.second;
                  });
        if(ordered_res.size() != 1) for(size_t i = 0; i < ordered_res.size(); ++i){
            std::cout << ordered_res[i].first << " : " << ROIName; // << std::endl;
            std::cout << std::endl;
        }
    }
    std::cout << std::endl;
*/

    
    return std::move(DICOM_data);
}
