#include <algorithm>
#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <fstream>
#include <functional>
#include <iterator>
#include <list>
#include <queue>
#include <map>
#include <memory>
#include <set> 
#include <stdexcept>
#include <string>    
#include <random>

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathIOOBJ.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.

#include "Explicator.h"

#include "../Contour_Boolean_Operations.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"

#include "../Simple_Meshing.h"
#include "../Surface_Meshes.h"

#include "CompareMeshes.h"

OperationDoc OpArgDocCompareMeshes(){
    OperationDoc out;
    out.name = "CompareMeshes";

    out.desc = 
        "This routine calculates various metrics of difference between two meshes and prints it to the terminal output."
        ;


    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection1";
    out.args.back().default_val = "#-0";
    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection2";
    out.args.back().default_val = "#-1";

    return out;
}

std::vector<std::set<uint64_t>> get_face_edges(const std::vector<uint64_t> &face) {
    std::vector<std::set<uint64_t>> edges({
        std::set<uint64_t>({face[0], face[1]}),
        std::set<uint64_t>({face[1], face[2]}),
        std::set<uint64_t>({face[2], face[0]})
    });

    return edges;
}

// will consolidate different indices that point to same vertex
// will not modify mesh
std::vector<std::vector<uint64_t>> GetCleanFaces(const std::shared_ptr<Surface_Mesh> &mesh) {
    std::map<vec3<double>, std::vector<int>> vertex_to_indices;
    int i = 0;
    auto &vertices = mesh.get()->meshes.vertices;
    for (auto &vertex : vertices) {
        vertex_to_indices[vertex].emplace_back(i);
        if (vertex_to_indices[vertex].size() > 1) YLOGINFO("Vertex " << vertex << " has " << vertex_to_indices[vertex].size());
        i++;
    }

    std::vector<std::vector<uint64_t>> new_faces;

    // loop through faces and access vertex for each one. pick first index
    for (auto &face : mesh.get()->meshes.faces) {
        std::vector<uint64_t> new_face;
        for (auto &vertex_index : face) {
            new_face.emplace_back(vertex_to_indices[vertices[vertex_index]].front());
        }
        new_faces.emplace_back(new_face);
    }

    return new_faces;
}

// returns true if mesh is edge manifold
// it is edge manifold when every edge is connected to 2 faces
// https://www.mathworks.com/help/lidar/ref/surfacemesh.isedgemanifold.html
bool IsEdgeManifold(const std::shared_ptr<Surface_Mesh> &mesh) {
    std::map<std::set<uint64_t>, int> edge_counts;
    auto mesh_faces = GetCleanFaces(mesh);
    int face_count = 0;

    face_count = 0;
    for (auto &face : mesh_faces) {
        // assumes each face has 3 vertices
        auto edges = get_face_edges(face);

        for (auto &edge : edges) {
            edge_counts[edge] += 1;
            if (edge_counts[edge] > 2) {
                return false;
            }
        }
        face_count++;
    }

    for (auto &key_value_pair : edge_counts) {
        if (key_value_pair.second != 2) {
            return false;
        }
    }

    return true;
}

