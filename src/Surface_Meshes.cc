//Surface_Meshes.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>    
#include <vector>
#include <map>
#include <list>
#include <functional>
#include <array>
#include <mutex>
#include <limits>
#include <cmath>
#include <cstdint>

#include <utility>            //Needed for std::pair.
#include <algorithm>

#ifdef DCMA_USE_CGAL
    #define BOOST_PARAMETER_MAX_ARITY 12

    #include <CGAL/subdivision_method_3.h>
    #include <CGAL/OFF_to_nef_3.h>
    #include <CGAL/Min_sphere_of_spheres_d.h>

    #include <CGAL/Polygon_mesh_slicer.h>
    #include <CGAL/Polygon_mesh_processing/orientation.h>
    #include <CGAL/Polygon_mesh_processing/remesh.h>
    #include <CGAL/Polygon_mesh_processing/repair.h>

    #include <CGAL/Surface_mesh_simplification/edge_collapse.h>
    #include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Count_stop_predicate.h>
    #include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Edge_length_cost.h>
    #include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Midpoint_placement.h>

    #include <CGAL/Advancing_front_surface_reconstruction.h>
    #include <CGAL/Surface_mesh.h>
    //#include <CGAL/disable_warnings.h>

    #include <CGAL/minkowski_sum_3.h>

    #include <CGAL/boost/graph/convert_nef_polyhedron_to_polygon_mesh.h>

    #include <CGAL/Mesh_triangulation_3.h>
    #include <CGAL/Mesh_complex_3_in_triangulation_3.h>
    #include <CGAL/Mesh_criteria_3.h>

    #include <CGAL/Implicit_mesh_domain_3.h> // Deprecation handling: for CGAL v4.13 and earlier.
    //#include <CGAL/Labeled_mesh_domain_3.h>  // Deprecation handling: for CGAL v4.13 and later.
    #include <CGAL/Mesh_domain_with_polyline_features_3.h>
    #include <CGAL/make_mesh_3.h>
#endif // DCMA_USE_CGAL

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorImages.h"

#include "Thread_Pool.h"
#include "Structs.h"
#include "CSG_SDF.h"

#include "YgorImages_Functors/Grouping/Misc_Functors.h"
#include "YgorImages_Functors/Processing/Partitioned_Image_Voxel_Visitor_Mutator.h"

#include "Surface_Meshes.h"


