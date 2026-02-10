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

// CGAL has been removed - all mesh processing now uses native implementations

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
        const auto img_unit = img_refw.get().ortho_unit();

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

    const auto N_cols = static_cast<int64_t>(std::ceil((bb.max.x - bb.min.x)/min_res_x));
    const auto N_rows = static_cast<int64_t>(std::ceil((bb.max.y - bb.min.y)/min_res_y));
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




// ======================================== Native Mesh Processing ========================================
// These functions provide native alternatives for common mesh operations.

// Compute the volume of a closed mesh using the divergence theorem.
// The mesh should be closed and consistently oriented for correct results.
// Returns the absolute value of the signed volume, so the result is always positive
// regardless of face winding order.
double
Mesh_Volume(const fv_surface_mesh<double, uint64_t>& mesh){
    // Use the divergence theorem: V = (1/6) * sum over all triangles of (v0 · (v1 × v2))
    // This computes the signed volume of the tetrahedra formed by each triangle and the origin.
    // We return abs() to handle meshes with inconsistent winding order and to always give
    // a positive volume for practical use.
    double volume = 0.0;
    for(const auto& face : mesh.faces){
        if(face.size() < 3) continue;
        
        const auto& v0 = mesh.vertices.at(face[0]);
        const auto& v1 = mesh.vertices.at(face[1]);
        const auto& v2 = mesh.vertices.at(face[2]);
        
        // Signed volume of tetrahedron with one vertex at origin
        volume += v0.Dot(v1.Cross(v2));
    }
    return std::abs(volume) / 6.0;
}

// Compute the surface area of a mesh by summing triangle areas.
double
Mesh_Surface_Area(const fv_surface_mesh<double, uint64_t>& mesh){
    double area = 0.0;
    for(const auto& face : mesh.faces){
        if(face.size() < 3) continue;
        
        const auto& v0 = mesh.vertices.at(face[0]);
        const auto& v1 = mesh.vertices.at(face[1]);
        const auto& v2 = mesh.vertices.at(face[2]);
        
        // Area of triangle = 0.5 * ||(v1-v0) × (v2-v0)||
        const auto edge1 = v1 - v0;
        const auto edge2 = v2 - v0;
        area += edge1.Cross(edge2).length() * 0.5;
    }
    return area;
}

// Slice a mesh with a plane to produce contours.
// Returns a contour_collection containing all intersection contours.
contour_collection<double>
Slice_Mesh(const fv_surface_mesh<double, uint64_t>& mesh,
           const std::list<plane<double>>& planes){
    
    contour_collection<double> cc;
    
    for(const auto& aplane : planes){
        // For each plane, find all edge-plane intersections
        // Build contours by connecting intersection points
        
        // Store edge intersections as pairs of (intersection_point, edge_id)
        // where edge_id = (min_vertex_index, max_vertex_index)
        std::map<std::pair<uint64_t, uint64_t>, vec3<double>> edge_intersections;
        
        // Map from face index to its intersection points
        std::map<uint64_t, std::vector<std::pair<std::pair<uint64_t, uint64_t>, vec3<double>>>> face_edge_intersections;
        
        const double plane_eps = 1e-10; // Tolerance for determining if vertex is on the plane
        
        for(uint64_t fi = 0; fi < mesh.faces.size(); ++fi){
            const auto& face = mesh.faces[fi];
            if(face.size() < 3) continue;
            
            std::vector<std::pair<std::pair<uint64_t, uint64_t>, vec3<double>>> local_intersections;
            
            for(size_t i = 0; i < face.size(); ++i){
                const auto v0_idx = face[i];
                const auto v1_idx = face[(i + 1) % face.size()];
                
                const auto& v0 = mesh.vertices.at(v0_idx);
                const auto& v1 = mesh.vertices.at(v1_idx);
                
                const double d0 = aplane.Get_Signed_Distance_To_Point(v0);
                const double d1 = aplane.Get_Signed_Distance_To_Point(v1);
                
                // Handle case where vertex lies exactly on the plane
                const bool v0_on_plane = std::abs(d0) < plane_eps;
                const bool v1_on_plane = std::abs(d1) < plane_eps;
                
                // Create canonical edge identifier (smaller index first)
                auto edge_id = std::make_pair(std::min(v0_idx, v1_idx), std::max(v0_idx, v1_idx));
                
                if(v0_on_plane && v1_on_plane){
                    // Both vertices on plane - skip this edge as it lies in the plane
                    continue;
                }else if(v0_on_plane){
                    // First vertex is on the plane - use it as intersection
                    edge_intersections[edge_id] = v0;
                    local_intersections.push_back({edge_id, v0});
                }else if(v1_on_plane){
                    // Second vertex is on the plane - use it as intersection
                    edge_intersections[edge_id] = v1;
                    local_intersections.push_back({edge_id, v1});
                }else if((d0 > 0.0 && d1 < 0.0) || (d0 < 0.0 && d1 > 0.0)){
                    // Edge crosses the plane (different signs)
                    // Compute intersection point via linear interpolation
                    const double t = d0 / (d0 - d1);
                    const auto intersection = v0 + (v1 - v0) * t;
                    
                    edge_intersections[edge_id] = intersection;
                    local_intersections.push_back({edge_id, intersection});
                }
            }
            
            if(local_intersections.size() == 2){
                face_edge_intersections[fi] = local_intersections;
            }
        }
        
        if(edge_intersections.empty()) continue;
        
        // Build contours by following edge intersections through faces
        std::set<uint64_t> visited_faces;
        
        for(const auto& [start_face, start_intersections] : face_edge_intersections){
            if(visited_faces.count(start_face) > 0) continue;
            
            contour_of_points<double> contour;
            contour.closed = true;
            
            // Start a new contour
            uint64_t current_face = start_face;
            auto current_edge = start_intersections[0].first;
            auto current_pt = start_intersections[0].second;
            
            contour.points.push_back(current_pt);
            
            // Walk through connected faces
            size_t max_iterations = mesh.faces.size() + 1;
            size_t iter = 0;
            
            while(iter < max_iterations){
                ++iter;
                visited_faces.insert(current_face);
                
                const auto& intersections = face_edge_intersections.at(current_face);
                
                // Find the other edge in this face
                std::pair<uint64_t, uint64_t> next_edge;
                vec3<double> next_pt;
                bool found = false;
                
                for(const auto& [edge_id, pt] : intersections){
                    if(edge_id != current_edge){
                        next_edge = edge_id;
                        next_pt = pt;
                        found = true;
                        break;
                    }
                }
                
                if(!found) break;
                
                contour.points.push_back(next_pt);
                
                // Find the next face that shares this edge
                uint64_t next_face = static_cast<uint64_t>(-1);
                for(const auto& [fi, intersects] : face_edge_intersections){
                    if(fi == current_face) continue;
                    if(visited_faces.count(fi) > 0) continue;
                    
                    for(const auto& [edge_id, pt] : intersects){
                        if(edge_id == next_edge){
                            next_face = fi;
                            break;
                        }
                    }
                    if(next_face != static_cast<uint64_t>(-1)) break;
                }
                
                if(next_face == static_cast<uint64_t>(-1)){
                    // Check if we've completed a loop back to start
                    break;
                }
                
                current_face = next_face;
                current_edge = next_edge;
            }
            
            // Only add contours with enough points
            if(contour.points.size() >= 3){
                cc.contours.push_back(contour);
            }
        }
    }
    
    return cc;
}