// returns true if mesh if vertex manifold
// it is vertex manifold when each vertex's faces form an open or closed fan
// https://www.mathworks.com/help/lidar/ref/surfacemesh.isvertexmanifold.html
bool IsVertexManifold(const std::shared_ptr<Surface_Mesh>&mesh) {
    // store vertex to face hashmap
    // for each vertex, store edge to list of faces
    // start with a face
    // make sure two edges have adjacent faces (and add to queue)
    // search adjacent faces (only one edge now)
    // have a list of visited edges to avoid re-searching
    // keep searching until no more faces (make sure number of faces searched is equal to total faces)

    using face_type = std::vector<uint64_t>;
    auto mesh_faces = GetCleanFaces(mesh);
    std::map<uint64_t, std::vector<face_type>> vertex_to_faces;

    for (auto &mesh_face : mesh_faces) {
        vertex_to_faces[mesh_face[0]].emplace_back(mesh_face);
        vertex_to_faces[mesh_face[1]].emplace_back(mesh_face);
        vertex_to_faces[mesh_face[2]].emplace_back(mesh_face);
    }

    for (auto &vertex_faces_pair : vertex_to_faces) {
        auto vertex = vertex_faces_pair.first;
        auto faces = vertex_faces_pair.second;

        std::map<std::set<uint64_t>, std::vector<face_type>> edge_to_faces;
        for (auto &face : faces) {
            auto edges = get_face_edges(face);
            for (auto &edge : edges) edge_to_faces[edge].emplace_back(face);
        }

        std::set<face_type> visited_faces;
        std::queue<face_type> face_queue;
        face_queue.push(faces[0]);

        while (face_queue.size() != 0) {
            auto face = face_queue.front();
            face_queue.pop();
            if (visited_faces.find(face) != visited_faces.end()) continue; // already visited
            visited_faces.emplace(face);
            auto edges = get_face_edges(face);
            for (auto &edge : edges) {
                auto adjacent_faces = edge_to_faces[edge];
                for (auto &adj_face : adjacent_faces) face_queue.push(adj_face);
            }
        }

        if (visited_faces.size() != faces.size()) return false;
    }
    return true;
}

