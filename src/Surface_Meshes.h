//Surface_Meshes.h - A part of DICOMautomaton 2017. Written by hal clark.

#pragma once

#include <string>
#include <utility>

#ifdef DCMA_USE_CGAL
#else
    #error "Attempted to compile without CGAL support, which is required."
#endif

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polyhedron_3.h>
#include <CGAL/Surface_mesh_default_triangulation_3.h>
#include <CGAL/boost/graph/graph_traits_Polyhedron_3.h>

namespace dcma_surface_meshes {

using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
using Polyhedron = CGAL::Polyhedron_3<Kernel>;


typedef enum {
        Fast,
        Medium,
        High
} ReproductionQuality;


struct Parameters {
    long int NumberOfImages = -1; // If not sensible, defaults to ~number of unique contour planes.

    long int GridRows = 512;  // As of writing, the procedure limiting ramping this up is inclusivity testing.
    long int GridColumns = 512;

    Mutate_Voxels_Opts MutateOpts; // Controls how contours are interpretted.

    // Control the mesh quality-vs-speed tradeoff.
    //
    // - Fast trades off final mesh quality for speed. It is useful in situations where a mesh is needed for somewhat
    //   imprecise purposes. Contour slices through the resultant mesh will most likely not reproduce the input contours
    //   very closely.
    //
    // - Medium tries to strike a balance between meshing time and final mesh quality. It should suffice for most
    //   purposes.
    //
    // - High will manipulate the mesh to try achieve the greatest reproducibility; contour slices through the resultant
    //   mesh should be nearly identical to the input contours. Note that meshes with high quality will generally have too
    //   many vertices to reasonably dilate or erode.
    ReproductionQuality RQ = ReproductionQuality::High;

};


Polyhedron
Estimate_Surface_Mesh(
        std::list<std::reference_wrapper<contour_collection<double>>> cc_ROIs,
        Parameters p );


Polyhedron
Estimate_Surface_Mesh_Marching_Cubes(
        std::list<std::reference_wrapper<contour_collection<double>>> cc_ROIs,
        Parameters p );


Polyhedron
Estimate_Surface_Mesh_Marching_Cubes(
        std::list<std::reference_wrapper<planar_image<float,double>>> grid_imgs,
        double inclusion_threshold, // The voxel value threshold demarcating surface 'interior' and 'exterior.'
        bool below_is_interior,  // Controls how the inclusion_threshold is interpretted.
                                 // If true, anything <= is considered to be interior to the surface.
                                 // If false, anything >= is considered to be interior to the surface.
        Parameters p );


Polyhedron
Estimate_Surface_Mesh_AdvancingFront(
        std::list<std::reference_wrapper<contour_collection<double>>> cc_ROIs,
        Parameters p );


Polyhedron
FVSMeshToPolyhedron(
        const fv_surface_mesh<double, uint64_t> &mesh );


} // namespace dcma_surface_meshes




namespace polyhedron_processing {

using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
using Polyhedron = CGAL::Polyhedron_3<Kernel>;


Polyhedron
Regular_Icosahedron(double radius = 1.0);

void
Subdivide(Polyhedron &mesh,
          long int iters = 3);

void
Remesh(Polyhedron &mesh,
          double target_edge_length = 1.0,
          long int iters = 3);

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

double
Volume(const Polyhedron &mesh);

double
SurfaceArea(const Polyhedron &mesh);

} // namespace polyhedron_processing

