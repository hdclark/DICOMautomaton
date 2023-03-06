#include <algorithm>
#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <fstream>
#include <functional>
#include <iterator>
#include <list>
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
    for (auto & vertex : mesh1->meshes.vertices) {
        // find closest vertex on mesh2 that corresponds to vertex
        double min_distance = -1;
        for (auto & vertex2 : mesh2->meshes.vertices) {
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

    for (auto & vertex : mesh2->meshes.vertices) {
        // find closest vertex on mesh2 that corresponds to vertex
        double min_distance = -1;
        for (auto & vertex2 : mesh1->meshes.vertices) {
            double distance = vertex.distance(vertex2);
            if (min_distance == -1) {
                min_distance = distance;
            } else {
                min_distance = std::min(distance, min_distance);
            }
        }
        second_max_distance = std::max(min_distance, second_max_distance);
    }
    
    mesh1->meshes.convert_to_triangles();
    mesh2->meshes.convert_to_triangles();


    double volume1 = 0;
    double volume2 = 0;

    decltype(mesh1->meshes.faces)* faces = &(mesh1->meshes.faces);
    for (auto & fv : *faces){

        const auto P_A = mesh1->meshes.vertices.at( fv[0] );
        const auto P_B = mesh1->meshes.vertices.at( fv[1] );
        const auto P_C = mesh1->meshes.vertices.at( fv[2] );

        volume1 += (-P_C.x*P_B.y*P_A.z + P_B.x*P_C.y*P_A.z + 
                P_C.x*P_A.y*P_B.z - P_A.x*P_C.y*P_B.z - 
                P_B.x*P_A.y*P_C.z + P_A.x*P_B.y*P_C.z)/(6.0);

        }

    FUNCINFO("vol1 = "<<abs(volume1)<<".")


    faces = &(mesh2->meshes.faces);
    for (auto & fv : *faces){

        const auto P_A = mesh2->meshes.vertices.at( fv[0] );
        const auto P_B = mesh2->meshes.vertices.at( fv[1] );
        const auto P_C = mesh2->meshes.vertices.at( fv[2] );

        volume2 += (-P_C.x*P_B.y*P_A.z + P_B.x*P_C.y*P_A.z + 
                P_C.x*P_A.y*P_B.z - P_A.x*P_C.y*P_B.z - 
                P_B.x*P_A.y*P_C.z + P_A.x*P_B.y*P_C.z)/(6.0);
        }

    FUNCINFO("vol2 = "<<abs(volume2)<<".")
    FUNCINFO("diffVol = "<<abs(abs(volume1)-abs(volume2)))

    FUNCINFO("HAUSDORFF DISTANCE: " << max_distance << " or " << second_max_distance);

    // print out surface areas
    FUNCINFO("SURFACE AREA: First mesh = " << mesh1->meshes.surface_area() << ", second mesh = " << mesh2->meshes.surface_area());
    FUNCINFO("SURFACE AREA difference: " << mesh1->meshes.surface_area() - mesh2->meshes.surface_area());

    return true;
}