bool CompareMeshes(Drover &DICOM_data,
                               const OperationArgPkg& OptArgs,
                               std::map<std::string, std::string>& /*InvocationMetadata*/,
                               const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelection1Str = OptArgs.getValueStr("MeshSelection1").value();
    const auto MeshSelection2Str = OptArgs.getValueStr("MeshSelection2").value();

    //-----------------------------------------------------------------------------------------------------------------
    auto SMs_all = All_SMs( DICOM_data );
    auto SMs1 = Whitelist( SMs_all, MeshSelection1Str );
    auto SMs2 = Whitelist( SMs_all, MeshSelection2Str );


    if (SMs1.size() < 1 || SMs2.size() < 1) {
        FUNCERR("Must select at least two meshes.");
    }
    if (SMs1.size() > 1 || SMs2.size() > 1) {
        FUNCWARN("Can only calculate the metrics of two meshes, only looking at last element of each selected");
    }
    
    std::shared_ptr<Surface_Mesh> mesh1 = *SMs1.front();
    std::shared_ptr<Surface_Mesh> mesh2 = *SMs2.front();

    double max_distance = 0;

    FUNCINFO("Iterating through " << size(mesh1->meshes.vertices) << " and " << size(mesh2->meshes.vertices) << " vertices.");

    // O(n^2) time, might be way to speed this up if this is ever a bottleneck
    for (auto& vertex : mesh1->meshes.vertices) {
        // find closest vertex on mesh2 that corresponds to vertex
        double min_distance = -1;
        for (auto& vertex2 : mesh2->meshes.vertices) {
            double distance = vertex.distance(vertex2);
            if (min_distance == -1) {
                min_distance = distance;
            } else {
                min_distance = std::min(distance, min_distance);
            }
        }
        max_distance = std::max(min_distance, max_distance);
    }

    double second_max_distance = 0;

    for (auto& vertex : mesh2->meshes.vertices) {
        // find closest vertex on mesh2 that corresponds to vertex
        double min_distance = -1;
        for (auto& vertex2 : mesh1->meshes.vertices) {
            double distance = vertex.distance(vertex2);
            if (min_distance == -1) {
                min_distance = distance;
            } else {
                min_distance = std::min(distance, min_distance);
            }
        }
        second_max_distance = std::max(min_distance, second_max_distance);
    }

    // Calculate centroids by finding the average of all the vertices in the surface mesh
    // Assumes that contours vertices are distributed evenly on the surface mesh which is not strictly true.
    double sumx = 0, sumy = 0, sumz = 0;
    for (auto& vertex : mesh1->meshes.vertices) {
        sumx += vertex.x;
        sumy += vertex.y;
        sumz += vertex.z;
    }
    vec3 centroid1 = vec3(
        sumx/size(mesh1->meshes.vertices),
        sumy/size(mesh1->meshes.vertices),
        sumz/size(mesh1->meshes.vertices)
    );

    sumx = 0, sumy= 0, sumz = 0;
    for (auto& vertex : mesh2->meshes.vertices) {
        sumx += vertex.x;
        sumy += vertex.y;
        sumz += vertex.z;
    }
    vec3 centroid2 = vec3(sumx/size(mesh1->meshes.vertices) , sumy/size(mesh1->meshes.vertices) , sumz/size(mesh1->meshes.vertices));

    const double centroid_shift = sqrt(pow((centroid2.x-centroid1.x),2) + pow((centroid2.y-centroid1.y),2) + pow((centroid2.y-centroid1.y),2));
    
    // Converting to a triangular mesh to ensure that each face is made up of 3 vertices for volume calculation
    // Total volume is calculated by summing the signed volme of the triganular prism made by each face and the origin as the apex.
    // This method returns a finite volume even if the mesh is open, so watertightness needs to be checked separately.

    mesh1->meshes.convert_to_triangles();
    mesh2->meshes.convert_to_triangles();


    double volume1 = 0;
    double volume2 = 0;

    decltype(mesh1->meshes.faces)* faces = &(mesh1->meshes.faces);
    for (auto& fv : *faces){

        const auto P_A = mesh1->meshes.vertices.at( fv[0] );
        const auto P_B = mesh1->meshes.vertices.at( fv[1] );
        const auto P_C = mesh1->meshes.vertices.at( fv[2] );

        volume1 += (-P_C.x*P_B.y*P_A.z + P_B.x*P_C.y*P_A.z + 
                P_C.x*P_A.y*P_B.z - P_A.x*P_C.y*P_B.z - 
                P_B.x*P_A.y*P_C.z + P_A.x*P_B.y*P_C.z)/(6.0);

    }


    faces = &(mesh2->meshes.faces);
    for (auto& fv : *faces){

        const auto P_A = mesh2->meshes.vertices.at( fv[0] );
        const auto P_B = mesh2->meshes.vertices.at( fv[1] );
        const auto P_C = mesh2->meshes.vertices.at( fv[2] );

        volume2 += (-P_C.x*P_B.y*P_A.z + P_B.x*P_C.y*P_A.z +
                P_C.x*P_A.y*P_B.z - P_A.x*P_C.y*P_B.z - 
                P_B.x*P_A.y*P_C.z + P_A.x*P_B.y*P_C.z)/(6.0);
    }

    bool v_manifold_1 = IsVertexManifold(mesh1);
    bool v_manifold_2 = IsVertexManifold(mesh2);
    bool e_manifold_1 = IsEdgeManifold(mesh1);
    bool e_manifold_2 = IsEdgeManifold(mesh2);
    FUNCINFO("Vertex manifoldness (first vs. second): " << v_manifold_1 << " and " << v_manifold_2);
    FUNCINFO("Edge manifoldness (first vs. second): " << e_manifold_1 << " and " << e_manifold_2);

    bool manifold1 = v_manifold_1 && e_manifold_1;
    bool manifold2 = v_manifold_2 && e_manifold_2;

    FUNCINFO("HAUSDORFF DISTANCE: " << max_distance << " or " << second_max_distance);
    FUNCINFO("SURFACE AREA: First mesh = " << mesh1->meshes.surface_area() << ", second mesh = " << mesh2->meshes.surface_area());
    FUNCINFO("SURFACE AREA (%) difference: " << (mesh1->meshes.surface_area() - mesh2->meshes.surface_area())*100/mesh1->meshes.surface_area());
    FUNCINFO("VOLUME: First mesh = " <<abs(volume1) << ", second mesh = " << abs(volume2));
    FUNCINFO("VOLUME (%) difference: " << (abs(abs(volume1)-abs(volume2)))*100/abs(volume1));
    FUNCINFO("CENTROID: First mesh = " << centroid1.x << "," << centroid1.y << "," << centroid1.z);
    FUNCINFO("CENTROID: Second mesh = " << centroid2.x << "," << centroid2.y << "," << centroid2.z);
    FUNCINFO("Centroid Shift = " << centroid_shift);
    FUNCINFO("MANIFOLD: First mesh = " << manifold1 << ", second mesh = " << manifold2);
    return true;
}
