//Surface_Meshes.h - A part of DICOMautomaton 2017, 2024. Written by hal clark.
//
// CGAL has been removed - all mesh processing now uses native implementations.

#pragma once

#include <string>
#include <utility>
#include <cstdint>

#include "CSG_SDF.h"

namespace dcma_surface_meshes {

    typedef enum {
            Fast,
            Medium,
            High
    } ReproductionQuality;

    struct Parameters {
        int64_t NumberOfImages = -1; // If not sensible, defaults to ~number of unique contour planes.

        int64_t GridRows = 512;  // As of writing, the procedure limiting ramping this up is inclusivity testing.
        int64_t GridColumns = 512;

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

    fv_surface_mesh<double, uint64_t>
    Estimate_Surface_Mesh_Marching_Cubes(
            const std::list<std::reference_wrapper<contour_collection<double>>>& cc_ROIs,
            Parameters p );

    fv_surface_mesh<double, uint64_t>
    Estimate_Surface_Mesh_Marching_Cubes(
            std::shared_ptr<csg::sdf::node> sdf,
            const vec3<double> &minimum_resolution,
            double inclusion_threshold, // The voxel value threshold demarcating surface 'interior' and 'exterior.'
            bool below_is_interior,  // Controls how the inclusion_threshold is interpretted.
                                     // If true, anything <= is considered to be interior to the surface.
                                     // If false, anything >= is considered to be interior to the surface.
            Parameters p );

    fv_surface_mesh<double, uint64_t>
    Estimate_Surface_Mesh_Marching_Cubes(
            const std::list<std::reference_wrapper<planar_image<float,double>>>& grid_imgs,
            double inclusion_threshold, // The voxel value threshold demarcating surface 'interior' and 'exterior.'
            bool below_is_interior,  // Controls how the inclusion_threshold is interpretted.
                                     // If true, anything <= is considered to be interior to the surface.
                                     // If false, anything >= is considered to be interior to the surface.
            Parameters p );

    // ============== Native mesh processing functions ==============
    
    // Compute the volume of a closed mesh using the divergence theorem.
    double
    Mesh_Volume(const fv_surface_mesh<double, uint64_t>& mesh);

    // Compute the surface area of a mesh by summing triangle areas.
    double
    Mesh_Surface_Area(const fv_surface_mesh<double, uint64_t>& mesh);

    // Slice a mesh with planes to produce contours.
    contour_collection<double>
    Slice_Mesh(const fv_surface_mesh<double, uint64_t>& mesh,
               const std::list<plane<double>>& planes);

    // Native mesh subdivision for triangle meshes.
    // This is a simplified subdivision that adds edge midpoints without smoothing.
    void
    Loop_Subdivide(fv_surface_mesh<double, uint64_t>& mesh, int64_t iterations);

} // namespace dcma_surface_meshes

