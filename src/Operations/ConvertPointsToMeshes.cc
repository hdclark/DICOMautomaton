// ConvertPointsToMeshes.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>
#include <cstdint>

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMeshesConvexHull.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../Metadata.h"

#include "ConvertPointsToMeshes.h"


OperationDoc OpArgDocConvertPointsToMeshes(){
    OperationDoc out;
    out.name = "ConvertPointsToMeshes";

    out.tags.emplace_back("category: point cloud processing");
    out.tags.emplace_back("category: mesh processing");

    out.desc = 
        "This operation converts point clouds to a surface mesh. There are four supported methods:"
        " 'expand' -- represents each point as a small axis-aligned cube centered at the point location;"
        " 'convexhull' -- computes the convex hull of all points using the legacy Convex_Hull_3 template;"
        " 'ygor-convexhull' -- computes the convex hull of all points using Ygor's incremental IncrementalConvexHull class;"
        " 'ygor-divide-and-conquer-convexhull' -- computes the convex hull of all points using Ygor's DivideAndConquerConvexHull class.";
        
    out.notes.emplace_back(
        "Point clouds are unaltered. Existing surface meshes are ignored and unaltered."
    );
    out.notes.emplace_back(
        "The 'expand' method creates a mesh with multiple disjoint surfaces (one cube per point)"
        " combined into a single Surface_Mesh object. Meshes may overlap with one another."
    );
    out.notes.emplace_back(
        "The 'convexhull', 'ygor-convexhull', and 'ygor-divide-and-conquer-convexhull' methods create a single manifold surface mesh that encloses all points."
        " These methods only work for point sets with non-zero volume and will produce a convex shape."
    );


    out.args.emplace_back();
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "PointSelection";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back().name = "Method";
    out.args.back().desc = "There are currently four supported methods:"
                           "\n\n"
                           "Method 'expand' replaces each point with an axis-aligned cube."
                           " This method is most useful for visualizing a point cloud."
                           "\n\n"
                           "Method 'convexhull' computes the convex hull of all selected points"
                           " using the legacy Convex_Hull_3 template."
                           "\n\n"
                           "Method 'ygor-convexhull' computes the convex hull of all selected points"
                           " using Ygor's incremental IncrementalConvexHull class."
                           " This is useful for creating a minimal bounding surface."
                           "\n\n"
                           "Method 'ygor-divide-and-conquer-convexhull' computes the convex hull of all selected points"
                           " using Ygor's DivideAndConquerConvexHull class."
                           " This is useful for creating a minimal bounding surface.";
    out.args.back().default_val = "expand";
    out.args.back().expected = true;
    out.args.back().examples = { "expand", "convexhull", "ygor-convexhull", "ygor-divide-and-conquer-convexhull" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back().name = "CubeWidth";
    out.args.back().desc = "The width (side length) of each cube representing a point."
                           " All cubes are axis-aligned and centered at the point location."
                           " This parameter is only used with the 'expand' method.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.01", "0.1", "1.0", "2.0", "50.0" };

    return out;
}