// ----------------------------------------------- Pure contour meshing -----------------------------------------------
namespace dcma_surface_meshes {

// Marching Cubes core implementation. This routine must be fed an image volume.
//
// NOTE: This implementation borrows from the public domain implementation available at
//       <https://paulbourke.net/geometry/polygonise/marchingsource.cpp> (accessed 20190217).
//       The header lists Cory Bloyd as the author. The implementation provided here extends the public domain
//       version to support rectangular cubes, avoid 3D interpolation, and explicitly constructs a polyhedron mesh.
//       Thanks Cory! Thanks Paul!
//

static
fv_surface_mesh<double, uint64_t>
Marching_Cubes_Implementation(
        std::list<std::reference_wrapper<planar_image<float,double>>> grid_imgs,
        std::shared_ptr<csg::sdf::node> sdf,
        double inclusion_threshold, // The voxel value threshold demarcating surface 'interior' and 'exterior.'
        bool below_is_interior,  // Controls how the inclusion_threshold is interpretted.
                                 // If true, anything <= is considered to be interior to the surface.
                                 // If false, anything >= is considered to be interior to the surface.
        Parameters /*params*/ ){

    const double ExteriorVal = inclusion_threshold + (below_is_interior ? 1.0 : -1.0);

    if(grid_imgs.empty()){
        throw std::invalid_argument("An insufficient number of images was provided. Cannot continue.");
    }
    if(!Images_Form_Rectilinear_Grid(grid_imgs)){
        throw std::logic_error("Grid images do not form a rectilinear grid. Cannot continue");
    }

    const auto GridZ = grid_imgs.front().get().image_plane().N_0;

    const vec3<double> zero3( static_cast<double>(0),
                              static_cast<double>(0),
                              static_cast<double>(0) );

    planar_image_adjacency<float,double> img_adj( grid_imgs, {}, GridZ );

    const bool has_signed_dist_func = (sdf != nullptr);

    // ============================================== Marching Cubes ================================================

    // Use a curb vertex inclusivity int (8 bits) to determine which (of 12) edges are intersected by the ROI surface.
    std::array<int32_t, 256> aiCubeEdgeFlags { {
        0b000000000000, 0b000100001001, 0b001000000011, 0b001100001010, 0b010000000110, 0b010100001111, 0b011000000101,
        0b011100001100, 0b100000001100, 0b100100000101, 0b101000001111, 0b101100000110, 0b110000001010, 0b110100000011,
        0b111000001001, 0b111100000000, 0b000110010000, 0b000010011001, 0b001110010011, 0b001010011010, 0b010110010110,
        0b010010011111, 0b011110010101, 0b011010011100, 0b100110011100, 0b100010010101, 0b101110011111, 0b101010010110,
        0b110110011010, 0b110010010011, 0b111110011001, 0b111010010000, 0b001000110000, 0b001100111001, 0b000000110011,
        0b000100111010, 0b011000110110, 0b011100111111, 0b010000110101, 0b010100111100, 0b101000111100, 0b101100110101,
        0b100000111111, 0b100100110110, 0b111000111010, 0b111100110011, 0b110000111001, 0b110100110000, 0b001110100000,
        0b001010101001, 0b000110100011, 0b000010101010, 0b011110100110, 0b011010101111, 0b010110100101, 0b010010101100,
        0b101110101100, 0b101010100101, 0b100110101111, 0b100010100110, 0b111110101010, 0b111010100011, 0b110110101001,
        0b110010100000, 0b010001100000, 0b010101101001, 0b011001100011, 0b011101101010, 0b000001100110, 0b000101101111,
        0b001001100101, 0b001101101100, 0b110001101100, 0b110101100101, 0b111001101111, 0b111101100110, 0b100001101010,
        0b100101100011, 0b101001101001, 0b101101100000, 0b010111110000, 0b010011111001, 0b011111110011, 0b011011111010,
        0b000111110110, 0b000011111111, 0b001111110101, 0b001011111100, 0b110111111100, 0b110011110101, 0b111111111111,
        0b111011110110, 0b100111111010, 0b100011110011, 0b101111111001, 0b101011110000, 0b011001010000, 0b011101011001,
        0b010001010011, 0b010101011010, 0b001001010110, 0b001101011111, 0b000001010101, 0b000101011100, 0b111001011100,
        0b111101010101, 0b110001011111, 0b110101010110, 0b101001011010, 0b101101010011, 0b100001011001, 0b100101010000,
        0b011111000000, 0b011011001001, 0b010111000011, 0b010011001010, 0b001111000110, 0b001011001111, 0b000111000101,
        0b000011001100, 0b111111001100, 0b111011000101, 0b110111001111, 0b110011000110, 0b101111001010, 0b101011000011,
        0b100111001001, 0b100011000000, 0b100011000000, 0b100111001001, 0b101011000011, 0b101111001010, 0b110011000110,
        0b110111001111, 0b111011000101, 0b111111001100, 0b000011001100, 0b000111000101, 0b001011001111, 0b001111000110,
        0b010011001010, 0b010111000011, 0b011011001001, 0b011111000000, 0b100101010000, 0b100001011001, 0b101101010011,
        0b101001011010, 0b110101010110, 0b110001011111, 0b111101010101, 0b111001011100, 0b000101011100, 0b000001010101,
        0b001101011111, 0b001001010110, 0b010101011010, 0b010001010011, 0b011101011001, 0b011001010000, 0b101011110000,
        0b101111111001, 0b100011110011, 0b100111111010, 0b111011110110, 0b111111111111, 0b110011110101, 0b110111111100,
        0b001011111100, 0b001111110101, 0b000011111111, 0b000111110110, 0b011011111010, 0b011111110011, 0b010011111001,
        0b010111110000, 0b101101100000, 0b101001101001, 0b100101100011, 0b100001101010, 0b111101100110, 0b111001101111,
        0b110101100101, 0b110001101100, 0b001101101100, 0b001001100101, 0b000101101111, 0b000001100110, 0b011101101010,
        0b011001100011, 0b010101101001, 0b010001100000, 0b110010100000, 0b110110101001, 0b111010100011, 0b111110101010,
        0b100010100110, 0b100110101111, 0b101010100101, 0b101110101100, 0b010010101100, 0b010110100101, 0b011010101111,
        0b011110100110, 0b000010101010, 0b000110100011, 0b001010101001, 0b001110100000, 0b110100110000, 0b110000111001,
        0b111100110011, 0b111000111010, 0b100100110110, 0b100000111111, 0b101100110101, 0b101000111100, 0b010100111100,
        0b010000110101, 0b011100111111, 0b011000110110, 0b000100111010, 0b000000110011, 0b001100111001, 0b001000110000,
        0b111010010000, 0b111110011001, 0b110010010011, 0b110110011010, 0b101010010110, 0b101110011111, 0b100010010101,
        0b100110011100, 0b011010011100, 0b011110010101, 0b010010011111, 0b010110010110, 0b001010011010, 0b001110010011,
        0b000010011001, 0b000110010000, 0b111100000000, 0b111000001001, 0b110100000011, 0b110000001010, 0b101100000110,
        0b101000001111, 0b100100000101, 0b100000001100, 0b011100001100, 0b011000000101, 0b010100001111, 0b010000000110,
        0b001100001010, 0b001000000011, 0b000100001001, 0b000000000000
    } };

    // Determine which triangulation (0-5 triangles) is needed given the edge-surface intersections.
    std::array< std::array<int32_t, 16>, 256> a2iTriangleConnectionTable  { {
           { -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  8,  3,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  1,  9,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  8,  3,    9,  8,  1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  2, 10,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  8,  3,    1,  2, 10,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9,  2, 10,    0,  2,  9,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  2,  8,  3,    2, 10,  8,   10,  9,  8,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  3, 11,  2,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0, 11,  2,    8, 11,  0,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  9,  0,    2,  3, 11,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1, 11,  2,    1,  9, 11,    9,  8, 11,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  3, 10,  1,   11, 10,  3,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0, 10,  1,    0,  8, 10,    8, 11, 10,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  3,  9,  0,    3, 11,  9,   11, 10,  9,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9,  8, 10,   10,  8, 11,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  4,  7,  8,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  4,  3,  0,    7,  3,  4,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  1,  9,    8,  4,  7,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  4,  1,  9,    4,  7,  1,    7,  3,  1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  2, 10,    8,  4,  7,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  3,  4,  7,    3,  0,  4,    1,  2, 10,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9,  2, 10,    9,  0,  2,    8,  4,  7,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  2, 10,  9,    2,  9,  7,    2,  7,  3,    7,  9,  4,   -1, -1, -1,   -1 },
           {  8,  4,  7,    3, 11,  2,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           { 11,  4,  7,   11,  2,  4,    2,  0,  4,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9,  0,  1,    8,  4,  7,    2,  3, 11,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  4,  7, 11,    9,  4, 11,    9, 11,  2,    9,  2,  1,   -1, -1, -1,   -1 },
           {  3, 10,  1,    3, 11, 10,    7,  8,  4,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1, 11, 10,    1,  4, 11,    1,  0,  4,    7, 11,  4,   -1, -1, -1,   -1 },
           {  4,  7,  8,    9,  0, 11,    9, 11, 10,   11,  0,  3,   -1, -1, -1,   -1 },
           {  4,  7, 11,    4, 11,  9,    9, 11, 10,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9,  5,  4,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9,  5,  4,    0,  8,  3,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  5,  4,    1,  5,  0,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  8,  5,  4,    8,  3,  5,    3,  1,  5,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  2, 10,    9,  5,  4,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  3,  0,  8,    1,  2, 10,    4,  9,  5,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  5,  2, 10,    5,  4,  2,    4,  0,  2,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  2, 10,  5,    3,  2,  5,    3,  5,  4,    3,  4,  8,   -1, -1, -1,   -1 },
           {  9,  5,  4,    2,  3, 11,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0, 11,  2,    0,  8, 11,    4,  9,  5,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  5,  4,    0,  1,  5,    2,  3, 11,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  2,  1,  5,    2,  5,  8,    2,  8, 11,    4,  8,  5,   -1, -1, -1,   -1 },
           { 10,  3, 11,   10,  1,  3,    9,  5,  4,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  4,  9,  5,    0,  8,  1,    8, 10,  1,    8, 11, 10,   -1, -1, -1,   -1 },
           {  5,  4,  0,    5,  0, 11,    5, 11, 10,   11,  0,  3,   -1, -1, -1,   -1 },
           {  5,  4,  8,    5,  8, 10,   10,  8, 11,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9,  7,  8,    5,  7,  9,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9,  3,  0,    9,  5,  3,    5,  7,  3,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  7,  8,    0,  1,  7,    1,  5,  7,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  5,  3,    3,  5,  7,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9,  7,  8,    9,  5,  7,   10,  1,  2,   -1, -1, -1,   -1, -1, -1,   -1 },
           { 10,  1,  2,    9,  5,  0,    5,  3,  0,    5,  7,  3,   -1, -1, -1,   -1 },
           {  8,  0,  2,    8,  2,  5,    8,  5,  7,   10,  5,  2,   -1, -1, -1,   -1 },
           {  2, 10,  5,    2,  5,  3,    3,  5,  7,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  7,  9,  5,    7,  8,  9,    3, 11,  2,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9,  5,  7,    9,  7,  2,    9,  2,  0,    2,  7, 11,   -1, -1, -1,   -1 },
           {  2,  3, 11,    0,  1,  8,    1,  7,  8,    1,  5,  7,   -1, -1, -1,   -1 },
           { 11,  2,  1,   11,  1,  7,    7,  1,  5,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9,  5,  8,    8,  5,  7,   10,  1,  3,   10,  3, 11,   -1, -1, -1,   -1 },
           {  5,  7,  0,    5,  0,  9,    7, 11,  0,    1,  0, 10,   11, 10,  0,   -1 },
           { 11, 10,  0,   11,  0,  3,   10,  5,  0,    8,  0,  7,    5,  7,  0,   -1 },
           { 11, 10,  5,    7, 11,  5,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           { 10,  6,  5,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  8,  3,    5, 10,  6,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9,  0,  1,    5, 10,  6,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  8,  3,    1,  9,  8,    5, 10,  6,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  6,  5,    2,  6,  1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  6,  5,    1,  2,  6,    3,  0,  8,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9,  6,  5,    9,  0,  6,    0,  2,  6,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  5,  9,  8,    5,  8,  2,    5,  2,  6,    3,  2,  8,   -1, -1, -1,   -1 },
           {  2,  3, 11,   10,  6,  5,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           { 11,  0,  8,   11,  2,  0,   10,  6,  5,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  1,  9,    2,  3, 11,    5, 10,  6,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  5, 10,  6,    1,  9,  2,    9, 11,  2,    9,  8, 11,   -1, -1, -1,   -1 },
           {  6,  3, 11,    6,  5,  3,    5,  1,  3,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  8, 11,    0, 11,  5,    0,  5,  1,    5, 11,  6,   -1, -1, -1,   -1 },
           {  3, 11,  6,    0,  3,  6,    0,  6,  5,    0,  5,  9,   -1, -1, -1,   -1 },
           {  6,  5,  9,    6,  9, 11,   11,  9,  8,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  5, 10,  6,    4,  7,  8,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  4,  3,  0,    4,  7,  3,    6,  5, 10,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  9,  0,    5, 10,  6,    8,  4,  7,   -1, -1, -1,   -1, -1, -1,   -1 },
           { 10,  6,  5,    1,  9,  7,    1,  7,  3,    7,  9,  4,   -1, -1, -1,   -1 },
           {  6,  1,  2,    6,  5,  1,    4,  7,  8,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  2,  5,    5,  2,  6,    3,  0,  4,    3,  4,  7,   -1, -1, -1,   -1 },
           {  8,  4,  7,    9,  0,  5,    0,  6,  5,    0,  2,  6,   -1, -1, -1,   -1 },
           {  7,  3,  9,    7,  9,  4,    3,  2,  9,    5,  9,  6,    2,  6,  9,   -1 },
           {  3, 11,  2,    7,  8,  4,   10,  6,  5,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  5, 10,  6,    4,  7,  2,    4,  2,  0,    2,  7, 11,   -1, -1, -1,   -1 },
           {  0,  1,  9,    4,  7,  8,    2,  3, 11,    5, 10,  6,   -1, -1, -1,   -1 },
           {  9,  2,  1,    9, 11,  2,    9,  4, 11,    7, 11,  4,    5, 10,  6,   -1 },
           {  8,  4,  7,    3, 11,  5,    3,  5,  1,    5, 11,  6,   -1, -1, -1,   -1 },
           {  5,  1, 11,    5, 11,  6,    1,  0, 11,    7, 11,  4,    0,  4, 11,   -1 },
           {  0,  5,  9,    0,  6,  5,    0,  3,  6,   11,  6,  3,    8,  4,  7,   -1 },
           {  6,  5,  9,    6,  9, 11,    4,  7,  9,    7, 11,  9,   -1, -1, -1,   -1 },
           { 10,  4,  9,    6,  4, 10,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  4, 10,  6,    4,  9, 10,    0,  8,  3,   -1, -1, -1,   -1, -1, -1,   -1 },
           { 10,  0,  1,   10,  6,  0,    6,  4,  0,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  8,  3,  1,    8,  1,  6,    8,  6,  4,    6,  1, 10,   -1, -1, -1,   -1 },
           {  1,  4,  9,    1,  2,  4,    2,  6,  4,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  3,  0,  8,    1,  2,  9,    2,  4,  9,    2,  6,  4,   -1, -1, -1,   -1 },
           {  0,  2,  4,    4,  2,  6,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  8,  3,  2,    8,  2,  4,    4,  2,  6,   -1, -1, -1,   -1, -1, -1,   -1 },
           { 10,  4,  9,   10,  6,  4,   11,  2,  3,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  8,  2,    2,  8, 11,    4,  9, 10,    4, 10,  6,   -1, -1, -1,   -1 },
           {  3, 11,  2,    0,  1,  6,    0,  6,  4,    6,  1, 10,   -1, -1, -1,   -1 },
           {  6,  4,  1,    6,  1, 10,    4,  8,  1,    2,  1, 11,    8, 11,  1,   -1 },
           {  9,  6,  4,    9,  3,  6,    9,  1,  3,   11,  6,  3,   -1, -1, -1,   -1 },
           {  8, 11,  1,    8,  1,  0,   11,  6,  1,    9,  1,  4,    6,  4,  1,   -1 },
           {  3, 11,  6,    3,  6,  0,    0,  6,  4,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  6,  4,  8,   11,  6,  8,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  7, 10,  6,    7,  8, 10,    8,  9, 10,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  7,  3,    0, 10,  7,    0,  9, 10,    6,  7, 10,   -1, -1, -1,   -1 },
           { 10,  6,  7,    1, 10,  7,    1,  7,  8,    1,  8,  0,   -1, -1, -1,   -1 },
           { 10,  6,  7,   10,  7,  1,    1,  7,  3,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  2,  6,    1,  6,  8,    1,  8,  9,    8,  6,  7,   -1, -1, -1,   -1 },
           {  2,  6,  9,    2,  9,  1,    6,  7,  9,    0,  9,  3,    7,  3,  9,   -1 },
           {  7,  8,  0,    7,  0,  6,    6,  0,  2,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  7,  3,  2,    6,  7,  2,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  2,  3, 11,   10,  6,  8,   10,  8,  9,    8,  6,  7,   -1, -1, -1,   -1 },
           {  2,  0,  7,    2,  7, 11,    0,  9,  7,    6,  7, 10,    9, 10,  7,   -1 },
           {  1,  8,  0,    1,  7,  8,    1, 10,  7,    6,  7, 10,    2,  3, 11,   -1 },
           { 11,  2,  1,   11,  1,  7,   10,  6,  1,    6,  7,  1,   -1, -1, -1,   -1 },
           {  8,  9,  6,    8,  6,  7,    9,  1,  6,   11,  6,  3,    1,  3,  6,   -1 },
           {  0,  9,  1,   11,  6,  7,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  7,  8,  0,    7,  0,  6,    3, 11,  0,   11,  6,  0,   -1, -1, -1,   -1 },
           {  7, 11,  6,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  7,  6, 11,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  3,  0,  8,   11,  7,  6,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  1,  9,   11,  7,  6,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  8,  1,  9,    8,  3,  1,   11,  7,  6,   -1, -1, -1,   -1, -1, -1,   -1 },
           { 10,  1,  2,    6, 11,  7,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  2, 10,    3,  0,  8,    6, 11,  7,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  2,  9,  0,    2, 10,  9,    6, 11,  7,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  6, 11,  7,    2, 10,  3,   10,  8,  3,   10,  9,  8,   -1, -1, -1,   -1 },
           {  7,  2,  3,    6,  2,  7,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  7,  0,  8,    7,  6,  0,    6,  2,  0,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  2,  7,  6,    2,  3,  7,    0,  1,  9,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  6,  2,    1,  8,  6,    1,  9,  8,    8,  7,  6,   -1, -1, -1,   -1 },
           { 10,  7,  6,   10,  1,  7,    1,  3,  7,   -1, -1, -1,   -1, -1, -1,   -1 },
           { 10,  7,  6,    1,  7, 10,    1,  8,  7,    1,  0,  8,   -1, -1, -1,   -1 },
           {  0,  3,  7,    0,  7, 10,    0, 10,  9,    6, 10,  7,   -1, -1, -1,   -1 },
           {  7,  6, 10,    7, 10,  8,    8, 10,  9,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  6,  8,  4,   11,  8,  6,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  3,  6, 11,    3,  0,  6,    0,  4,  6,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  8,  6, 11,    8,  4,  6,    9,  0,  1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9,  4,  6,    9,  6,  3,    9,  3,  1,   11,  3,  6,   -1, -1, -1,   -1 },
           {  6,  8,  4,    6, 11,  8,    2, 10,  1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  2, 10,    3,  0, 11,    0,  6, 11,    0,  4,  6,   -1, -1, -1,   -1 },
           {  4, 11,  8,    4,  6, 11,    0,  2,  9,    2, 10,  9,   -1, -1, -1,   -1 },
           { 10,  9,  3,   10,  3,  2,    9,  4,  3,   11,  3,  6,    4,  6,  3,   -1 },
           {  8,  2,  3,    8,  4,  2,    4,  6,  2,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  4,  2,    4,  6,  2,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  9,  0,    2,  3,  4,    2,  4,  6,    4,  3,  8,   -1, -1, -1,   -1 },
           {  1,  9,  4,    1,  4,  2,    2,  4,  6,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  8,  1,  3,    8,  6,  1,    8,  4,  6,    6, 10,  1,   -1, -1, -1,   -1 },
           { 10,  1,  0,   10,  0,  6,    6,  0,  4,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  4,  6,  3,    4,  3,  8,    6, 10,  3,    0,  3,  9,   10,  9,  3,   -1 },
           { 10,  9,  4,    6, 10,  4,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  4,  9,  5,    7,  6, 11,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  8,  3,    4,  9,  5,   11,  7,  6,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  5,  0,  1,    5,  4,  0,    7,  6, 11,   -1, -1, -1,   -1, -1, -1,   -1 },
           { 11,  7,  6,    8,  3,  4,    3,  5,  4,    3,  1,  5,   -1, -1, -1,   -1 },
           {  9,  5,  4,   10,  1,  2,    7,  6, 11,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  6, 11,  7,    1,  2, 10,    0,  8,  3,    4,  9,  5,   -1, -1, -1,   -1 },
           {  7,  6, 11,    5,  4, 10,    4,  2, 10,    4,  0,  2,   -1, -1, -1,   -1 },
           {  3,  4,  8,    3,  5,  4,    3,  2,  5,   10,  5,  2,   11,  7,  6,   -1 },
           {  7,  2,  3,    7,  6,  2,    5,  4,  9,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9,  5,  4,    0,  8,  6,    0,  6,  2,    6,  8,  7,   -1, -1, -1,   -1 },
           {  3,  6,  2,    3,  7,  6,    1,  5,  0,    5,  4,  0,   -1, -1, -1,   -1 },
           {  6,  2,  8,    6,  8,  7,    2,  1,  8,    4,  8,  5,    1,  5,  8,   -1 },
           {  9,  5,  4,   10,  1,  6,    1,  7,  6,    1,  3,  7,   -1, -1, -1,   -1 },
           {  1,  6, 10,    1,  7,  6,    1,  0,  7,    8,  7,  0,    9,  5,  4,   -1 },
           {  4,  0, 10,    4, 10,  5,    0,  3, 10,    6, 10,  7,    3,  7, 10,   -1 },
           {  7,  6, 10,    7, 10,  8,    5,  4, 10,    4,  8, 10,   -1, -1, -1,   -1 },
           {  6,  9,  5,    6, 11,  9,   11,  8,  9,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  3,  6, 11,    0,  6,  3,    0,  5,  6,    0,  9,  5,   -1, -1, -1,   -1 },
           {  0, 11,  8,    0,  5, 11,    0,  1,  5,    5,  6, 11,   -1, -1, -1,   -1 },
           {  6, 11,  3,    6,  3,  5,    5,  3,  1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  2, 10,    9,  5, 11,    9, 11,  8,   11,  5,  6,   -1, -1, -1,   -1 },
           {  0, 11,  3,    0,  6, 11,    0,  9,  6,    5,  6,  9,    1,  2, 10,   -1 },
           { 11,  8,  5,   11,  5,  6,    8,  0,  5,   10,  5,  2,    0,  2,  5,   -1 },
           {  6, 11,  3,    6,  3,  5,    2, 10,  3,   10,  5,  3,   -1, -1, -1,   -1 },
           {  5,  8,  9,    5,  2,  8,    5,  6,  2,    3,  8,  2,   -1, -1, -1,   -1 },
           {  9,  5,  6,    9,  6,  0,    0,  6,  2,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  5,  8,    1,  8,  0,    5,  6,  8,    3,  8,  2,    6,  2,  8,   -1 },
           {  1,  5,  6,    2,  1,  6,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  3,  6,    1,  6, 10,    3,  8,  6,    5,  6,  9,    8,  9,  6,   -1 },
           { 10,  1,  0,   10,  0,  6,    9,  5,  0,    5,  6,  0,   -1, -1, -1,   -1 },
           {  0,  3,  8,    5,  6, 10,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           { 10,  5,  6,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           { 11,  5, 10,    7,  5, 11,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           { 11,  5, 10,   11,  7,  5,    8,  3,  0,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  5, 11,  7,    5, 10, 11,    1,  9,  0,   -1, -1, -1,   -1, -1, -1,   -1 },
           { 10,  7,  5,   10, 11,  7,    9,  8,  1,    8,  3,  1,   -1, -1, -1,   -1 },
           { 11,  1,  2,   11,  7,  1,    7,  5,  1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  8,  3,    1,  2,  7,    1,  7,  5,    7,  2, 11,   -1, -1, -1,   -1 },
           {  9,  7,  5,    9,  2,  7,    9,  0,  2,    2, 11,  7,   -1, -1, -1,   -1 },
           {  7,  5,  2,    7,  2, 11,    5,  9,  2,    3,  2,  8,    9,  8,  2,   -1 },
           {  2,  5, 10,    2,  3,  5,    3,  7,  5,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  8,  2,  0,    8,  5,  2,    8,  7,  5,   10,  2,  5,   -1, -1, -1,   -1 },
           {  9,  0,  1,    5, 10,  3,    5,  3,  7,    3, 10,  2,   -1, -1, -1,   -1 },
           {  9,  8,  2,    9,  2,  1,    8,  7,  2,   10,  2,  5,    7,  5,  2,   -1 },
           {  1,  3,  5,    3,  7,  5,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  8,  7,    0,  7,  1,    1,  7,  5,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9,  0,  3,    9,  3,  5,    5,  3,  7,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9,  8,  7,    5,  9,  7,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  5,  8,  4,    5, 10,  8,   10, 11,  8,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  5,  0,  4,    5, 11,  0,    5, 10, 11,   11,  3,  0,   -1, -1, -1,   -1 },
           {  0,  1,  9,    8,  4, 10,    8, 10, 11,   10,  4,  5,   -1, -1, -1,   -1 },
           { 10, 11,  4,   10,  4,  5,   11,  3,  4,    9,  4,  1,    3,  1,  4,   -1 },
           {  2,  5,  1,    2,  8,  5,    2, 11,  8,    4,  5,  8,   -1, -1, -1,   -1 },
           {  0,  4, 11,    0, 11,  3,    4,  5, 11,    2, 11,  1,    5,  1, 11,   -1 },
           {  0,  2,  5,    0,  5,  9,    2, 11,  5,    4,  5,  8,   11,  8,  5,   -1 },
           {  9,  4,  5,    2, 11,  3,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  2,  5, 10,    3,  5,  2,    3,  4,  5,    3,  8,  4,   -1, -1, -1,   -1 },
           {  5, 10,  2,    5,  2,  4,    4,  2,  0,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  3, 10,  2,    3,  5, 10,    3,  8,  5,    4,  5,  8,    0,  1,  9,   -1 },
           {  5, 10,  2,    5,  2,  4,    1,  9,  2,    9,  4,  2,   -1, -1, -1,   -1 },
           {  8,  4,  5,    8,  5,  3,    3,  5,  1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  4,  5,    1,  0,  5,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  8,  4,  5,    8,  5,  3,    9,  0,  5,    0,  3,  5,   -1, -1, -1,   -1 },
           {  9,  4,  5,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  4, 11,  7,    4,  9, 11,    9, 10, 11,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  8,  3,    4,  9,  7,    9, 11,  7,    9, 10, 11,   -1, -1, -1,   -1 },
           {  1, 10, 11,    1, 11,  4,    1,  4,  0,    7,  4, 11,   -1, -1, -1,   -1 },
           {  3,  1,  4,    3,  4,  8,    1, 10,  4,    7,  4, 11,   10, 11,  4,   -1 },
           {  4, 11,  7,    9, 11,  4,    9,  2, 11,    9,  1,  2,   -1, -1, -1,   -1 },
           {  9,  7,  4,    9, 11,  7,    9,  1, 11,    2, 11,  1,    0,  8,  3,   -1 },
           { 11,  7,  4,   11,  4,  2,    2,  4,  0,   -1, -1, -1,   -1, -1, -1,   -1 },
           { 11,  7,  4,   11,  4,  2,    8,  3,  4,    3,  2,  4,   -1, -1, -1,   -1 },
           {  2,  9, 10,    2,  7,  9,    2,  3,  7,    7,  4,  9,   -1, -1, -1,   -1 },
           {  9, 10,  7,    9,  7,  4,   10,  2,  7,    8,  7,  0,    2,  0,  7,   -1 },
           {  3,  7, 10,    3, 10,  2,    7,  4, 10,    1, 10,  0,    4,  0, 10,   -1 },
           {  1, 10,  2,    8,  7,  4,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  4,  9,  1,    4,  1,  7,    7,  1,  3,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  4,  9,  1,    4,  1,  7,    0,  8,  1,    8,  7,  1,   -1, -1, -1,   -1 },
           {  4,  0,  3,    7,  4,  3,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  4,  8,  7,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9, 10,  8,   10, 11,  8,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  3,  0,  9,    3,  9, 11,   11,  9, 10,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  1, 10,    0, 10,  8,    8, 10, 11,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  3,  1, 10,   11,  3, 10,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  2, 11,    1, 11,  9,    9, 11,  8,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  3,  0,  9,    3,  9, 11,    1,  2,  9,    2, 11,  9,   -1, -1, -1,   -1 },
           {  0,  2, 11,    8,  0, 11,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  3,  2, 11,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  2,  3,  8,    2,  8, 10,   10,  8,  9,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  9, 10,  2,    0,  9,  2,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  2,  3,  8,    2,  8, 10,    0,  1,  8,    1, 10,  8,   -1, -1, -1,   -1 },
           {  1, 10,  2,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  1,  3,  8,    9,  1,  8,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  9,  1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           {  0,  3,  8,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 },
           { -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1, -1, -1,   -1 }
    } };

    // Convert an edge index to the corner vertex indices for a cube.
    const std::array< std::array<int32_t, 2>, 12> a2iEdgeConnection { {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},  // Bottom face.
        {4, 5}, {5, 6}, {6, 7}, {7, 4},  // Top face.
        {0, 4}, {1, 5}, {2, 6}, {3, 7}   // Side faces.
    } };

    // Storage for partially-connected meshes within the plane of a single image.
    // Data is processed one image at a time and we only merge meshes and de-duplicate out-of-plane vertices afterward.
    struct per_img_fv_mesh_t {
        std::vector<vec3<double>> verts;
        std::vector<std::vector<uint64_t>> faces;

        std::vector<std::vector<uint64_t>> fsrel; // which img num vert num is relative to.
        std::vector<uint64_t> vscor; // lower bound index of the local verts that correspond to a given voxel.
    };
    std::map<int64_t, per_img_fv_mesh_t> per_img_fv_mesh;

    // Prime the map.
    const auto [img_num_min, img_num_max] = img_adj.get_min_max_indices();
    for(int64_t i = img_num_min; i <= img_num_max; ++i){
        per_img_fv_mesh[i - img_num_min].vscor.emplace_back(0);
    }
    const auto vscor_index = [](int64_t N_rows, int64_t N_cols, 
                                int64_t drow, int64_t dcol) -> int64_t {
        return N_cols * drow + dcol; // 2D index.
    };

    std::mutex saver_printer; // Thread synchro lock for saving shared data, logging, and counter iterating.
    int64_t completed = 0;
    const int64_t img_count = grid_imgs.size();
    auto final_merge_tol = std::numeric_limits<double>::infinity();

    // Iterate over all voxels, traversing the images in order of adjacency for consistency.
    //
    // NOTE: The order of traversal must reflect image adjacency. This requirement could be relaxed if candidate
    //       vertices (below) were indexed, but even this would involve extra memory usage for little gain.
    const auto work = [&,
                       img_num_min = img_num_min](planar_image_adjacency<float,double>::img_refw_t img_refw) -> void {
        const auto pxl_dx = img_refw.get().pxl_dx;
        const auto pxl_dy = img_refw.get().pxl_dy;
        const auto pxl_dz = img_refw.get().pxl_dz;

        const auto row_unit = img_refw.get().row_unit.unit();
        const auto col_unit = img_refw.get().col_unit.unit();
        const auto img_unit = row_unit.Cross(col_unit).unit();

        // Tolerance for deciding if meshed vertices are identical. If too lax, then topological inconsistencies may
        // result; if too tight, then the mesh will either be non-watertight, non-manifold, or will be constructed in an
        // invalid way by the mesher.
        //
        // The following tolerance was estimated with a Gaussian-smoothed spherical phantom.
        // If these tolerances are increased, meshing will 'wobble' around the boundary and possibly create zero-area
        // needles and surface crossovers. However, merely setting the tolerance to zero will likely cause meshing to
        // fail outright.
        constexpr auto machine_eps = std::numeric_limits<double>::epsilon();
        const auto dvec3_tol = std::max<double>(
                                   std::min<double>( { pxl_dx, pxl_dy, pxl_dz } ) * 1E-4,
                                   std::sqrt(machine_eps) * 100.0 ); // Guard against pxl_dz = 0.
        final_merge_tol = std::min<double>(final_merge_tol, dvec3_tol);

        // List of Marching Cube voxel corner positions relative to image voxel centre.
        //
        // Note that the Marching cube and image voxels are not the same. They are offset such that
        // the corner of the Marching Cube voxel is at the centre of the image voxel. This is done
        // to avoid surface discontinuties that would arise from sampling the boundary of border
        // voxels; numerical instability could potentially lead to random fluctuation by 1 voxel width on
        // straight borders.
        const std::array< vec3<double>, 8> a2fVertexOffset { {
            (zero3),
            (zero3 + row_unit * pxl_dx),
            (zero3 + row_unit * pxl_dx + col_unit * pxl_dy),
            (zero3 + col_unit * pxl_dy),

            (zero3 + img_unit * pxl_dz),
            (zero3 + row_unit * pxl_dx + img_unit * pxl_dz),
            (zero3 + row_unit * pxl_dx + col_unit * pxl_dy + img_unit * pxl_dz),
            (zero3 + col_unit * pxl_dy + img_unit * pxl_dz)
        } };

        // The vector needed to translate from tail to head for each edge.
        const std::array< vec3<double>, 12> a2fEdgeDirection { {
            row_unit * pxl_dx, // Bottom face.
            col_unit * pxl_dy,
            row_unit * -pxl_dx,
            col_unit * -pxl_dy,

            row_unit * pxl_dx, // Top face.
            col_unit * pxl_dy,
            row_unit * -pxl_dx,
            col_unit * -pxl_dy,

            img_unit * pxl_dz, // Side faces.
            img_unit * pxl_dz,
            img_unit * pxl_dz,
            img_unit * pxl_dz
        } };

        const auto img_num = img_adj.image_to_index( img_refw );
        const auto img_num_p1 = img_num + 1;
        const auto img_is_adj = img_adj.index_present(img_num_p1);
        const auto img_p1 = (img_is_adj) ? img_adj.index_to_image(img_num_p1) : img_refw;

        const auto shifted_img_num = static_cast<int64_t>(img_num - img_num_min); // Used for per-img mesh lookups.

        per_img_fv_mesh_t* m_mini_mesh_ptr = &(per_img_fv_mesh[shifted_img_num]); // 'Middle' (current) image plane.
        per_img_fv_mesh_t* l_mini_mesh_ptr = nullptr; // 'Lower' adjacent image plane.
        per_img_fv_mesh_t* u_mini_mesh_ptr = nullptr; // 'Upper' adjacent image plane.
        if(per_img_fv_mesh.count(shifted_img_num - 1L) != 0){
            l_mini_mesh_ptr = &(per_img_fv_mesh[shifted_img_num - 1L]);
        }
        if(per_img_fv_mesh.count(shifted_img_num + 1L) != 0){
            u_mini_mesh_ptr = &(per_img_fv_mesh[shifted_img_num + 1L]);
        }

        const auto N_rows = img_refw.get().rows;
        const auto N_cols = img_refw.get().columns;

        // I think planar adjacency enforces this already, but I'd like to be explicit about it.
        // This implementation is complicated enough already.
        if( (N_rows != img_p1.get().rows)
        ||  (N_cols != img_p1.get().columns) ){
            throw std::invalid_argument("Regular grids are required for this algorithm -- images must all have the same number of rows and columns");
        }

        // Generate a generic list of vertex index offsets to check for vertex deduplication.
        // This list will be offset by the current voxel coordinates and unreachable neighbours will be pruned.
        std::set<int64_t> vscor_to_check;
        {
            vscor_to_check.insert(vscor_index(N_rows, N_cols, -1, -1));
            vscor_to_check.insert(vscor_index(N_rows, N_cols, -1,  0));
            vscor_to_check.insert(vscor_index(N_rows, N_cols, -1,  1));
            vscor_to_check.insert(vscor_index(N_rows, N_cols,  0, -1));
            vscor_to_check.insert(vscor_index(N_rows, N_cols,  0,  0));
            vscor_to_check.insert(vscor_index(N_rows, N_cols,  0,  1));
            vscor_to_check.insert(vscor_index(N_rows, N_cols,  1, -1));
            vscor_to_check.insert(vscor_index(N_rows, N_cols,  1,  0));
            vscor_to_check.insert(vscor_index(N_rows, N_cols,  1,  1));
        }

        // Tailor the generic list of vertex index offset to check to the current position, discarding irrelevant items.
        const auto customize_vscor_to_check = [&vscor_index]( int64_t row, int64_t col,
                                                  int64_t N_rows, int64_t N_cols,
                                                  const std::set<int64_t> &vscor_to_check,
                                                  const per_img_fv_mesh_t* per_img_fv_mesh_ptr ){
            std::set<size_t> verts_to_check;
            if(per_img_fv_mesh_ptr != nullptr){
                // Now for each i in vscor_to_check, insert per_img_fv_mesh_ptr->vscor[i] ... per_img_fv_mesh_ptr->vscor[i+1] into verts_to_check (iff it currently exists!)
                const auto N_vscor = static_cast<int64_t>(per_img_fv_mesh_ptr->vscor.size());
                for(const auto &i : vscor_to_check){
                    const auto i_offset = vscor_index(N_rows, N_cols, row, col);
                    const auto i_total = i_offset + i;
                    if( (-1L < i_total) && ((i_total + 1L) < N_vscor) ){
                        const auto j_min = per_img_fv_mesh_ptr->vscor[i_total];
                        const auto j_max = per_img_fv_mesh_ptr->vscor[i_total + 1L];
                        for(auto j = j_min; j < j_max; ++j){
                            verts_to_check.insert(j);
                        }
                    }
                }
            }
            return verts_to_check;
        };

        for(int64_t row = 0; row < N_rows; ++row){
            for(int64_t col = 0; col < N_cols; ++col){
                const auto pos = img_refw.get().position(row, col);

                // Sample voxel corner values.
                std::array<double, 8> afCubeValue;
                
                // Use the voxel intensity as a level-set or 'oracle' function to define the mesh boundary.
                if(!has_signed_dist_func){
                    //// Option A: Interpolate in 3D using trilinear interpolation.
                    ////
                    //// This approach is slow but extremely flexible, e.g, does not require image rectilinearity.
                    //for(int32_t corner = 0; corner < 8; ++corner){
                    //    afCubeValue[corner] = interpolate_3D(pos + a2fVertexOffset[corner]);
                    //}
                    
                    // Option B: Align Marching Cube voxel corners with image voxel centres.
                    //
                    // This approach is fast, but requires image rectilinearity.
                    const auto row_p1 = (row+1);
                    const auto row_is_adj = (row_p1 < N_rows);

                    const auto col_p1 = (col+1);
                    const auto col_is_adj = (col_p1 < N_cols);

                    afCubeValue[0] = img_refw.get().value(row, col, 0);
                    afCubeValue[1] = (row_is_adj)                             ? img_refw.get().value(row_p1, col, 0)    : ExteriorVal;
                    afCubeValue[2] = (row_is_adj && col_is_adj)               ? img_refw.get().value(row_p1, col_p1, 0) : ExteriorVal;
                    afCubeValue[3] = (col_is_adj)                             ? img_refw.get().value(row, col_p1, 0)    : ExteriorVal;
                    afCubeValue[4] = (img_is_adj)                             ? img_p1.get().value(row, col, 0)         : ExteriorVal;
                    afCubeValue[5] = (row_is_adj && img_is_adj)               ? img_p1.get().value(row_p1, col, 0)      : ExteriorVal;
                    afCubeValue[6] = (row_is_adj && col_is_adj && img_is_adj) ? img_p1.get().value(row_p1, col_p1, 0)   : ExteriorVal;
                    afCubeValue[7] = (col_is_adj && img_is_adj)               ? img_p1.get().value(row, col_p1, 0)      : ExteriorVal;

                // Use the provided signed distance function to 'override' the image voxel intensities.
                //
                // This approach is slow but extremely flexible for meshing complicated shapes (e.g., Booleans).
                }else{
                    for(int32_t corner = 0; corner < 8; ++corner){
                        afCubeValue[corner] = sdf->evaluate_sdf(pos + a2fVertexOffset[corner]);
                    }
                }
                
                // Convert vertex inclusion to a bitmask.
                int32_t iFlagIndex = 0;
                for(int32_t corner = 0; corner < 8; ++corner){
                    if(below_is_interior){
                        if(afCubeValue[corner] <= inclusion_threshold) iFlagIndex |= (1 << corner);
                    }else{
                        if(afCubeValue[corner] >= inclusion_threshold) iFlagIndex |= (1 << corner);
                    }
                }

                // Convert vertex inclusion into a list of 'involved' edges that cross the ROI surface.
                const int32_t iEdgeFlags = aiCubeEdgeFlags[iFlagIndex];

                // If the cube is entirely inside or outside of the surface, then there will be no intersections.
                if(iEdgeFlags == 0){
                    m_mini_mesh_ptr->vscor.emplace_back(m_mini_mesh_ptr->verts.size());
                    continue;
                }

                // Find the point of intersection of the surface with each edge.
                std::array<vec3<double>, 12> asEdgeVertex;
                for(int32_t edge = 0; edge < 12; edge++){
                    if(iEdgeFlags & (1 << edge)){ // continue iff involved.

                        const double value_A = afCubeValue[a2iEdgeConnection[edge][0]];
                        const double value_B = afCubeValue[a2iEdgeConnection[edge][1]];

                        // Find the (approximate) point along the edge where the surface intersects, parameterized to [0:1].
                        const double d_value = (value_B - value_A);
                        const double inv_d_value = static_cast<double>(1.0)/d_value;
                        const double lin_interp = (inclusion_threshold - value_A) * inv_d_value;
                        const double surf_dl = std::isfinite(lin_interp) ? lin_interp : static_cast<double>(0.5);
                        if(!isininc(0.0,surf_dl,1.0)){
                            throw std::logic_error("Interpolation of surface-edge intersection failed. Refusing to continue");
                        }

                        asEdgeVertex[edge] = (a2fEdgeDirection[edge] * surf_dl);
                        asEdgeVertex[edge] += a2fVertexOffset[a2iEdgeConnection[edge][0]];
                        asEdgeVertex[edge] += pos;
                    }
                }

                // Tailor the generic list of indices to check to the current position, discarding irrelevant items.
                const auto l_verts_to_check = customize_vscor_to_check(row, col, N_rows, N_cols, vscor_to_check, l_mini_mesh_ptr);
                auto m_verts_to_check = customize_vscor_to_check(row, col, N_rows, N_cols, vscor_to_check, m_mini_mesh_ptr);
                const auto u_verts_to_check = customize_vscor_to_check(row, col, N_rows, N_cols, vscor_to_check, u_mini_mesh_ptr);

                // Process the triangles that were identified.
                for(int32_t tri = 0; tri < 5; tri++){

                    // Stop when the first -1 index is encountered (signifying there are no further triangles).
                    if(a2iTriangleConnectionTable[iFlagIndex][3*tri] < 0) break;

                    std::array<vec3<double>, 3> tri_verts;
                    for(int32_t tri_corner = 0; tri_corner < 3; ++tri_corner){
                        const int32_t vert_idx = a2iTriangleConnectionTable[iFlagIndex][3*tri + tri_corner];
                        tri_verts[tri_corner] = asEdgeVertex[vert_idx];
    
                    }

                    // If any two vertices are indistinct from one another, remove the face. It is degenerate
                    // and should not create holes.
                    //
                    // Note that this can potentially cause issues if vertex spacing is on the order of the tolerance
                    // distance.
                    if( (tri_verts[0].sq_dist(tri_verts[1]) < (dvec3_tol*dvec3_tol))
                    ||  (tri_verts[0].sq_dist(tri_verts[2]) < (dvec3_tol*dvec3_tol))
                    ||  (tri_verts[1].sq_dist(tri_verts[2]) < (dvec3_tol*dvec3_tol)) ){
                        // Do nothing.

                    // Otherwise, add vertices and faces to the mesh.
                    //
                    // Most vertices will be duplicated FOUR times. Unfortunately, I can't think of a reasonable way to
                    // de-duplicate inline, even though the connectivity info is all available, because it's faster to
                    // extract everything in parallel and then de-duplicate afterward.
                    }else{
                        std::vector<vec3<double>> new_verts;
                        std::vector<uint64_t> new_indices;
                        std::vector<uint64_t> new_fsrel;

                        for(int32_t tri_corner = 0; tri_corner < 3; ++tri_corner){
                            // Option 1: *always* create new vertices.
                            //
                            // This is extremely fast, but consumes lots of extra memory and makes it hard to do
                            // anything with the resulting mesh. (The mesh is in a form called 'polygon soup.')
                            if(false){
                                new_verts.emplace_back( tri_verts[tri_corner] );
                                new_indices.emplace_back(m_mini_mesh_ptr->verts.size() + new_verts.size() - 1);
                                new_fsrel.emplace_back(shifted_img_num);
                            }


                            // Option 2: deduplicate vertices as soon as we can.
                            //
                            // This is slower, and much harder to parallelize, but avoids 'polygon soup' headaches
                            // later.
                            //
                            // In-plane vertices require deduplication checks for every new vertex, since there are many
                            // marching cube configurations with shared in-plane vertices.
                            //
                            // Out-of-plane vertices are more complicated. By controlling the order of image processing,
                            // we can guarantee that adjacent image planes are never processed at the same time.
                            // Therefore, we can assume that either (1) the adjacent image has not yet been processed,
                            // or (2) it has already been processed. In case (1), we can freely create vertices without
                            // fear that they will be duplicates. In case (2), we have to check for duplicates before
                            // creating any new vertices.

                            bool located = false;

                            // In-plane checks.
                            for(const auto &v_i : m_verts_to_check){
                                const bool duplicate = tri_verts[tri_corner].sq_dist( m_mini_mesh_ptr->verts.at(v_i) ) < (dvec3_tol*dvec3_tol);
                                if(duplicate){
                                    new_indices.emplace_back(v_i);
                                    new_fsrel.emplace_back(shifted_img_num);
                                    located = true;
                                    break;
                                }
                            }

                            // Out-of-plane checks.
                            if(!located && (l_mini_mesh_ptr != nullptr)){
                                for(const auto &v_i : l_verts_to_check){
                                    const bool duplicate = tri_verts[tri_corner].sq_dist( l_mini_mesh_ptr->verts.at(v_i) ) < (dvec3_tol*dvec3_tol);
                                    if(duplicate){
                                        new_indices.emplace_back(v_i);
                                        new_fsrel.emplace_back(shifted_img_num - 1);
                                        located = true;
                                        break;
                                    }
                                }
                            }
                            if(!located && (u_mini_mesh_ptr != nullptr)){
                                for(const auto &v_i : u_verts_to_check){
                                    const bool duplicate = tri_verts[tri_corner].sq_dist( u_mini_mesh_ptr->verts.at(v_i) ) < (dvec3_tol*dvec3_tol);
                                    if(duplicate){
                                        new_indices.emplace_back(v_i);
                                        new_fsrel.emplace_back(shifted_img_num + 1);
                                        located = true;
                                        break;
                                    }
                                }
                            }

                            // Otherwise create a new vertex.
                            if(!located){
                                const auto N_verts_prev = m_mini_mesh_ptr->verts.size();

                                m_verts_to_check.insert(N_verts_prev);
                                new_indices.emplace_back(N_verts_prev);
                                m_mini_mesh_ptr->verts.emplace_back( tri_verts[tri_corner] );
                                new_fsrel.emplace_back(shifted_img_num);
                            }
                        }

                        m_mini_mesh_ptr->fsrel.emplace_back(new_fsrel);
                        m_mini_mesh_ptr->faces.emplace_back(new_indices);
                    }
                } // Loop over triangles.

                m_mini_mesh_ptr->vscor.emplace_back(m_mini_mesh_ptr->verts.size());
            } // Loop over columns.
        } // Loop over rows.

        //Report operation progress.
        {
            std::lock_guard<std::mutex> lock(saver_printer);
            ++completed;
            YLOGINFO("Completed " << completed << " of " << img_count
                  << " --> " << static_cast<int>(1000.0*(completed)/img_count)/10.0 << "% done");
        }
        return;
    };

    // NOTE: if lower memory use is needed, we can further break down the traversal order and eagerly purge sidecar
    // information (e.g., vscor for completely deduplicated submeshes).
    YLOGINFO("Extracting odd-numbered image meshes");
    {
        work_queue<std::function<void(void)>> wq;
        for(int64_t i = img_num_min; i <= img_num_max; ++i){
            if( i % 2 == 0 ) continue;
            wq.submit_task( std::bind(work, img_adj.index_to_image(i)) );
        }
    }

    YLOGINFO("Extracting even-numbered image meshes");
    {
        work_queue<std::function<void(void)>> wq;
        for(int64_t i = img_num_min; i <= img_num_max; ++i){
            if( i % 2 == 1 ) continue;
            wq.submit_task( std::bind(work, img_adj.index_to_image(i)) );
        }
    }

    YLOGINFO("Joining mesh partitions..");
    // Count the (non-self-inclusive) running total number of vertices contained within each sub-mesh.
    std::vector<int64_t> verts_offset;
    verts_offset.push_back( 0 );
    for(const auto& [img_num, l_mini_mesh] : per_img_fv_mesh){
        const auto l_N_verts = l_mini_mesh.verts.size();
        const auto prev_total = (verts_offset.empty()) ? static_cast<size_t>(0) : verts_offset.back();
        verts_offset.push_back( prev_total + l_N_verts );
    }

    // Adjust indices to account for vertices being merged together sequentially.
// NOTE: this loop can be parallelized too!
    for(auto& [img_num, l_mini_mesh] : per_img_fv_mesh){
        const auto l_N_faces = l_mini_mesh.faces.size();
        for(size_t i = 0; i < l_N_faces; ++i){
            const auto l_N_vert_indices = l_mini_mesh.faces[i].size();
            for(size_t j = 0; j < l_N_vert_indices; ++j){
                const auto l_img_num = l_mini_mesh.fsrel[i][j];
                l_mini_mesh.faces[i][j] += verts_offset[ l_img_num ];
            }
        }
    }

    // Insert the vertices and adjusted indices.
    fv_surface_mesh<double, uint64_t> fv_mesh;
    for(auto& [img_num, l_mini_mesh] : per_img_fv_mesh){
        fv_mesh.vertices.insert( std::end(fv_mesh.vertices),
                                 std::begin(l_mini_mesh.verts), std::end(l_mini_mesh.verts) );

        fv_mesh.faces.insert( std::end(fv_mesh.faces),
                              std::begin(l_mini_mesh.faces), std::end(l_mini_mesh.faces) );
    }

//    YLOGINFO("Deduplicating vertices..");
//    fv_mesh.merge_duplicate_vertices(final_merge_tol);

    YLOGINFO("Orienting face normals..");
    // Note that consistent reorientation can legitimately fail for non-orientable objects (e.g., Klein bottles).
    // Note that non-manifold meshes are difficult to orient consistently since adjacency information can be
    // incomplete.
    const auto reorient_faces = [](fv_surface_mesh<double, uint64_t> &fv_mesh) -> bool {
        uint64_t connected_component = 0;
        if(!fv_mesh.faces.empty()){

            // Make all faces triangles.
            fv_mesh.convert_to_triangles();

            // Regenerate involved faces index.
            fv_mesh.recreate_involved_face_index();
            const auto N_faces = fv_mesh.faces.size();

            // Vector to track which faces belong to which connected components.
            std::vector<uint64_t> face_component(N_faces, 0UL);

            // Vector used to keep track of each connected component's signed volume.
            std::vector<Stats::Running_Sum<double>> component_signed_volume;

            // Create state to track which faces have been re-oriented.
            enum class orientation_t : uint8_t {
                unknown,
                reoriented,
            };
            std::vector<orientation_t> reoriented_faces( N_faces, orientation_t::unknown );
            const auto rf_beg = std::begin(reoriented_faces);
            const auto rf_end = std::end(reoriented_faces);

            // Prime the search and begin processing faces.
            //
            // Each individual mesh will be processed one-at-a-time, since we won't be able to traverse disconnected
            // meshes by moving along face adjacencies.
            auto p_it = std::find(rf_beg, rf_end, orientation_t::unknown );
            while(p_it != rf_end){
                component_signed_volume.emplace_back(); // Prime the signed volume for this component.
                component_signed_volume.back().Digest(0.0);

                const auto starting_face = static_cast<uint64_t>( std::distance( rf_beg, p_it ) );
                reoriented_faces[starting_face] = orientation_t::reoriented;

                // Create a set of half-edges that we expect adjacent faces to contain.
                std::set<std::pair<uint64_t,uint64_t>> expected_half_edges;

                // Prime it with the half-edges from the starting face.
                uint64_t A = fv_mesh.faces[starting_face][0];
                uint64_t B = fv_mesh.faces[starting_face][1];
                uint64_t C = fv_mesh.faces[starting_face][2];
                expected_half_edges.insert({ B, A });
                expected_half_edges.insert({ C, B });
                expected_half_edges.insert({ A, C });

                // Create a face queue, which contains all the adjacent faces we need to visit.
                std::set<uint64_t> adj_faces;

                // Prime the list with all nearby faces we still need to visit.
    {
        const uint64_t i = starting_face;
        std::map<uint64_t, uint16_t> l_adj_faces;
        for(const auto &j : fv_mesh.faces.at(i)){ // j refers to a vertex.
            for(const auto &k : fv_mesh.involved_faces.at(j)){ // lookup the involved faces for vert j.
                if(k != i) l_adj_faces[k] += 1U;
            }
        }
        //std::set<uint64_t> result;
        for(const auto &p : l_adj_faces){
            const auto j = p.first;
            if( (1U < p.second)
            &&  (reoriented_faces.at(j) == orientation_t::unknown) ){
                //result.insert(j);
                adj_faces.insert(j);
            }
        }
    }

                // While there are half-edges still in the queue.
                while(!adj_faces.empty()){
                    const auto i = *std::begin(adj_faces);
                    A = fv_mesh.faces[i][0];
                    B = fv_mesh.faces[i][1];
                    C = fv_mesh.faces[i][2];

                    // Check if at least one half-edge is expected.
                    // If any are found, this face already has the correct orientation.
                    // If none are found, reorient the face.
                    // Note that there might be clashes due to non-orientability, but we ignore them for now (TODO).
                    auto AB_found = (expected_half_edges.count({ A, B }) != 0);
                    auto BC_found = (expected_half_edges.count({ B, C }) != 0);
                    auto CA_found = (expected_half_edges.count({ C, A }) != 0);

                    if( !AB_found
                    &&  !BC_found
                    &&  !CA_found ){
                        std::swap( fv_mesh.faces[i][0], fv_mesh.faces[i][1] );
                        A = fv_mesh.faces[i][0];
                        B = fv_mesh.faces[i][1];
                        C = fv_mesh.faces[i][2];
                        AB_found = (expected_half_edges.count({ A, B }) != 0);
                        BC_found = (expected_half_edges.count({ B, C }) != 0);
                        CA_found = (expected_half_edges.count({ C, A }) != 0);
                    }

                    // The face is now considered to be consistently oriented.
                    // (This is done, even if there are topological issues, to ensure we don't get stuck on a face.)
                    reoriented_faces[i] = orientation_t::reoriented;
                    face_component[i] = connected_component;

                    // Add the current face's contribution to the signed volume.
                    //
                    // Note: using suggestion to only use a single linear dimension from
                    // https://math.stackexchange.com/questions/689418/how-to-compute-surface-normal-pointing-out-of-the-object
                    // I'm not certain if this is valid for meshes with holes...
                    try{
                        const vec3<double> pos = (fv_mesh.vertices[A] + fv_mesh.vertices[B] + fv_mesh.vertices[C]) / 3.0;
                        const vec3<double> cross = (fv_mesh.vertices[B] - fv_mesh.vertices[A]).Cross(fv_mesh.vertices[C] - fv_mesh.vertices[A]);
                        const vec3<double> norm = cross.unit();
                        const double area = cross.length() * 0.5;
                        component_signed_volume.back().Digest(norm.x * pos.x * area);
                    }catch(const std::exception &){}; // In case the face is denegerate.

                    // Note that it's possible the face will not be able to find relevant half-edges.
                    // This can happen during marching cubes if an isolated voxel in the corner of an image volume is
                    // high and surrounded by all low voxels -- the face won't have any neighbours at all, so no
                    // matching half-edges will be found.
                    //
                    // Since a solitary face cannot be oriented, put it back the way it was and move on.
                    if( !AB_found
                    &&  !BC_found
                    &&  !CA_found ){
                        std::swap( fv_mesh.faces[i][0], fv_mesh.faces[i][1] );
                        adj_faces.erase(i);
                        continue;
                    }


                    // Add neighbouring faces, but only those that share at least one edge.
                    // Faces of interest will appear at least twice when enumerating the adjacent faces for all vertices.
                    {
                        std::map<uint64_t, uint16_t> l_adj_faces;
                        for(const auto &j : fv_mesh.faces.at(i)){ // j refers to a vertex.
                            for(const auto &k : fv_mesh.involved_faces.at(j)){ // lookup the involved faces for vert j.
                                if(k != i) l_adj_faces[k] += 1U;
                            }
                        }
                        //std::set<uint64_t> result;
                        for(const auto &p : l_adj_faces){
                            const auto j = p.first;
                            if( (1U < p.second)
                            &&  (reoriented_faces.at(j) == orientation_t::unknown) ){
                                //result.insert(j);
                                adj_faces.insert(j);
                            }
                        }
                    }

                    // Remove this face from the queue.
                    adj_faces.erase(i);

                    // Add half-edges relevant for adjacent faces.
                    if(!AB_found) expected_half_edges.insert({ B, A });
                    if(!BC_found) expected_half_edges.insert({ C, B });
                    if(!CA_found) expected_half_edges.insert({ A, C });

                    // Remove the half-edges that we matched.
                    // We assume no other faces will match (assuming manifold mesh).
                    if(AB_found) expected_half_edges.erase({ A, B });
                    if(BC_found) expected_half_edges.erase({ B, C });
                    if(CA_found) expected_half_edges.erase({ C, A });
                }

                // If the queue is empty, but there are still faces that need to be re-oriented, then there are multiple
                // disconnected meshes. Start again using one of the faces as a new prototype.
                ++connected_component;

                // Search for the next unknown face.
                p_it = std::find(p_it, rf_end, orientation_t::unknown );
            }

            // Normals should (barring non-orientability) be consistent. But we still need to decide if they correctly
            // point outward. We can use signed volume to evaluate this.
            for(size_t i = 0; i < N_faces; ++i){
                const auto pos_orien = (0.0 <= component_signed_volume[face_component[i]].Current_Sum());
                if(!pos_orien) std::swap( fv_mesh.faces[i][0], fv_mesh.faces[i][1] );
            }
        }
        YLOGINFO("Finished re-orienting mesh with " << connected_component << " connected components");
        return true;
    };
    if(!reorient_faces(fv_mesh)){
        YLOGWARN("Unable to consistently re-orient mesh. This should never happen after marching cubes");
    }

    YLOGINFO("Removing disconnected vertices..");
    fv_mesh.recreate_involved_face_index();
    fv_mesh.remove_disconnected_vertices();
    fv_mesh.involved_faces.clear();

    YLOGINFO("The triangulated surface has " << fv_mesh.vertices.size() << " vertices"
             " and " << fv_mesh.faces.size() << " faces");
  
    return fv_mesh;
}



// This sub-routine performs the Marching Cubes algorithm for the given ROI contours.
// ROI inclusivity is separately pre-computed before surface probing by generating an inclusivity mask on a
// custom-fitted planar image collection. This is done for performance purposes and so inclusivity and surface
// meshing can be separately tweaked as necessary.
//
// NOTE: This routine does not require the images that the contours were originally generated on.
//       A custom set of dummy images that contiguously cover all ROIs are generated and used internally.
//
// NOTE: This routine assumes all ROIs are co-planar.
//
// NOTE: This routine will handle ROIs with several disconnected components (e.g., "eyes"). But all components
//       will be lumped together into a single polyhedron.
//
fv_surface_mesh<double, uint64_t>
Estimate_Surface_Mesh_Marching_Cubes(
        const std::list<std::reference_wrapper<contour_collection<double>>>& cc_ROIs,
        Parameters params ){

    // Define the convention we will use in our mask.
    const double inclusion_threshold = 0.0;
    const bool below_is_interior = true;  // Anything <= is considered to be interior to the ROI.

    const double InteriorVal = inclusion_threshold - (below_is_interior ? 1.0 : -1.0);
    const double ExteriorVal = inclusion_threshold + (below_is_interior ? 1.0 : -1.0);

    // Figure out plane alignment and work out spacing.
    const auto est_cont_normal = Average_Contour_Normals(cc_ROIs);
    const auto unique_planar_separation_threshold = 0.005; // Contours separated by less are considered to be on the same plane.
    const auto ucp = Unique_Contour_Planes(cc_ROIs, est_cont_normal, unique_planar_separation_threshold);

    // ============================================= Export the data ================================================
    // Export the contour vertices for inspection or processing with other tools.
    /*
    {
        std::vector<Point_3> verts;
        std::ofstream out("/tmp/SurfaceMesh_ROI_vertices.xyz");

        for(auto &cc_ref : cc_ROIs){
            for(const auto &c : cc_ref.get().contours){
                for(const auto &p : c.points){
                    Point_3 cgal_p( p.x, p.y, p.z );
                    verts.push_back(cgal_p);
                }
            }
        }
        if(!out || !CGAL::write_xyz_points(out, verts)){
            throw std::runtime_error("Unable to write contour vertices.");
        } 
    }
    */


    // ============================================== Generate a grid  ==============================================

    // Compute the number of images to make into the grid: number of unique contour planes + 2.
    // The extra two help provide slices above and below the contour extent to ensure polyhedron will be closed.
    if(params.NumberOfImages <= 0) params.NumberOfImages = (ucp.size() + 2);
    YLOGINFO("Number of images: " << params.NumberOfImages);

    // Find grid alignment vectors.
    const auto pi = std::acos(-1.0);
    const auto GridZ = est_cont_normal.unit();
    vec3<double> GridX = GridZ.rotate_around_z(pi * 0.5); // Try Z. Will often be idempotent.
    if(GridX.Dot(GridZ) > 0.25){
        GridX = GridZ.rotate_around_y(pi * 0.5);  //Should always work since GridZ is parallel to Z.
    }
    vec3<double> GridY = GridZ.Cross(GridX);
    if(!GridZ.GramSchmidt_orthogonalize(GridX, GridY)){
        throw std::runtime_error("Unable to find grid orientation vectors.");
    }
    GridX = GridX.unit();
    GridY = GridY.unit();

    // Figure out what z-margin is needed so the extra two images do not interfere with the grid lining up with the
    // contours. (Want exactly one contour plane per image.) So the margin should be large enough so the empty
    // images have no contours inside them, but small enough so that it doesn't affect the location of contours in the
    // other image slices. The ideal is if each image slice has the same thickness so contours are all separated by some
    // constant separation -- in this case we make the margin exactly as big as if two images were also included.
    double z_margin = 0.0;
    if(ucp.size() > 1){
        // Determine the minimum contour plane spacing.
        //
        // This approach is not robust, breaking down when a single contour plane is translated or out of place.
        //const auto sep_per_plane = Estimate_Contour_Separation(cc_ROIs, est_cont_normal, unique_planar_separation_threshold);

        // Alternative computation of separations using medians. It is more robust, but the whole procedure falls apart
        // if the slices are not regularly separated. Leaving here to potentially incorporate into Ygor or elsewhere at
        // a later date.
        std::vector<double> seps;
        for(auto itA = std::begin(ucp); ; ++itA){
            auto itB = std::next(itA);
            if(itB == std::end(ucp)) break;
            seps.emplace_back( std::abs(itA->Get_Signed_Distance_To_Point(itB->R_0)) );
        }
        const auto sep_min = Stats::Min(seps);
        const auto sep_med = Stats::Median(seps);
        const auto sep_max = Stats::Max(seps);
        const auto sep_per_plane = sep_med;

        if(RELATIVE_DIFF(sep_min, sep_max) > 0.01){
            YLOGINFO("Planar separation: min, median, max = " << sep_min << ", " << sep_med << ", " << sep_max);
            YLOGWARN("Planar separations are not consistent."
                     " This could result in topological invalidity (e.g., disconnected or open meshes)."
                     " Assuming the median separation for all contours.");
        }

        // Add TOTAL zmargin of 1*sep_per_plane each for each extra images, and 0.5*sep_per_plane for each of the images
        // which will stick out beyond the contour planes. However, the margin is added to both the top and the bottom so
        // halve the total amount.
        z_margin = sep_per_plane * 1.5;

    }else{
        YLOGWARN("Only a single contour plane was detected. Guessing its thickness.."); 
        z_margin = 5.0;
    }

    // Figure out what a reasonable x-margin and y-margin are. 
    //
    // NOTE: Could also use (median? maximum?) distance from centroid to vertex.
    //       These should always be 1-2 voxels from the image edge, and are really unrelated from the z-margin. TODO.
    double x_margin = std::max<double>(2.0, z_margin);
    double y_margin = std::max<double>(2.0, z_margin);

    // Generate a grid volume bounding the ROI(s).
    const int64_t NumberOfChannels = 1;
    const double PixelFill = ExteriorVal; //std::numeric_limits<double>::quiet_NaN();
    const bool OnlyExtremeSlices = false;
    auto grid_image_collection = Contiguously_Grid_Volume<float,double>(
             cc_ROIs, 
             x_margin, y_margin, z_margin,
             params.GridRows, params.GridColumns, 
             NumberOfChannels, params.NumberOfImages,
             GridX, GridY, GridZ,
             PixelFill, OnlyExtremeSlices );

    // Generate an ROI inclusivity voxel map.
    {
        PartitionedImageVoxelVisitorMutatorUserData ud;

        // Use options from the user, but override what we need to for a valid mesh extraction.
        ud.mutation_opts = params.MutateOpts;

        ud.mutation_opts.editstyle = Mutate_Voxels_Opts::EditStyle::InPlace;
        ud.mutation_opts.aggregate = Mutate_Voxels_Opts::Aggregate::First;
        ud.mutation_opts.adjacency = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
        ud.mutation_opts.maskmod   = Mutate_Voxels_Opts::MaskMod::Noop;

        /// Leave the following options up to the caller.
        //ud.mutation_opts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Centre;
        //ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::ImplicitOrientations;

        ud.description = "ROI Inclusivity";

        ud.f_bounded = [&](int64_t /*row*/, int64_t /*col*/, int64_t /*chan*/,
                                          std::reference_wrapper<planar_image<float,double>> /*img_refw*/,
                                          std::reference_wrapper<planar_image<float,double>> /*mask_img_refw*/,
                                          float &voxel_val) {
            voxel_val = InteriorVal;
        };
        //ud.f_unbounded = [&](int64_t /*row*/, int64_t /*col*/, int64_t /*chan*/,
        //                                    std::reference_wrapper<planar_image<float,double>> /*img_refw*/,
        //                                    std::reference_wrapper<planar_image<float,double>> /*mask_img_refw*/,
        //                                    float &voxel_val) {
        //    voxel_val = ExteriorVal;
        //};

        if(!grid_image_collection.Process_Images_Parallel( GroupIndividualImages,
                                                           PartitionedImageVoxelVisitorMutator,
                                                           {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to create an ROI inclusivity map.");
        }
    }

    std::list<std::reference_wrapper<planar_image<float,double>>> grid_imgs;
    for(auto &img : grid_image_collection.images){
        grid_imgs.push_back( std::ref(img) );
    }

    if(!Images_Form_Rectilinear_Grid(grid_imgs)){
        throw std::logic_error("Grid images do not form a rectilinear grid. Cannot continue");
    }

    // Offload the actual Marching Cubes computation.
    return Marching_Cubes_Implementation( grid_imgs,
                                          std::shared_ptr<csg::sdf::node>(),
                                          inclusion_threshold,
                                          below_is_interior,
                                          params );
}

// Perform Marching Cubes using a user-provided signed-distance function.
fv_surface_mesh<double, uint64_t>
Estimate_Surface_Mesh_Marching_Cubes(
        std::shared_ptr<csg::sdf::node> sdf,
        const vec3<double>& minimum_resolution,
        double inclusion_threshold, // The voxel value threshold demarcating surface 'interior' and 'exterior.'
        bool below_is_interior,  // Controls how the inclusion_threshold is interpretted.
                                 // If true, anything <= is considered to be interior to the surface.
                                 // If false, anything >= is considered to be interior to the surface.
        Parameters params ){

    auto bb = sdf->evaluate_aa_bbox();
    if(!bb.min.isfinite() || !bb.max.isfinite()){
        throw std::invalid_argument("SDF produces a non-finite bounding box");
    }

    // Make an image volume that covers the bounding box + a margin.
    const double min_res_x = minimum_resolution.x;
    const double min_res_y = minimum_resolution.y;
    const double min_res_z = minimum_resolution.z;

    // Add a resolution-aware margin to the bounding box.
    const auto l_bb_min = bb.min;
    const auto l_bb_max = bb.max;
    bb.digest( l_bb_min - vec3<double>(min_res_x, min_res_y, min_res_z) * 2.0 );
    bb.digest( l_bb_max + vec3<double>(min_res_x, min_res_y, min_res_z) * 2.0 );

    const double margin_x = min_res_x; // Ensure at least one voxel surrounds entire object.
    const double margin_y = min_res_y;
    const double margin_z = min_res_z;

    const auto N_rows = static_cast<int64_t>(std::ceil((bb.max.x - bb.min.x)/min_res_x));
    const auto N_cols = static_cast<int64_t>(std::ceil((bb.max.y - bb.min.y)/min_res_y));
    const auto N_imgs = static_cast<int64_t>(std::ceil((bb.max.z - bb.min.z)/min_res_z));
    const auto N_chns = static_cast<int64_t>(1);

    const vec3<double> row_unit(1.0, 0.0, 0.0);
    const vec3<double> col_unit(0.0, 1.0, 0.0);

    const auto c1 = vec3<double>(bb.min.x, bb.min.y, bb.min.z);
    const auto c2 = vec3<double>(bb.max.x, bb.min.y, bb.min.z);
    const auto c3 = vec3<double>(bb.max.x, bb.max.y, bb.min.z);
    const auto c4 = vec3<double>(bb.min.x, bb.max.y, bb.min.z);

    const auto c5 = vec3<double>(bb.min.x, bb.min.y, bb.max.z);
    const auto c6 = vec3<double>(bb.max.x, bb.min.y, bb.max.z);
    const auto c7 = vec3<double>(bb.max.x, bb.max.y, bb.max.z);
    const auto c8 = vec3<double>(bb.min.x, bb.max.y, bb.max.z);

    contour_collection<double> cc;
    cc.contours.emplace_back(contour_of_points(std::list<vec3<double>>{{ c1, c2, c3, c4 }}));
    cc.contours.emplace_back(contour_of_points(std::list<vec3<double>>{{ c5, c6, c7, c8 }}));
    std::list<std::reference_wrapper<contour_collection<double>>> ccs;
    ccs.emplace_back( std::ref(cc) );

    csg::sdf::aa_bbox c_bb;
    for(const auto& cc_refw : ccs){
        for(const auto& c : cc_refw.get().contours){
            for(const auto& p : c.points){
                c_bb.digest(p);
            }
        }
    }
//YLOGINFO("Contours stretch from " << c_bb.min << " to " << c_bb.max);
//YLOGINFO("Using N_rows, N_cols, N_imgs = " << N_rows << ", " << N_cols << ", " << N_imgs);
//YLOGINFO("Margin {x,y,z} = " << margin_x << ", " << margin_y << ", " << margin_z);
//YLOGINFO("row_unit, col_unit = " << row_unit << ", " << col_unit);

    auto pic = Contiguously_Grid_Volume<float,double>(ccs,
                                                      margin_x, margin_y, margin_z,
                                                      N_rows, N_cols, N_chns, N_imgs,
                                                      row_unit, col_unit); //, img_unit);

    std::list<std::reference_wrapper<planar_image<float,double>>> imgs;
//csg::sdf::aa_bbox img_bb;
    for(auto& img : pic.images){
        imgs.emplace_back( std::ref(img) );
//img_bb.digest( img.position(0,0) );
//img_bb.digest( img.position(N_rows-1,0) );
//img_bb.digest( img.position(0,N_cols-1) );
//img_bb.digest( img.position(N_rows-1,N_cols-1) );
    }
//YLOGINFO("Images stretch from " << img_bb.min << " to " << img_bb.max);

    return Marching_Cubes_Implementation( imgs,
                                          sdf,
                                          inclusion_threshold,
                                          below_is_interior,
                                          params );
}

// This sub-routine performs the Marching Cubes algorithm for the provided images.
// Images should abut and not overlap; if they do, the generated surface will have seams where the images do not abut.
// Meshes with seams might be acceptable in some cases, so grids are only explicitly checked for rectilinearity.
//
fv_surface_mesh<double, uint64_t>
Estimate_Surface_Mesh_Marching_Cubes(
        const std::list<std::reference_wrapper<planar_image<float,double>>>& grid_imgs,
        double inclusion_threshold, // The voxel value threshold demarcating surface 'interior' and 'exterior.'
        bool below_is_interior,  // Controls how the inclusion_threshold is interpretted.
                                 // If true, anything <= is considered to be interior to the surface.
                                 // If false, anything >= is considered to be interior to the surface.
        Parameters params ){

    if(grid_imgs.empty()){
        throw std::invalid_argument("An insufficient number of images was provided. Cannot continue.");
    }
    if(!Images_Form_Rectilinear_Grid(grid_imgs)){
        throw std::logic_error("Grid images do not form a rectilinear grid. Cannot continue");
    }

    // Offload the actual Marching Cubes computation.
    return Marching_Cubes_Implementation( grid_imgs,
                                          std::shared_ptr<csg::sdf::node>(),
                                          inclusion_threshold,
                                          below_is_interior,
                                          params );
}

// This sub-routine assumes ROI contours are 'cylindrically' extruded 2D polygons with a fixed separation.
// ROI inclusivity is separately pre-computed before surface probing by generating an inclusivity mask on a
// custom-fitted planar image collection. This is done for performance purposes and so inclusivity and surface
// meshing can be separately tweaked as necessary.
//
// NOTE: This routine does not require the images that the contours were originally generated on.
//       A custom set of dummy images that contiguously cover all ROIs are generated and used internally.
//
// NOTE: This routine assumes all ROIs are co-planar.
//
// NOTE: This routine does not handle ROIs with several disconnected components (e.g., "eyes"). In such cases 
//       it is best to individually process each component (e.g., "left eye" and "right eye").
//
#ifdef DCMA_USE_CGAL
Polyhedron Estimate_Surface_Mesh(
        std::list<std::reference_wrapper<contour_collection<double>>> cc_ROIs,
        Parameters params ){

    // Figure out plane alignment and work out spacing. (Spacing is twice the thickness.)
    const auto est_cont_normal = cc_ROIs.front().get().contours.front().Estimate_Planar_Normal();
    const auto unique_planar_separation_threshold = 0.005; // Contours separated by less are considered to be on the same plane.
    const auto ucp = Unique_Contour_Planes(cc_ROIs, est_cont_normal, unique_planar_separation_threshold);
    
    // CGAL types.
    using STr = CGAL::Surface_mesh_default_triangulation_3;
    //using C2t3 = CGAL::Surface_mesh_complex_2_in_triangulation_3<STr>;

    using GT = STr::Geom_traits;
    using Sphere_3 = GT::Sphere_3;
    using Point_3 = GT::Point_3;
    using FT = GT::FT;

    // Construct a sphere surrounding the vertices to bound the surface.
    vec3<double> bounding_sphere_center;
    double bounding_sphere_radius; 
    const double extra_space = 1.0; //Extra space around each point, in DICOM coords.
    {
        using FT = double;
        using K = CGAL::Cartesian<FT>;
        using UseSqrts = CGAL::Tag_true;
        using Traits = CGAL::Min_sphere_of_spheres_d_traits_3<K,FT,UseSqrts>;
        using Min_sphere = CGAL::Min_sphere_of_spheres_d<Traits>;
        using Point = K::Point_3;
        using Sphere = Traits::Sphere;

        std::vector<Sphere> spheres;
        for(auto &cc_ref : cc_ROIs){
            for(const auto &c : cc_ref.get().contours){
                for(const auto &p : c.points){
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

// TODO: find vertex nearest to the bounding sphere centre and shift+grow the sphere so it is centred on a vertex.

    }
    YLOGINFO("Finished computing bounding sphere for selected ROIs; centre, radius = " << bounding_sphere_center << ", " << bounding_sphere_radius);

    // ============================================= Export the data ================================================
    // Export the contour vertices for inspection or processing with other tools.
/*    
    if(!params.OutfileBase.empty()){
        std::vector<Point_3> verts;
        std::ofstream out(params.OutfileBase + "_contour_vertices.xyz");

        for(auto &cc_ref : cc_ROIs){
            for(const auto &c : cc_ref.get().contours){
                for(const auto &p : c.points){
                    Point_3 cgal_p( p.x, p.y, p.z );
                    verts.push_back(cgal_p);
                }
            }
        }
        if(!out || !CGAL::write_xyz_points(out, verts)){
            throw std::runtime_error("Unable to write contour vertices.");
        } 
    }
*/

    // ============================================== Generate a grid  ==============================================

    // Compute the number of images to make into the grid: number of unique contour planes + 2.
    // The extra two help provide a buffer that simplifies interpolation.
    if(params.NumberOfImages <= 0) params.NumberOfImages = (ucp.size() + 2);
    YLOGINFO("Number of images: " << params.NumberOfImages);

    // Find grid alignment vectors.
    //
    // Note: Because we want to be able to compare images from different scans, we use a deterministic technique for
    //       generating two orthogonal directions involving the cardinal directions and Gram-Schmidt
    //       orthogonalization.
    const auto GridZ = est_cont_normal.unit();
    const auto pi = std::acos(-1.0);
    vec3<double> GridX = GridZ.rotate_around_z(pi * 0.5); // Try Z. Will often be idempotent.
    if(GridX.Dot(GridZ) > 0.25){
        GridX = GridZ.rotate_around_y(pi * 0.5);  //Should always work since GridZ is parallel to Z.
    }
    vec3<double> GridY = GridZ.Cross(GridX);
    if(!GridZ.GramSchmidt_orthogonalize(GridX, GridY)){
        throw std::runtime_error("Unable to find grid orientation vectors.");
    }
    GridX = GridX.unit();
    GridY = GridY.unit();

    // Figure out what z-margin is needed so the extra two images do not interfere with the grid lining up with the
    // contours. (Want exactly one contour plane per image.) So the margin should be large enough so the empty
    // images have no contours inside them, but small enough so that it doesn't affect the location of contours in the
    // other image slices. The ideal is if each image slice has the same thickness so contours are all separated by some
    // constant separation -- in this case we make the margin exactly as big as if two images were also included.
    double z_margin = 0.0;
    if(ucp.size() > 1){
        // Compute the total distance between the (centre of the) top and (centre of the) bottom planes.
        // (Note: the images associated with these contours will usually extend further. This is dealt with below.)
        const auto total_sep =  std::abs(ucp.front().Get_Signed_Distance_To_Point(ucp.back().R_0));
        const auto sep_per_plane = total_sep / static_cast<double>(ucp.size()-1);

        // Alternative computation of separations. It is more robust, but the whole procedure falls apart if the slices
        // are not regularly separated. Leaving here to potentially incorporate into Ygor or elsewhere at a later date.
        //std::vector<double> seps;
        //for(auto itA = std::begin(ucp); ; ++itA){
        //    auto itB = std::next(itA);
        //    if(itB == std::end(ucp)) break;
        //    seps.emplace_back( std::abs(itA->Get_Signed_Distance_To_Point(itB->R_0)) );
        //}
        //const auto sep_per_plane = Stats::YgorMedian(seps);

        // Add TOTAL zmargin of 1*sep_per_plane each for each extra images, and 0.5*sep_per_plane for each of the images
        // which will stick out beyond the contour planes. However, the margin is added to both the top and the bottom so
        // halve the total amount.
        z_margin = sep_per_plane * 1.5;

    }else{
        YLOGWARN("Only a single contour plane was detected. Guessing its thickness.."); 
        z_margin = 5.0;
    }

    // Figure out what a reasonable x-margin and y-margin are. 
    //
    // NOTE: Could also use (median? maximum?) distance from centroid to vertex.
    double x_margin = z_margin;
    double y_margin = z_margin;

    // Generate a grid volume bounding the ROI(s).
    const int64_t NumberOfChannels = 1;
    const double PixelFill = std::numeric_limits<double>::quiet_NaN();
    const bool OnlyExtremeSlices = false;
    auto grid_image_collection = Contiguously_Grid_Volume<float,double>(
             cc_ROIs, 
             x_margin, y_margin, z_margin,
             params.GridRows, params.GridColumns, 
             NumberOfChannels, params.NumberOfImages,
             GridX, GridY, GridZ,
             PixelFill, OnlyExtremeSlices );

    // Generate an ROI inclusivity voxel map.
    const auto InteriorVal = -1.0;
    const auto ExteriorVal = -InteriorVal;
    {
        PartitionedImageVoxelVisitorMutatorUserData ud;

        ud.mutation_opts.editstyle = Mutate_Voxels_Opts::EditStyle::InPlace;
        ud.mutation_opts.aggregate = Mutate_Voxels_Opts::Aggregate::First;
        ud.mutation_opts.adjacency = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
        ud.mutation_opts.maskmod   = Mutate_Voxels_Opts::MaskMod::Noop;
        ud.mutation_opts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Centre;
        ud.description = "ROI Inclusivity";

        /*
        if( std::regex_match(ContourOverlapStr, regex_ignore) ){
            ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::Ignore;
        }else if( std::regex_match(ContourOverlapStr, regex_honopps) ){
            ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::HonourOppositeOrientations;
        }else if( std::regex_match(ContourOverlapStr, regex_cancel) ){
            ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::ImplicitOrientations;
        }else{
            throw std::invalid_argument("ContourOverlap argument '"_s + ContourOverlapStr + "' is not valid");
        }
        */
        ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::Ignore;

        ud.f_bounded = [&](int64_t /*row*/, int64_t /*col*/, int64_t /*chan*/,
                           std::reference_wrapper<planar_image<float,double>> /*img_refw*/,
                           std::reference_wrapper<planar_image<float,double>> /*mask_img_refw*/,
                           float &voxel_val) {
            voxel_val = InteriorVal;
        };
        ud.f_unbounded = [&](int64_t /*row*/, int64_t /*col*/, int64_t /*chan*/,
                             std::reference_wrapper<planar_image<float,double>> /*img_refw*/,
                             std::reference_wrapper<planar_image<float,double>> /*mask_img_refw*/,
                             float &voxel_val) {
            voxel_val = ExteriorVal;
        };

        if(!grid_image_collection.Process_Images_Parallel( GroupIndividualImages,
                                                           PartitionedImageVoxelVisitorMutator,
                                                           {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to create an ROI inclusivity map.");
        }
    }
    
    // Inspect the grid by exporting since injecting into DICOM_data is not possible.
    //for(auto & img : grid_image_collection.images){
    //    const auto filename = Get_Unique_Sequential_Filename("./inspect_", 6, ".fits");
    //    WriteToFITS(img, filename);
    //}
    //for(auto & img : grid_image_collection.images){
    //    std::cout << "Image z: " << img.center() << std::endl;
    //}


    // ============================================== Modify the mask ==============================================
/*
    // Gaussian blur to help smooth the sharp edges.
    grid_image_collection.Gaussian_Pixel_Blur( {}, 2.0 ); // Sigma in terms of pixel count.

    // Supersample the surface mask.
    {
        InImagePlaneBicubicSupersampleUserData bicub_ud;
        bicub_ud.RowScaleFactor    = 3;
        bicub_ud.ColumnScaleFactor = 3;

        if(!grid_arr_ptr->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                             InImagePlaneBicubicSupersample,
                                                             {}, {}, &bicub_ud )){
            YLOGERR("Unable to bicubically supersample surface mask");
        }
    }

    // Threshold the surface mask.
    grid_arr_ptr->imagecoll.apply_to_pixels([=](int64_t, int64_t, int64_t, float &val) -> void {
            if(!isininc(void_mask_val, val, surface_mask_val)){
                val = void_mask_val;
                return;
            }

            if( (val - void_mask_val) > 0.25*(surface_mask_val - void_mask_val) ){
                val = surface_mask_val;
            }else{
                val = void_mask_val;
            }
            return;
    });
*/


    // ============================================ Sample the surface  =============================================

    // This routine is an 'oracle' that reports if a given point is inside or outside the surface to be triangulated.
    // The surface is implicitly defined by the isosurface(s) where this function is zero.
    // This oracle uses contour inclusivity pre-computed over a grid to speed up the surface probing process.
    auto surface_oracle = [&](Point_3 p) -> FT {
        const vec3<double> P(static_cast<double>(CGAL::to_double(p.x())), 
                             static_cast<double>(CGAL::to_double(p.y())),
                             static_cast<double>(CGAL::to_double(p.z())) );
        const int64_t channel = 0;
        const double out_of_bounds = ExteriorVal;

        return static_cast<FT>( grid_image_collection.trilinearly_interpolate(P, channel, out_of_bounds) );
    };

    const Point_3 cgal_bounding_sphere_center(bounding_sphere_center.x, bounding_sphere_center.y, bounding_sphere_center.z );
    const Sphere_3 cgal_bounding_sphere( cgal_bounding_sphere_center, std::pow(bounding_sphere_radius,2.0) );

    // Define some CGAL types for meshing purposes.

    // Deprecation handling: for CGAL v4.13 and earlier.
    using Implicit_Function = decltype(surface_oracle);
    using Mesh_domain = CGAL::Mesh_domain_with_polyline_features_3< CGAL::Implicit_mesh_domain_3<Implicit_Function,Kernel> >;

    // Deprecation handling: for CGAL v4.13 and later.
    //using Mesh_domain = CGAL::Mesh_domain_with_polyline_features_3< CGAL::Labeled_mesh_domain_3<Kernel> >;

    using Polyline_3 = std::vector<Point_3>;

    //using Concurrency_tag = CGAL::Parallel_tag;
    using Concurrency_tag = CGAL::Sequential_tag; // CGAL::Parallel_tag; // <-- requires Intel TBB library.

    using Tr = CGAL::Mesh_triangulation_3<Mesh_domain,CGAL::Default,Concurrency_tag>::type;
    using C3t3 = CGAL::Mesh_complex_3_in_triangulation_3<Tr,Mesh_domain::Corner_index,Mesh_domain::Curve_index>;

    using Mesh_criteria = CGAL::Mesh_criteria_3<Tr>;


    // --- Request that the input contours be protected in the final mesh. ---
    //
    // For fast-quality meshing, no contours are protected.
    // For medium-quality meshing, all contours are protected but might be extremely thin and likely to be smoothed.
    // For high-quality meshing, all contours are protected and smoothing is protected against by extruding the contours.
  
    std::vector<double> dUs; // Extrusion distances along the contour normal.
    if(params.RQ == ReproductionQuality::Fast){
        dUs.push_back(0.0);
    }else if(params.RQ == ReproductionQuality::Medium){
        dUs.push_back(0.0);
    }else if(params.RQ == ReproductionQuality::High){
        if(ucp.size() > 1){
            // Compute the total distance between the (centre of the) top and (centre of the) bottom planes.
            const auto total_sep =  std::abs(ucp.front().Get_Signed_Distance_To_Point(ucp.back().R_0));
            const auto sep_per_plane = total_sep / static_cast<double>(ucp.size()-1);
  
            dUs.push_back(-0.25*sep_per_plane);
            dUs.push_back( 0.25*sep_per_plane);
        }else{
            // If there is no scale available, guess something reasonable.
            dUs.push_back(-0.1);
            dUs.push_back( 0.1);
        }
    }
  
    std::list<Polyline_3> polylines; // Convert extruded contours to CGAL polyline format.
    for(auto &cc_ref : cc_ROIs){
        for(const auto &c : cc_ref.get().contours){
            for(const auto &dU : dUs){
                polylines.emplace_back();
                for(const auto &p : c.points){
                    auto shifted_p = p + est_cont_normal * dU; 
                    Point_3 cgal_p( shifted_p.x, shifted_p.y, shifted_p.z );
                    polylines.back().push_back( cgal_p );
                }
                if(polylines.back().empty()){
                    polylines.pop_back();
                }else{
                    polylines.back().push_back( polylines.back().front() ); // Close the loop.
                }
            }
        }
    }
  
   
    // Purge some contours and rely on interpolation for faster meshing.
    //
    // NOTE: Should rather purge unique contour planes, because this approach can lead to pathologically missing whole
    //       subvolumes when there are %2=0 contours on each plane.
    if(params.RQ == ReproductionQuality::Fast){
        size_t count = 0;
        polylines.remove_if( [&](const Polyline_3 &) -> bool {
            return ( (++count % 2) == 0 );
        });
    }
  
  
    // Create the meshing domain using a bounding sphere.
    double err_bound;
    if(params.RQ == ReproductionQuality::Fast){
        err_bound = 0.50 / bounding_sphere_radius;
    }else if(params.RQ == ReproductionQuality::Medium){
        err_bound = 0.15 / bounding_sphere_radius;
    }else if(params.RQ == ReproductionQuality::High){
        err_bound = 0.05 / bounding_sphere_radius;
    }

    // Deprecation handling: for CGAL v4.13 and earlier.
    Mesh_domain domain(surface_oracle,
                       cgal_bounding_sphere, 
                       err_bound);

    // Deprecation handling: for CGAL v4.13 and later.
    //Mesh_domain domain = Mesh_domain::create_implicit_mesh_domain(
    //                       CGAL::parameters::function = surface_oracle,
    //                       CGAL::parameters::bounding_object = cgal_bounding_sphere,
    //                       CGAL::parameters::relative_error_bound = err_bound);

    domain.add_features(polylines.begin(), polylines.end());
  
  
    // Attach meshing criteria.
    Mesh_criteria criteria( 
                            // For 1D features.
                            //
                            // Maximum sampling distance along 1D features.                                 
                            // Setting below the smallest contour vertex-to-vertex spacing will result                                                   
                            // in interpolation between the vertices. It probably won't increase overall
                            // mesh quality, but will add additional complexity to the mesh.
                            CGAL::parameters::edge_size = 1.0, 
  
                            // For surfaces.
                            //
                            // Facet angle is a lower bound, in degrees. Should be <=30.
                            // High angles mostly relevant for visual quality.
                            CGAL::parameters::facet_angle = 15.0, 
  
                            CGAL::parameters::facet_size = 2.0, 
                            CGAL::parameters::facet_distance = 0.5,
                            //CGAL::parameters::facet_topology = CGAL::FACET_VERTICES_ON_SAME_SURFACE_PATCH,
  
                            // For tetrahedra.
                            //
                            // Cell radius edge ratio should be >= 2.
                            CGAL::parameters::cell_radius_edge_ratio = 5.0,
                            CGAL::parameters::cell_size = 5.0 );
  
  
    // Perform the meshing.
    YLOGINFO("Beginning meshing. This may take a while");
    C3t3 c3t3 = CGAL::make_mesh_3<C3t3>(domain, 
                                        criteria, 
                                        //CGAL::parameters::lloyd(CGAL::parameters::time_limit=10.0),
                                        CGAL::parameters::no_lloyd(),
                                        CGAL::parameters::no_odt(), 
                                        //CGAL::parameters::exude(CGAL::parameters::time_limit=10.0), 
                                        CGAL::parameters::no_exude(), 
                                        CGAL::parameters::no_perturb(),
                                        CGAL::parameters::manifold());
  
    // Output the mesh for inspection.
    //std::ofstream off_file("/tmp/out.off");
    //c3t3.output_boundary_to_off(off_file);
  
    // Refine the mesh with additional optimization passes.
    //
    // NOTE: This optimization might not be appropriate for our use-cases. In particular the refine may invalidate
    //       meshing criteria by optimizing connectivity.
    if(params.RQ == ReproductionQuality::High){
        YLOGINFO("Refining mesh. This may take a while");
        CGAL::refine_mesh_3(c3t3, 
                            domain, 
                            criteria,
                            CGAL::parameters::lloyd(CGAL::parameters::time_limit=10.0),
                            //CGAL::parameters::no_lloyd(),
                            CGAL::parameters::no_odt(), 
                            CGAL::parameters::exude(CGAL::parameters::time_limit=10.0), 
                            //CGAL::parameters::no_exude(), 
                            CGAL::parameters::no_perturb(),
                            CGAL::parameters::manifold());
    }
  
  
    // Extract the polyhedral surface.
    Polyhedron output_mesh;
    try{
        CGAL::facets_in_complex_3_to_triangle_mesh(c3t3, output_mesh);
    }catch(const std::exception &e){
        throw std::runtime_error(std::string("Could not convert surface mesh to a polyhedron representation: ") + e.what());
    }
    YLOGINFO("The triangulated surface has " << output_mesh.size_of_vertices() << " vertices"
             " and " << output_mesh.size_of_facets() << " faces");
  
    // Remove disconnected vertices, if there are any.
    const auto removed_verts = CGAL::Polygon_mesh_processing::remove_isolated_vertices(output_mesh);
    if(removed_verts != 0){
        YLOGWARN(removed_verts << " isolated vertices were removed");
    }
  
    return output_mesh;
}
#endif // DCMA_USE_CGAL




// This is a WIP surface meshing routine that may be removed some time in the future.
#ifdef DCMA_USE_CGAL
Polyhedron Estimate_Surface_Mesh_AdvancingFront(
        std::list<std::reference_wrapper<contour_collection<double>>> cc_ROIs,
        Parameters ){
    
    // Perform an advancing-front surface reconstruction from a point-set.
    using Facet = CGAL::cpp11::array<std::size_t,3>;
    using Point_3 = Kernel::Point_3;
    using Mesh = CGAL::Surface_mesh<Point_3>;
    using PointList = std::vector<Point_3>;
    using PointIterator = PointList::iterator;

    struct Construct{
        Mesh& mesh;

        Construct(Mesh& mesh, PointIterator a, PointIterator b) : mesh(mesh){
            for( ; a != b; ++a){
                boost::graph_traits<Mesh>::vertex_descriptor v;
                v = add_vertex(mesh);
                mesh.point(v) = *a;
            }
        }

        Construct& operator=(const Facet f){
            using vertex_descriptor = boost::graph_traits<Mesh>::vertex_descriptor;
            using size_type = boost::graph_traits<Mesh>::vertices_size_type;
            mesh.add_face( vertex_descriptor(static_cast<size_type>(f[0])),
                           vertex_descriptor(static_cast<size_type>(f[1])),
                           vertex_descriptor(static_cast<size_type>(f[2])) );
            return *this;
        }

        Construct& operator*() { return *this; }
        Construct& operator++() { return *this; }
        Construct operator++(int) { return *this; }
    };


    PointList points;
    auto Feq = [](const vec3<double> &a, const vec3<double> &b) -> bool {
        return (a.distance(b) < 1E-3);
    };
    for(auto &cc_ref : cc_ROIs){
        for(const auto &c : cc_ref.get().contours){
            auto clean = c;
            clean.Remove_Sequential_Duplicate_Points(Feq);
            clean.Remove_Extraneous_Points(Feq);
            clean.Remove_Needles(Feq);
            clean.Remove_Extraneous_Points(Feq);
            clean.Remove_Sequential_Duplicate_Points(Feq);

            for(const auto &p : clean.points){
                Point_3 cgal_p( p.x, p.y, p.z );
                points.push_back(cgal_p);
            }
        }
    }

    // ============================================= Export the data ================================================
    // Export the contour vertices for inspection or processing with other tools.
/*    
    if(!params.OutfileBase.empty()){
        std::ofstream out(params.OutfileBase + "_contour_vertices.xyz");
        if(!out || !CGAL::write_xyz_points(out, points)){
            throw std::runtime_error("Unable to write contour vertices.");
        } 
    }
*/

    const double radius_ratio_bound = 0.1;
    const auto pi = std::acos(-1.0);
    const double beta = pi*0.05/6.0;

    Mesh output_mesh;
    Construct construct(output_mesh, points.begin(), points.end());
    CGAL::advancing_front_surface_reconstruction(points.begin(), points.end(), construct, radius_ratio_bound, beta);
    
    // Remove disconnected vertices.
    const auto removed_verts = CGAL::Polygon_mesh_processing::remove_isolated_vertices(output_mesh);
    if(removed_verts != 0){
        YLOGWARN(removed_verts << " isolated vertices were removed");
    }

    // Retain only the largest surface component.
/*    
    const auto removed_components = output_mesh.keep_largest_connected_components(1);
    if(removed_components != 0){
        YLOGWARN(removed_components << " of the smallest mesh components were removed");
    }
*/

    YLOGERR("This routine is not complete. Need to convert mesh types");
    return Polyhedron();
}
#endif // DCMA_USE_CGAL


// Convert from fv_surface_mesh to CGAL's Polyhedron class.
//
// The former can be non-manifold, but the latter cannot.
// This routine attempts to correct non-manifoldness. It may not work, but could carry on silently.
// If it matters, attempt a direct conversion without non-manifold correction.
#ifdef DCMA_USE_CGAL
Polyhedron
FVSMeshToPolyhedron(
        const fv_surface_mesh<double, uint64_t> &in ){

    
    std::vector<Kernel::Point_3>         triangle_verts;
    std::vector< std::array<size_t, 3> > triangle_faces;


    // Populate the triangle soup.
    for(const auto &v : in.vertices){
        triangle_verts.emplace_back( Kernel::Point_3( v.x, v.y, v.z ) );
    }
    std::array<size_t, 3> shtl;
    for(const auto &f : in.faces){
        if(f.size() == 3){
            shtl[0] = f[0];
            shtl[1] = f[1];
            shtl[2] = f[2];
            triangle_faces.emplace_back( shtl );
        }else if(f.size() == 4){
            shtl[0] = f[0];
            shtl[1] = f[1];
            shtl[2] = f[2];
            triangle_faces.emplace_back( shtl );
            shtl[0] = f[0];
            shtl[1] = f[2];
            shtl[2] = f[3];
            triangle_faces.emplace_back( shtl );
        }else{
            throw std::runtime_error("This routine only supports triangle and quad faces. Cannot continue.");
        }
    }

    YLOGINFO("Orienting face normals..");
    CGAL::Polygon_mesh_processing::orient_polygon_soup(triangle_verts, triangle_faces);

    // Output the mesh for inspection.
    //std::ofstream off_file("/tmp/out.off");
    //c3t3.output_boundary_to_off(off_file);
  
    // Extract the polyhedral surface.
    YLOGINFO("Extracting the polyhedral surface..");
    Polyhedron output_mesh;
    CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(triangle_verts, triangle_faces, output_mesh);

//    if(!CGAL::is_closed(output_mesh)){
//        std::ofstream openmesh("/tmp/open_mesh.off");
//        openmesh << output_mesh;
//        throw std::runtime_error("Mesh not closed but should be. Verify vector equality is not too loose. Dumped to /tmp/open_mesh.off.");
//    }
    if(!CGAL::Polygon_mesh_processing::is_outward_oriented(output_mesh)){
        YLOGINFO("Reorienting face orientation so faces face outward..");
        CGAL::Polygon_mesh_processing::reverse_face_orientations(output_mesh);
    }

    YLOGINFO("The triangulated surface has " << output_mesh.size_of_vertices() << " vertices"
             " and " << output_mesh.size_of_facets() << " faces");
  
    // Remove disconnected vertices, if there are any.
    const auto removed_verts = CGAL::Polygon_mesh_processing::remove_isolated_vertices(output_mesh);
    if(removed_verts != 0){
        YLOGWARN(removed_verts << " isolated vertices were removed");
    }

    return output_mesh;
}
#endif // DCMA_USE_CGAL


} // namespace dcma_surface_meshes.



#ifdef DCMA_USE_CGAL
namespace polyhedron_processing {

using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
using Polyhedron = CGAL::Polyhedron_3<Kernel>;

// This routine is a factory function for a regular icosahedron surface mesh.
// It can be subdivided to quickly approach a spherical surface.
// This routine is meant to assist surface mesh dilation and Minkowski sums.
Polyhedron
Regular_Icosahedron(double radius){
    std::stringstream ss;
    ss << "OFF" << std::endl
       << "12 20 0" << std::endl
       << "0 1.618034 1 " << std::endl  // The vertices.
       << "0 1.618034 -1 " << std::endl
       << "0 -1.618034 1 " << std::endl
       << "0 -1.618034 -1 " << std::endl
       << "1.618034 1 0 " << std::endl
       << "1.618034 -1 0 " << std::endl
       << "-1.618034 1 0 " << std::endl
       << "-1.618034 -1 0 " << std::endl
       << "1 0 1.618034 " << std::endl
       << "-1 0 1.618034 " << std::endl
       << "1 0 -1.618034 " << std::endl
       << "-1 0 -1.618034 " << std::endl
       << "3 1 0 4" << std::endl          // The faces.
       << "3 0 1 6" << std::endl
       << "3 2 3 5" << std::endl
       << "3 3 2 7" << std::endl
       << "3 4 5 10" << std::endl
       << "3 5 4 8" << std::endl
       << "3 6 7 9" << std::endl
       << "3 7 6 11" << std::endl
       << "3 8 9 2" << std::endl
       << "3 9 8 0" << std::endl
       << "3 10 11 1" << std::endl
       << "3 11 10 3" << std::endl
       << "3 0 8 4" << std::endl
       << "3 0 6 9" << std::endl
       << "3 1 4 10" << std::endl
       << "3 1 11 6" << std::endl
       << "3 2 5 8" << std::endl
       << "3 2 9 7" << std::endl
       << "3 3 10 5" << std::endl
       << "3 3 7 11" << std::endl;

    Polyhedron output_mesh;
    ss >> output_mesh;

    CGAL::Aff_transformation_3<Kernel> Aff(CGAL::SCALING, radius);
    std::transform( output_mesh.points_begin(), output_mesh.points_end(),
                    output_mesh.points_begin(), Aff );

    return output_mesh;
}


void
Subdivide(Polyhedron &mesh,
          int64_t iters){

    if(!mesh.is_pure_triangle()) throw std::runtime_error("Mesh is not purely triangular.");
    if(!mesh.is_valid()) throw std::runtime_error("Mesh is not combinatorially valid.");
    //if(!mesh.is_closed()) throw std::runtime_error("Mesh is not closed; it has a boundary")

    YLOGINFO("About to perform mesh subdivision. If this fails, the mesh topology is probably incompatible");
    for(int64_t i = 0; i < iters; ++i){
        // Note: the following takes a parameter indicating the number of interations to perform, but seems to ignore
        // it. (Perhaps the interface has changed?)
        CGAL::Subdivision_method_3::Loop_subdivision(mesh);
        //CGAL::Subdivision_method_3::DooSabin_subdivision(output_mesh,params.MeshingSubdivisionIterations);
        //CGAL::Subdivision_method_3::Sqrt3_subdivision(output_mesh,params.MeshingSubdivisionIterations);
        YLOGINFO("The subdivided surface has " << mesh.size_of_vertices() << " vertices"
                 " and " << mesh.size_of_facets() << " faces");
    }
    return;
}

void
Remesh(Polyhedron &mesh,
       double target_edge_length,
       int64_t iters){

    // This function remeshes a given mesh to improve the triangle quality. From the CGAL documentation:
    //    "The incremental triangle-based isotropic remeshing algorithm introduced by Botsch et al [2], [4] is implemented
    //     in this package. This algorithm incrementally performs simple operations such as edge splits, edge collapses,
    //     edge flips, and Laplacian smoothing. All the vertices of the remeshed patch are reprojected to the original
    //     surface to keep a good approximation of the input."
    if(!mesh.is_pure_triangle()) throw std::runtime_error("Mesh is not purely triangular.");
    if(!mesh.is_valid()) throw std::runtime_error("Mesh is not combinatorially valid.");
    if(!mesh.is_closed()) throw std::runtime_error("Mesh is not closed; it has a boundary. Refusing to continue.");

    if(iters > 0){
        // Polyhedron_3 do not have a queryable face_index_map, so we need to provide one.
        using face_descriptor = boost::graph_traits<Polyhedron>::face_descriptor;
        std::map<face_descriptor, std::size_t> fi_map;
        std::size_t id = 0;
        for(auto &f : faces(mesh)){
            fi_map[f] = id++;
        }
    
        CGAL::Polygon_mesh_processing::isotropic_remeshing(
                faces(mesh),
                target_edge_length,
                mesh,
                CGAL::Polygon_mesh_processing::parameters::number_of_iterations(iters)
                  .face_index_map(boost::make_assoc_property_map(fi_map))
        );
        YLOGINFO("The remeshed surface has " << mesh.size_of_vertices() << " vertices"
                 " and " << mesh.size_of_facets() << " faces");
    }
    return;
}    

void
Simplify(Polyhedron &mesh,
         int64_t edge_count_limit){

    // The required parameter is the upper limit to the number of faces remaining after mesh simplification.
    // For a genus 0 surface mesh (i.e., topologically euivalent to a sphere without holes like a torus)
    // the number of faces (f), edges (e), and vertices (v) are related via the Euler relation for convex
    // 3D polyhedra: v + f - e = 2. 
    // If the mesh is purely triangle, e ~= 3*v and f ~= 2*v so e ~= 3*v ~= 1.5*f. So the number of faces
    // and vertices can be controlled by limiting the number of edges.

    if(!mesh.is_pure_triangle()) throw std::runtime_error("Mesh is not purely triangular.");
    if(!mesh.is_valid()) throw std::runtime_error("Mesh is not combinatorially valid.");
    //if(!mesh.is_closed()) throw std::runtime_error("Mesh is not closed; it has a boundary");

    YLOGINFO("About to perform mesh simplification. If this fails, the mesh topology is probably incompatible");
    if(edge_count_limit > 0){
        CGAL::Surface_mesh_simplification::Count_stop_predicate<Polyhedron> stop_crit(edge_count_limit);
  
        const auto rem = CGAL::Surface_mesh_simplification::edge_collapse(
                                 mesh, 
                                 stop_crit,
                                 CGAL::parameters::vertex_index_map(get(CGAL::vertex_external_index, mesh))
                                                  .halfedge_index_map(get(CGAL::halfedge_external_index, mesh))
                                                  .get_cost(CGAL::Surface_mesh_simplification::Edge_length_cost<Polyhedron>())
                                                  .get_placement(CGAL::Surface_mesh_simplification::Midpoint_placement<Polyhedron>()) );

        YLOGINFO("Removed " << rem << " edges (" << edge_count_limit << " remain)");
        YLOGINFO("The simplified surface now has " << mesh.size_of_vertices() << " vertices"
                 " and " << mesh.size_of_facets() << " faces");
    }

    return;
}


bool
SaveAsOFF(Polyhedron &mesh,
          const std::string& filename){
    
    if(!filename.empty()){
        std::ofstream out(filename);
        if(!( out << mesh )) return false;
    }

    return true;
}



// This routine uses the divide-and-conquer strategy to compute full 3D Minkowski sum of arbitrary nef polyhedra,
// breaking them into sets of connected convex sub-polyhedra and performing a convex Minkowski sum for every pair. 
// It has bad algorithmic complexity: O(m^{3} n^{3}) where n and m are the complexity of the input polyhedra.
// Expect to wait for several tens of seconds, even with small meshes. The first mesh is dilated in-place and should 
// have around 750 faces or less. The 'sphere' mesh should also be small, probably 50 faces or less for a reasonable
// runtime. Refer to the CGAL user guide for timing information.
void Dilate( Polyhedron &mesh, 
             const Polyhedron& sphere ){

    if(!mesh.is_closed()){
        throw std::invalid_argument("Mesh is not closed. Unable to handle meshes with boundaries. Cannot continue.");
    }
    if(!sphere.is_closed()){
        throw std::invalid_argument("Sphere mesh is not closed. Unable to handle meshes with boundaries. Cannot continue.");
    }

    using NefKernel = CGAL::Exact_predicates_exact_constructions_kernel;
    using Nef_polyhedron = CGAL::Nef_polyhedron_3<NefKernel>;

    // Convert between Polyhedra (with EPIC kernel) and Nef_Polyhedra (with EPEC kernel) via conversion to OFF and back.
    //
    // Note: This is obviously not ideal, but it severs the coupling between polyhedra and it was not possible to have
    //       a uniform kernel for both.
    Nef_polyhedron nef_mesh;
    std::stringstream ss_mesh_off;
    ss_mesh_off << mesh;
    CGAL::OFF_to_nef_3(ss_mesh_off, nef_mesh);
    ss_mesh_off.clear();

    Nef_polyhedron nef_sphere;
    std::stringstream ss_sphere_off;
    ss_sphere_off << sphere;
    CGAL::OFF_to_nef_3(ss_sphere_off, nef_sphere);
    ss_sphere_off.clear();

    YLOGINFO("About to compute 3D Minkowski sum");
    Nef_polyhedron result = CGAL::minkowski_sum_3(nef_mesh, nef_sphere);

    if(!result.is_simple()){
        throw std::invalid_argument("Dilated mesh is not simple. Unable to convert to polyhedron. Cannot continue.");
    }
    const bool triangulate_all_faces = true;
    CGAL::convert_nef_polyhedron_to_polygon_mesh(result, mesh, triangulate_all_faces);
    
    return;
}



// This routine is similar to the other Dilate() but operates on contour vertices. It may be faster, but conversely if
// the dilation is too small relative to the maximum vertex-vertex distance, the resulting mesh may be incomplete or
// have holes.
void Dilate( Polyhedron &output_mesh, 
             std::list<std::reference_wrapper<contour_collection<double>>> cc_ROIs,
             const Polyhedron& sphere ){

    if(!sphere.is_closed()){
        throw std::invalid_argument("Sphere mesh is not closed. Unable to handle meshes with boundaries. Cannot continue.");
    }

    //using NefKernel = CGAL::Exact_predicates_exact_constructions_kernel;
    using NefKernel = CGAL::Exact_predicates_exact_constructions_kernel;
    using Nef_polyhedron = CGAL::Nef_polyhedron_3<NefKernel>;
    using Point_3 = NefKernel::Point_3;
    using Polyline_3 = std::vector<Point_3>;

    auto Feq = [](const vec3<double> &a, const vec3<double> &b) -> bool {
        return (a.distance(b) < 1E-3);
    };


    // Convert between Polyhedra (with EPIC kernel) and Nef_Polyhedra (with EPEC kernel) via conversion to OFF and back.
    //
    // Note: This is obviously not ideal, but it severs the coupling between polyhedra and it was not possible to have
    //       a uniform kernel for both.
    Nef_polyhedron nef_sphere;
    std::stringstream ss_sphere_off;
    ss_sphere_off << sphere;
    CGAL::OFF_to_nef_3(ss_sphere_off, nef_sphere);
    ss_sphere_off.clear();


/*
    // Vertex-based Nef polyhedron.
    std::vector<Point_3> points;
    for(auto &cc_ref : cc_ROIs){
        for(const auto &c : cc_ref.get().contours){
            auto clean = c;
            clean.Remove_Sequential_Duplicate_Points(Feq);
            clean.Remove_Extraneous_Points(Feq);
            clean.Remove_Needles(Feq);
            clean.Remove_Extraneous_Points(Feq);
            clean.Remove_Sequential_Duplicate_Points(Feq);

            for(const auto &p : clean.points){
                Point_3 cgal_p( p.x, p.y, p.z );
                points.push_back(cgal_p);
            }
        }
    }

    Nef_polyhedron nef_mesh(points.begin(), points.end(), Nef_polyhedron::Points_tag());
*/


/*
    // Use Nef Boolean operations to combine individual polygons.
    Nef_polyhedron nef_mesh;
    for(auto &cc_ref : cc_ROIs){
        for(const auto &c : cc_ref.get().contours){
            auto clean = c;
            clean.Remove_Sequential_Duplicate_Points(Feq);
            clean.Remove_Extraneous_Points(Feq);
            clean.Remove_Needles(Feq);
            clean.Remove_Extraneous_Points(Feq);
            clean.Remove_Sequential_Duplicate_Points(Feq);

            Polyline_3 polyline; // Convert extruded contours to CGAL polyline format.
            for(const auto &p : clean.points){
                Point_3 cgal_p( p.x, p.y, p.z );
                polyline.push_back( cgal_p );
            }
            if(!polyline.empty()){
                polyline.push_back( polyline.front() ); // Close the loop.
                Nef_polyhedron loop( polyline.begin(), polyline.end() );
                nef_mesh += loop;
            }
        }
    }


    // Use polylines directly from the ROIs.
    std::list<Polyline_3> polylines; // Convert extruded contours to CGAL polyline format.
    for(auto &cc_ref : cc_ROIs){
        for(const auto &c : cc_ref.get().contours){
            auto clean = c;
            clean.Remove_Sequential_Duplicate_Points(Feq);
            clean.Remove_Extraneous_Points(Feq);
            clean.Remove_Needles(Feq);
            clean.Remove_Extraneous_Points(Feq);
            clean.Remove_Sequential_Duplicate_Points(Feq);

            polylines.emplace_back();
            for(const auto &p : clean.points){
                Point_3 cgal_p( p.x, p.y, p.z );
                polylines.back().push_back( cgal_p );
            }
            if(polylines.back().size() >= 3){
                polylines.back().push_back( polylines.back().front() ); // Close the loop.
            }else{
                polylines.pop_back(); // Purge the contour.
            }
        }
    }
    using Polyline_iter = Polyline_3::iterator;
    std::list<std::pair<Polyline_iter,Polyline_iter>> polylines_first_end;
    for(auto &p : polylines) polylines_first_end.emplace_back( std::make_pair( p.begin(), p.end() ) );
    Nef_polyhedron nef_mesh(polylines_first_end.begin(), polylines_first_end.end(), Nef_polyhedron::Polylines_tag());

*/


    Nef_polyhedron amal;


    // Use polylines directly from the ROIs, but process them one-at-a-time.
    //
    // Note: CGAL was occasionally seg faulting when all contours were processed in a single attempt.
    //       Exposing in a loop also permits easier concurrency.
    for(auto &cc_ref : cc_ROIs){
        for(const auto &c : cc_ref.get().contours){

            std::list<Polyline_3> polylines; // Convert extruded contours to CGAL polyline format.

            auto clean = c;
            clean.Remove_Sequential_Duplicate_Points(Feq);
            clean.Remove_Extraneous_Points(Feq);
            clean.Remove_Needles(Feq);
            clean.Remove_Extraneous_Points(Feq);
            clean.Remove_Sequential_Duplicate_Points(Feq);

            polylines.emplace_back();
            for(const auto &p : clean.points){
                Point_3 cgal_p( p.x, p.y, p.z );
                polylines.back().push_back( cgal_p );
            }
            if(polylines.back().size() >= 3){
                polylines.back().push_back( polylines.back().front() ); // Close the loop.

                using Polyline_iter = Polyline_3::iterator;
                std::list<std::pair<Polyline_iter,Polyline_iter>> polylines_first_end;
                for(auto &p : polylines) polylines_first_end.emplace_back( std::make_pair( p.begin(), p.end() ) );
                Nef_polyhedron nef_mesh(polylines_first_end.begin(), polylines_first_end.end(), Nef_polyhedron::Polylines_tag());

                YLOGINFO("About to compute 3D Minkowski sum for a single contour");
                Nef_polyhedron result = CGAL::minkowski_sum_3(nef_mesh, nef_sphere);

                // TODO: select outer or inner contours for dilation and erosion, respectively.

                amal += result;
            }

        }
    }

    const bool triangulate_all_faces = true;
    CGAL::convert_nef_polyhedron_to_polygon_mesh(amal, output_mesh, triangulate_all_faces);

    return;


    // TODO: to compute the erosion, take the complement of the nef_mesh, perform a Minkowski sum, and then complement again.

/*
    YLOGINFO("About to compute 3D Minkowski sum");
    Nef_polyhedron result = CGAL::minkowski_sum_3(nef_mesh, nef_sphere);

    //if(!result.is_simple()){
    //    throw std::invalid_argument("Dilated mesh is not simple. Unable to convert to polyhedron. Cannot continue.");
    //}
    const bool triangulate_all_faces = true;
    CGAL::convert_nef_polyhedron_to_polygon_mesh(result, output_mesh, triangulate_all_faces);
*/


    return;
}

// Approximate dilation, erosion, or core/peel using approximate Minkowski sums/differences with a sphere-like shape.
void
Transform( Polyhedron &output_mesh,
           double distance,
           TransformOp op){


    const auto N_edges = output_mesh.size_of_halfedges() / 2;

    //Generate a Nef polyhedron from the surface mesh.
    using NefKernel = CGAL::Exact_predicates_exact_constructions_kernel;
    using Nef_polyhedron = CGAL::Nef_polyhedron_3<NefKernel>;
    using Vector_3 = NefKernel::Vector_3;


    const auto Poly_to_NefPoly = [=]( const Polyhedron &poly ) -> Nef_polyhedron {
        Nef_polyhedron nef_poly;
        std::stringstream ss_off;
        ss_off << poly;

        CGAL::OFF_to_nef_3(ss_off, nef_poly);
        ss_off.clear();
        return nef_poly;
    };

    const auto NefPoly_to_Poly = [=]( const Nef_polyhedron &nef_poly ) -> Polyhedron {
        Polyhedron poly;

//        if(!nef_poly.is_simple()){
//            throw std::invalid_argument("Transformed mesh is not simple. Unable to convert to polyhedron. Cannot continue.");
//        }
//        nef_poly.convert_to_polyhedron( poly );

        const bool triangulate_all_faces = true;
        CGAL::convert_nef_polyhedron_to_polygon_mesh(nef_poly, poly, triangulate_all_faces);
        return poly;
    };


    Nef_polyhedron orig = Poly_to_NefPoly(output_mesh);
    Nef_polyhedron amal(orig); // Amalgamated mesh -- the working mesh that is repeatedly Booleaned.

    //For a variety of orientations, translate a copy of the original mesh and boolean with the amalgamated mesh.
    std::list< vec3<double> > dUs {
        vec3<double>( 1.0, 0.0, 0.0 ).unit(),
        vec3<double>( 0.0, 1.0, 0.0 ).unit(),
        vec3<double>( 0.0, 0.0, 1.0 ).unit(),

        vec3<double>(-1.0, 0.0, 0.0 ).unit(),
        vec3<double>( 0.0,-1.0, 0.0 ).unit(),
        vec3<double>( 0.0, 0.0,-1.0 ).unit(),

        vec3<double>( 1.0, 1.0, 0.0 ).unit(),
        vec3<double>( 0.0, 1.0, 1.0 ).unit(),
        vec3<double>( 1.0, 0.0, 1.0 ).unit(),

        vec3<double>(-1.0, 1.0, 0.0 ).unit(),
        vec3<double>( 0.0,-1.0, 1.0 ).unit(),
        vec3<double>(-1.0, 0.0, 1.0 ).unit(),

        vec3<double>( 1.0,-1.0, 0.0 ).unit(),
        vec3<double>( 0.0, 1.0,-1.0 ).unit(),
        vec3<double>( 1.0, 0.0,-1.0 ).unit(),

        vec3<double>(-1.0,-1.0, 0.0 ).unit(),
        vec3<double>( 0.0,-1.0,-1.0 ).unit(),
        vec3<double>(-1.0, 0.0,-1.0 ).unit() };

    for(const auto& dU : dUs){
        auto u = dU * std::abs(distance);
        
        auto cgal_u = Vector_3(u.x, u.y, u.z);
        Nef_polyhedron::Aff_transformation_3 trans(CGAL::TRANSLATION, cgal_u);;

        Nef_polyhedron shifted(orig);
        shifted.transform(trans);
        
        YLOGINFO("Performing Boolean operation round now");
        if(op == TransformOp::Dilate){
            amal += shifted;
        }else if(op == TransformOp::Erode){
            amal ^= shifted;  // Symmetric difference.
        }else if(op == TransformOp::Shell){ 
            amal ^= shifted;
        }

        YLOGINFO("Amalgam mesh currently has " << amal.number_of_vertices() << " vertices");

/*
        // Simplify the mesh to reduce the complexity of future Boolean operations.
        YLOGINFO("About to simplify surface mesh");
        output_mesh = NefPoly_to_Poly(amal);
        Simplify(output_mesh, N_edges * 2);
        amal = Poly_to_NefPoly(output_mesh);
*/        
    }

    Nef_polyhedron result(amal);
    if(op == TransformOp::Shell) result = orig - amal;

    YLOGINFO("About to simplify surface mesh final time");
    output_mesh = NefPoly_to_Poly(result);
    Simplify(output_mesh, N_edges);

    return;
}

// This routine returns contours generated by slicing a mesh along the given planes.
contour_collection<double> Slice_Polyhedron(
        const Polyhedron &mesh,
        std::list<plane<double>> planes ){

    using Plane_3 = Kernel::Plane_3;
    using Polyline = std::vector<Kernel::Point_3>;
    using Polylines = std::list<Polyline>;

    CGAL::Polygon_mesh_slicer<Polyhedron, Kernel> slicer(mesh); 
    contour_collection<double> cc;

    for(auto &aplane : planes){
        const auto a = aplane.N_0.x;
        const auto b = aplane.N_0.y;
        const auto c = aplane.N_0.z;
        const auto d = -1.0 * (aplane.N_0.Dot(aplane.R_0));
        Plane_3 p3(a, b, c, d); // In form a*x + b*y + c*z + d = 0.

        Polylines polylines;
        slicer(p3, std::back_inserter(polylines));

        // Convert any polylines found and insert them into the contour_collection.
        if(!polylines.empty()){
            for(auto &apl : polylines){
                if(apl.size() < 3) continue; // Disregard degenerate cases.

                cc.contours.emplace_back();
                cc.contours.back().closed = true;
                for(auto &v : apl){
                    const vec3<double> p( static_cast<double>(CGAL::to_double(v.x())), 
                                          static_cast<double>(CGAL::to_double(v.y())),
                                          static_cast<double>(CGAL::to_double(v.z())) );
                    cc.contours.back().points.emplace_back(p);
                }
            }
        }
    }

/*    
    // Write the contours to a file.
    {
        std::vector<Kernel::Point_3> verts;
        std::ofstream out(Get_Unique_Sequential_Filename("/tmp/roi_verts_",3, ".xyz"));

        for(const auto &c : cc.contours){
            for(const auto &p : c.points){
                Kernel::Point_3 cgal_p( p.x, p.y, p.z );
                verts.push_back(cgal_p);
            }
        }
        if(!out || !CGAL::write_xyz_points(out, verts)){
            throw std::runtime_error("Unable to write contour vertices.");
        } 
    }
*/

    return cc;
}        


double
Volume(const Polyhedron &mesh){
    double volume = std::numeric_limits<double>::quiet_NaN();
    if(mesh.is_closed()){
        volume = static_cast<double>( CGAL::Polygon_mesh_processing::volume(mesh) );
    }
    return volume;
}

double
SurfaceArea(const Polyhedron &mesh){
    double sarea = std::numeric_limits<double>::quiet_NaN();
    if(mesh.is_closed()){
        sarea = static_cast<double>( CGAL::Polygon_mesh_processing::area(mesh) );
    }
    return sarea;
}


} // namespace polyhedron_processing
#endif // DCMA_USE_CGAL