// Native mesh subdivision for triangle meshes.
// This is a simplified subdivision that splits each triangle into 4 triangles by adding 
// edge midpoints. Unlike full Loop subdivision, this does not apply weighted smoothing
// to vertex positions, so it only refines the mesh geometry without smoothing.
// For many practical applications (e.g., increasing mesh resolution for slicing),
// this simpler approach is sufficient.
// Each iteration quadruples the number of faces.
void
Loop_Subdivide(fv_surface_mesh<double, uint64_t>& mesh, int64_t iterations){
    for(int64_t iter = 0; iter < iterations; ++iter){
        const auto N_orig_verts = mesh.vertices.size();
        const auto N_orig_faces = mesh.faces.size();
        
        // Map from edge (pair of vertex indices) to new midpoint vertex index
        std::map<std::pair<uint64_t, uint64_t>, uint64_t> edge_to_new_vert;
        
        // First pass: create edge midpoints
        for(const auto& face : mesh.faces){
            if(face.size() != 3) continue;
            
            for(size_t i = 0; i < 3; ++i){
                const auto v0_idx = face[i];
                const auto v1_idx = face[(i + 1) % 3];
                
                auto edge = std::make_pair(std::min(v0_idx, v1_idx), std::max(v0_idx, v1_idx));
                
                if(edge_to_new_vert.find(edge) == edge_to_new_vert.end()){
                    // Create new midpoint vertex (simple midpoint averaging)
                    const auto& v0 = mesh.vertices.at(v0_idx);
                    const auto& v1 = mesh.vertices.at(v1_idx);
                    const auto midpoint = (v0 + v1) * 0.5;
                    
                    const auto new_idx = static_cast<uint64_t>(mesh.vertices.size());
                    mesh.vertices.push_back(midpoint);
                    edge_to_new_vert[edge] = new_idx;
                }
            }
        }
        
        // Second pass: create new faces (each triangle becomes 4 triangles)
        std::vector<std::array<uint64_t, 3>> new_faces;
        new_faces.reserve(N_orig_faces * 4);
        
        for(const auto& face : mesh.faces){
            if(face.size() != 3) continue;
            
            const auto v0 = face[0];
            const auto v1 = face[1];
            const auto v2 = face[2];
            
            // Get edge midpoint vertices
            auto e01 = std::make_pair(std::min(v0, v1), std::max(v0, v1));
            auto e12 = std::make_pair(std::min(v1, v2), std::max(v1, v2));
            auto e20 = std::make_pair(std::min(v2, v0), std::max(v2, v0));
            
            const auto m01 = edge_to_new_vert.at(e01);
            const auto m12 = edge_to_new_vert.at(e12);
            const auto m20 = edge_to_new_vert.at(e20);
            
            // Create 4 new triangles
            new_faces.push_back({v0, m01, m20});   // Corner triangle at v0
            new_faces.push_back({v1, m12, m01});   // Corner triangle at v1
            new_faces.push_back({v2, m20, m12});   // Corner triangle at v2
            new_faces.push_back({m01, m12, m20});  // Center triangle
        }
        
        // Replace faces
        mesh.faces.clear();
        mesh.faces.reserve(new_faces.size());
        for(const auto& f : new_faces){
            mesh.faces.push_back({f[0], f[1], f[2]});
        }
        
        // Clear and rebuild involved_faces index
        mesh.involved_faces.clear();
        
        YLOGINFO("Subdivision iteration " << (iter + 1) << ": " 
                 << mesh.vertices.size() << " vertices, "
                 << mesh.faces.size() << " faces");
    }
}

} // namespace dcma_surface_meshes.