bool ConvertPointsToMeshes(Drover &DICOM_data,
                           const OperationArgPkg& OptArgs,
                           std::map<std::string, std::string>& /*InvocationMetadata*/,
                           const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto PointSelectionStr = OptArgs.getValueStr("PointSelection").value();
    const auto MethodStr = OptArgs.getValueStr("Method").value();
    const auto CubeWidth = std::stod( OptArgs.getValueStr("CubeWidth").value() );
    //-----------------------------------------------------------------------------------------------------------------

    const auto expand_regex = Compile_Regex("^ex?p?a?n?d?$");
    const auto convexhull_regex = Compile_Regex("^co?n?v?e?x?[-_]?hu?l?l?$");
    const auto ygor_convexhull_regex = Compile_Regex("^yg?o?r?[-_]?co?n?v?e?x?[-_]?hu?l?l?$");
    const auto ygor_dnc_convexhull_regex = Compile_Regex("^yg?o?r?[-_]?di?v?i?d?e?[-_]?a?n?d?[-_]?c?o?n?q?u?e?r?[-_]?co?n?v?e?x?[-_]?hu?l?l?$");

    auto PCs_all = All_PCs( DICOM_data );
    auto PCs = Whitelist( PCs_all, PointSelectionStr );
    const auto pc_count = PCs.size();
    YLOGINFO("Selected " << pc_count << " point clouds");

    if(pc_count == 0){
        throw std::invalid_argument("No point clouds selected. Cannot continue.");
    }

    // Construct a destination for the surface mesh.
    DICOM_data.smesh_data.emplace_back( std::make_shared<Surface_Mesh>() );

    // Gather metadata from all selected point clouds.
    std::vector<metadata_map_t> pc_metadata;
    for(auto & pcp_it : PCs){
        pc_metadata.push_back( (*pcp_it)->pset.metadata );
    }

    // Coalesce the metadata from all point clouds.
    metadata_map_t combined_metadata;
    for(const auto &md : pc_metadata){
        for(const auto &kv : md){
            combined_metadata[kv.first] = kv.second;
        }
    }

    // Apply the standard mesh metadata coalescing.
    DICOM_data.smesh_data.back()->meshes.metadata = coalesce_metadata_for_basic_mesh(combined_metadata, meta_evolve::iterate);
    DICOM_data.smesh_data.back()->meshes.metadata["Description"] = "Surface mesh derived from point clouds.";

    // -------------------------- Expand (cube) method --------------------------
    if(false){
    }else if(std::regex_match(MethodStr, expand_regex)){
        if(CubeWidth <= 0.0){
            throw std::invalid_argument("CubeWidth must be positive. Cannot continue.");
        }

        // Process each point cloud.
        int64_t completed = 0;
        int64_t total_points = 0;
        
        for(auto & pcp_it : PCs){
            const auto &points = (*pcp_it)->pset.points;
            total_points += points.size();

            // For each point, create a cube centered at the point.
            const double half_width = CubeWidth * 0.5;
            
            for(const auto &p : points){
                // Define the 8 vertices of the cube centered at point p.
                const size_t base_vertex_idx = DICOM_data.smesh_data.back()->meshes.vertices.size();
                
                // The 8 corners of an axis-aligned cube.
                DICOM_data.smesh_data.back()->meshes.vertices.emplace_back(
                    vec3<double>(p.x - half_width, p.y - half_width, p.z - half_width));
                DICOM_data.smesh_data.back()->meshes.vertices.emplace_back(
                    vec3<double>(p.x + half_width, p.y - half_width, p.z - half_width));
                DICOM_data.smesh_data.back()->meshes.vertices.emplace_back(
                    vec3<double>(p.x + half_width, p.y + half_width, p.z - half_width));
                DICOM_data.smesh_data.back()->meshes.vertices.emplace_back(
                    vec3<double>(p.x - half_width, p.y + half_width, p.z - half_width));
                DICOM_data.smesh_data.back()->meshes.vertices.emplace_back(
                    vec3<double>(p.x - half_width, p.y - half_width, p.z + half_width));
                DICOM_data.smesh_data.back()->meshes.vertices.emplace_back(
                    vec3<double>(p.x + half_width, p.y - half_width, p.z + half_width));
                DICOM_data.smesh_data.back()->meshes.vertices.emplace_back(
                    vec3<double>(p.x + half_width, p.y + half_width, p.z + half_width));
                DICOM_data.smesh_data.back()->meshes.vertices.emplace_back(
                    vec3<double>(p.x - half_width, p.y + half_width, p.z + half_width));
                
                // Create the 12 triangular faces (2 per cube face, 6 faces total).
                // Bottom face (z-)
                DICOM_data.smesh_data.back()->meshes.faces.emplace_back(
                    std::vector<uint64_t>{{base_vertex_idx + 0, base_vertex_idx + 1, base_vertex_idx + 2}});
                DICOM_data.smesh_data.back()->meshes.faces.emplace_back(
                    std::vector<uint64_t>{{base_vertex_idx + 0, base_vertex_idx + 2, base_vertex_idx + 3}});
                
                // Top face (z+)
                DICOM_data.smesh_data.back()->meshes.faces.emplace_back(
                    std::vector<uint64_t>{{base_vertex_idx + 4, base_vertex_idx + 6, base_vertex_idx + 5}});
                DICOM_data.smesh_data.back()->meshes.faces.emplace_back(
                    std::vector<uint64_t>{{base_vertex_idx + 4, base_vertex_idx + 7, base_vertex_idx + 6}});
                
                // Front face (y-)
                DICOM_data.smesh_data.back()->meshes.faces.emplace_back(
                    std::vector<uint64_t>{{base_vertex_idx + 0, base_vertex_idx + 4, base_vertex_idx + 5}});
                DICOM_data.smesh_data.back()->meshes.faces.emplace_back(
                    std::vector<uint64_t>{{base_vertex_idx + 0, base_vertex_idx + 5, base_vertex_idx + 1}});
                
                // Back face (y+)
                DICOM_data.smesh_data.back()->meshes.faces.emplace_back(
                    std::vector<uint64_t>{{base_vertex_idx + 2, base_vertex_idx + 6, base_vertex_idx + 7}});
                DICOM_data.smesh_data.back()->meshes.faces.emplace_back(
                    std::vector<uint64_t>{{base_vertex_idx + 2, base_vertex_idx + 7, base_vertex_idx + 3}});
                
                // Left face (x-)
                DICOM_data.smesh_data.back()->meshes.faces.emplace_back(
                    std::vector<uint64_t>{{base_vertex_idx + 0, base_vertex_idx + 3, base_vertex_idx + 7}});
                DICOM_data.smesh_data.back()->meshes.faces.emplace_back(
                    std::vector<uint64_t>{{base_vertex_idx + 0, base_vertex_idx + 7, base_vertex_idx + 4}});
                
                // Right face (x+)
                DICOM_data.smesh_data.back()->meshes.faces.emplace_back(
                    std::vector<uint64_t>{{base_vertex_idx + 1, base_vertex_idx + 5, base_vertex_idx + 6}});
                DICOM_data.smesh_data.back()->meshes.faces.emplace_back(
                    std::vector<uint64_t>{{base_vertex_idx + 1, base_vertex_idx + 6, base_vertex_idx + 2}});
            }

            ++completed;
            YLOGINFO("Completed " << completed << " of " << pc_count
                  << " --> " << static_cast<int>(1000.0*(completed)/pc_count)/10.0 << "% done");
        }

        // Recreate the involved face index for efficient queries.
        DICOM_data.smesh_data.back()->meshes.recreate_involved_face_index();

        YLOGINFO("Created surface mesh with " << total_points << " cubes ("
                 << DICOM_data.smesh_data.back()->meshes.vertices.size() << " vertices and "
                 << DICOM_data.smesh_data.back()->meshes.faces.size() << " faces)");

    // -------------------------- Convex hull method --------------------------
    }else if(std::regex_match(MethodStr, convexhull_regex)){
        // Gather all vertices from all selected point clouds.
        for(auto & pcp_it : PCs){
            for(const auto &p : (*pcp_it)->pset.points){
                DICOM_data.smesh_data.back()->meshes.vertices.emplace_back(p);
            }
        }

        YLOGINFO("Generating convex hull from " << DICOM_data.smesh_data.back()->meshes.vertices.size() << " vertices");

        // Construct the convex hull.
        using vert_vec_t = decltype(std::begin(DICOM_data.smesh_data.back()->meshes.vertices));
        DICOM_data.smesh_data.back()->meshes.faces = Convex_Hull_3<vert_vec_t,uint64_t>(
            std::begin(DICOM_data.smesh_data.back()->meshes.vertices),
            std::end(DICOM_data.smesh_data.back()->meshes.vertices) );

        // Prune unneeded vertices.
        DICOM_data.smesh_data.back()->meshes.remove_disconnected_vertices();

        // Rebuild adjacency/index metadata to keep behavior consistent with the expand method.
        DICOM_data.smesh_data.back()->meshes.recreate_involved_face_index();
        YLOGINFO("Created convex hull with "
                 << DICOM_data.smesh_data.back()->meshes.vertices.size() << " vertices and "
                 << DICOM_data.smesh_data.back()->meshes.faces.size() << " faces");

    // -------------------------- Ygor convex hull method --------------------------
    }else if(std::regex_match(MethodStr, ygor_convexhull_regex)){
        // Gather all vertices from all selected point clouds.
        std::vector<vec3<double>> all_verts;
        for(auto & pcp_it : PCs){
            for(const auto &p : (*pcp_it)->pset.points){
                all_verts.emplace_back(p);
            }
        }

        YLOGINFO("Generating Ygor incremental convex hull from " << all_verts.size() << " vertices");

        // Compute the convex hull using Ygor's IncrementalConvexHull class.
        IncrementalConvexHull<double> ch;
        ch.add_vertices(all_verts);

        // Retrieve the hull mesh, but preserve any existing metadata/state on the target mesh
        // by only updating its core geometry (vertices and faces).
        auto hull_mesh = ch.get_mesh();
        auto & target_mesh = DICOM_data.smesh_data.back()->meshes;
        target_mesh.vertices = std::move(hull_mesh.vertices);
        target_mesh.faces    = std::move(hull_mesh.faces);

        // Rebuild adjacency/index metadata to keep behavior consistent with the other methods.
        target_mesh.recreate_involved_face_index();

        YLOGINFO("Created Ygor incremental convex hull with "
                 << target_mesh.vertices.size() << " vertices and "
                 << target_mesh.faces.size() << " faces");

    // -------------------------- Ygor divide-and-conquer convex hull method --------------------------
    }else if(std::regex_match(MethodStr, ygor_dnc_convexhull_regex)){
        // Gather all vertices from all selected point clouds.
        std::vector<vec3<double>> all_verts;
        for(auto & pcp_it : PCs){
            for(const auto &p : (*pcp_it)->pset.points){
                all_verts.emplace_back(p);
            }
        }

        YLOGINFO("Generating Ygor divide-and-conquer convex hull from " << all_verts.size() << " vertices");

        // Compute the convex hull using Ygor's DivideAndConquerConvexHull class.
        DivideAndConquerConvexHull<double> ch;
        ch.add_vertices(all_verts);

        // Retrieve the hull mesh, but preserve any existing metadata/state on the target mesh
        // by only updating its core geometry (vertices and faces).
        auto hull_mesh = ch.get_mesh();
        auto & target_mesh = DICOM_data.smesh_data.back()->meshes;
        target_mesh.vertices = std::move(hull_mesh.vertices);
        target_mesh.faces    = std::move(hull_mesh.faces);

        // Rebuild adjacency/index metadata to keep behavior consistent with the other methods.
        target_mesh.recreate_involved_face_index();

        YLOGINFO("Created Ygor divide-and-conquer convex hull with "
                 << target_mesh.vertices.size() << " vertices and "
                 << target_mesh.faces.size() << " faces");

    }else{
        throw std::invalid_argument("Unrecognized method '" + MethodStr + "'. Cannot continue.");
    }

    return true;
}
