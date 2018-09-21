//Surface_Meshes.h - A part of DICOMautomaton 2017. Written by hal clark.

#pragma once

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

//#include <CGAL/IO/write_xyz_points.h>

#include <CGAL/subdivision_method_3.h>
#include <CGAL/centroid.h>

#include <CGAL/Min_sphere_of_spheres_d.h>

#include <CGAL/boost/graph/graph_traits_Polyhedron_3.h>
#include <CGAL/mesh_segmentation.h>


namespace contour_surface_meshes {

using Kernel = CGAL::Exact_predicates_exact_constructions_kernel;
using Polyhedron = CGAL::Polyhedron_3<Kernel>;

struct Parameters {
    long int NumberOfImages = -1; // If not sensible, defaults to ~number of unique contour planes.

    long int GridRows = 512;  // As of writing, the procedure limiting ramping this up is inclusivity testing.
    long int GridColumns = 512;

    // Meshing lower bound for surface face angle (degrees).
    double MeshingAngularBound = 0.05;

    // Meshing upper bound for Delauney spheres enclosing each face and centered on the surface.
    // Note that the surface and face need intersect. This parameter controls the maximum face size.
    // Setting too large will wastefully cause too many faces on flat surfaces, but setting too large
    // can lead to sharp faces that are numerically probematic. If possible, opt for the largest face
    // sizes possible.
    //
    // Reasonable scale: 1.5 - 100.0 (DICOM units).
    double MeshingFacetSphereRadiusBound = 50.0; 

    // Meshing upper bound on the deviation from the Delauney sphere centre and the face circumcentre.
    // This parameter controls how sharp edges are approximated: lower numbers produce better approximations.
    // Setting too large will cause large surface inaccuracies in the presence of unsmooth surface features.
    // Setting too small will cause an explosion of faces. Ideally, it should probably be 1/10 of the smallest
    // surface feature size or less. Practically, this may cause massive polyhedra unsuitable for further 
    // processing.
    //
    // Reasonable scale: 0.1 - 3.0 (DICOM units).
    double MeshingCentreCentreBound = 0.25; 

};


Polyhedron
Estimate_Surface_Mesh(
        std::list<std::reference_wrapper<contour_collection<double>>> cc_ROIs,
        Parameters p );


Polyhedron
Estimate_Surface_Mesh_AdvancingFront(
        std::list<std::reference_wrapper<contour_collection<double>>> cc_ROIs,
        Parameters p );


} // namespace contour_surface_meshes




namespace polyhedron_processing {

using Kernel = CGAL::Exact_predicates_exact_constructions_kernel;
using Polyhedron = CGAL::Polyhedron_3<Kernel>;


Polyhedron
Regular_Icosahedron(void);

void
Subdivide(Polyhedron &mesh,
          long int iters);

void
Simplify(Polyhedron &mesh,
         long int edge_count_limit);

bool
SaveAsOFF(Polyhedron &mesh,
          std::string filename);


// Exact Minkowski dilation for meshes.
void
Dilate(Polyhedron &mesh,
       Polyhedron sphere);

// Exact Minkowski dilation for contour vertex point clouds.
void
Dilate( Polyhedron &output_mesh, 
        std::list<std::reference_wrapper<contour_collection<double>>> cc_ROIs,
        Polyhedron sphere );

// Approximate dilation, erosion, or core/peel using approximate Minkowski sums/differences with a sphere-like shape.
enum class TransformOp {
    Dilate,  // Grow the surface boundary.
    Erode,   // Shrink the surface boundary.
    Shell    // Keep only the outer shell (original - eroded_original).
};

void
Transform( Polyhedron &output_mesh,
           double distance,
           TransformOp op);

// Slice a polyhedron to produce planar contours on the given planes.
contour_collection<double> 
Slice_Polyhedron(
        const Polyhedron &mesh,
        std::list<plane<double>> planes );


} // namespace polyhedron_processing

