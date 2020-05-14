//Simple_Meshing.cc - A part of DICOMautomaton 2020. Written by hal clark.

#include <string>    
#include <fstream>
#include <vector>
#include <set> 
#include <map>
#include <unordered_map>
#include <list>
#include <functional>
#include <iterator>
#include <thread>
#include <array>
#include <mutex>
#include <limits>
#include <cmath>
#include <memory>
#include <stdexcept>

#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <optional>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathIOOBJ.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorAlgorithms.h"   //Needed for For_Each_In_Parallel<..>(...)
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Structs.h"

#include "Simple_Meshing.h"



// Low-level routine that joins the vertices of two contours.
// Returns a list of faces where the vertex indices refer to A followed by B.
// The number of faces will be equal to the combined number of vertices in both contours.
std::vector< std::array<size_t, 3> >
Estimate_Contour_Correspondence(
        std::reference_wrapper<contour_of_points<double>> A,
        std::reference_wrapper<contour_of_points<double>> B ){

    contour_of_points<double> contour_A = A.get();
    contour_of_points<double> contour_B = B.get();

    const auto N_A = contour_A.points.size();
    const auto N_B = contour_B.points.size();
    if( N_A == 0 ){
        throw std::invalid_argument("Contour A contains no vertices. Cannot continue.");
    }
    if( N_B == 0 ){
        throw std::invalid_argument("Contour A contains no vertices. Cannot continue.");
    }
//FUNCINFO("Contours A and B have size " << N_A << " and " << N_B);

    // Determine contour orientations. Single-vertex contours can take any orientation, so use a reasonable default.
    auto ortho_unit_A = vec3<double>(0.0, 0.0, 1.0);
    auto ortho_unit_B = vec3<double>(0.0, 0.0, 1.0);
    try{
        ortho_unit_A = contour_A.Estimate_Planar_Normal();
    }catch(const std::exception &){}
    try{
        ortho_unit_B = contour_B.Estimate_Planar_Normal();
    }catch(const std::exception &){}

    // Ensure the contours have the same orientation.
    //
    // Note: This is crucial since the layout of vertices will be backward. To ignore it, we should make the orientation
    //       consistent by reversing layout of one of the contours.
    if(ortho_unit_A.Dot(ortho_unit_B) <= 0.0){
        throw std::invalid_argument("Mismatched contour orientations. Refusing to continue.");
        // Note: we could simply reverse the orientation of one contour, call self recursively, and then swap the faces
        // accordingly. (Have to special-case the single-vertex cases though.) TODO.
    }

    const auto plane_A = contour_A.Least_Squares_Best_Fit_Plane( ortho_unit_A );
    const auto plane_B = contour_B.Least_Squares_Best_Fit_Plane( ortho_unit_B );

    // Adjust the contours to make determining the initial correspondence easier.
    if( (N_A > 2) && (N_B > 2) ){
        const bool assume_planar_contours = true;
        const auto area_A = std::abs( contour_A.Get_Signed_Area(assume_planar_contours) );
        const auto area_B = std::abs( contour_B.Get_Signed_Area(assume_planar_contours) );
        const auto scale = std::sqrt( area_A / area_B );
        if(!std::isfinite(scale)){
            throw std::invalid_argument("Contour area ratio is not finite. Refusing to continue.");
        }
        const auto centroid_A = contour_A.Centroid();
        const auto centroid_B = contour_B.Centroid();

        const auto dcentroid = plane_A.Project_Onto_Plane_Orthogonally(centroid_A)
                             - plane_A.Project_Onto_Plane_Orthogonally(centroid_B);

        for(auto &p : contour_B.points){
            // Scale the vertex distance.
            p = centroid_B + (p - centroid_B) * scale;

            // Shift along with the centroid.
            p += dcentroid;
        }
    }

    auto beg_A = std::begin(contour_A.points);
    auto beg_B = std::begin(contour_B.points);
    auto end_A = std::end(contour_A.points);
    auto end_B = std::end(contour_B.points);

    // Additional metrics needed for alternative meshing heuristics below.
    //const auto total_perimeter_A = contour_A.Perimeter();
    //const auto total_perimeter_B = contour_B.Perimeter();
    //const auto centre_A = contour_A.Centroid();
    //const auto centre_B = contour_B.Centroid();
    //const auto centre_avg = (centre_A + centre_B) * 0.5;

    // Find two corresponding points to 'seed' the meshing.
    //
    // Note: This step is crucial as it effectively locks together the vertices. A full pass of both contours would be
    //       best (but slow). An alternative is to preferentially consider low-curvature vertices.
    //
    // ... TODO ...
    auto p_i = beg_A;
    auto p_j = beg_B;
    {
        double min_sqd_i_j = std::numeric_limits<double>::infinity();
        for(auto p = beg_B; p != end_B; ++p){
            const auto sqd_i_j = p_i->sq_dist(*p);
            if(sqd_i_j < min_sqd_i_j){
                p_j = p;
                min_sqd_i_j = sqd_i_j;
            }
        }
//FUNCINFO("Shortest distance is between " << *p_i << " and " << *p_j);
    }

    // Walk the contours, creating a sort of triangle strip.
    //std::set<size_t> used_verts_A;
    //std::set<size_t> used_verts_B;
    //size_t i = std::distance( beg_A, p_i );
    //size_t j = std::distance( beg_B, p_j );
    double accum_perimeter_A = 0.0;
    double accum_perimeter_B = 0.0;

    size_t N_edges_consumed_A = 0;
    size_t N_edges_consumed_B = 0;
    std::vector<std::array<size_t, 3>> faces; // Assuming A vertices are first. Zero-based.

    for(size_t n = 0; n < (N_A + N_B); ++n){
        // Figure out which candidate vertices we have.
        auto p_i_next = std::next(p_i);
        auto p_j_next = std::next(p_j);

        p_i_next = (p_i_next == end_A) ? beg_A : p_i_next; // Circular wrap.
        p_j_next = (p_j_next == end_B) ? beg_B : p_j_next; // Circular wrap.


        // Of the two candidate triangles, select one based on some criteria.
        double criteria_w_i_next = std::numeric_limits<double>::infinity();
        double criteria_w_j_next = std::numeric_limits<double>::infinity();

        if(N_edges_consumed_A < N_A){
            // Smallest face area.
            //criteria_w_i_next = 0.5 * ((*p_i_next - *p_i).Cross(*p_j - *p_i) ).length();

            // Shortest longest edge length.
            //criteria_w_i_next = (*p_i_next - *p_j).length();

            // Most 'vertical' cross-edge.
            //criteria_w_i_next = -std::abs( (*p_i_next - *p_j).Dot(ortho_unit_A) );

            // Smallest discrepancy between interior angles.
            //const auto angle_a = (*p_i_next - *p_i).angle(*p_j - *p_i);
            //const auto angle_b = (*p_i - *p_i_next).angle(*p_j - *p_i_next);
            //const auto angle_c = (*p_i - *p_j).angle(*p_i_next - *p_j);
            //const auto angle_max = std::max( {angle_a, angle_b, angle_c} );
            //const auto angle_min = std::min( {angle_a, angle_b, angle_c} );
            //criteria_w_i_next = angle_max - angle_min;

            // Smallest projected area onto the contour plane.
            contour_of_points<double> cop( std::list<vec3<double>>{{ *p_i, *p_i_next, *p_j }} );
            const auto proj_cop = cop.Project_Onto_Plane_Orthogonally(plane_A);
            criteria_w_i_next = proj_cop.Perimeter();
            //proj_cop.closed = true;
            //criteria_w_i_next = std::abs( proj_cop.Get_Signed_Area(true) );

            // Pick whichever is lagging in terms of perimeter.
            //criteria_w_i_next = accum_perimeter_A + (*p_i_next - *p_i).length() / total_perimeter_A;

            // Smallest face perimeter.
            //contour_of_points<double> cop( std::list<vec3<double>>{{ *p_i, *p_i_next, *p_j }} );
            //cop.closed = true;
            //criteria_w_i_next = cop.Perimeter();

            // Penalize adjacent moves that backtrack.

            // ... pointless because we can't backtrack? ...

            // Prefer the edge that most closely aligns with the rotational sweep plane (i.e., the hand of a clock
            // circulating around the contour).
            //const auto p_mid = (*p_i_next + *p_j) * 0.5;
            //const auto sweep_line = (p_mid - centre_avg).unit();
            //const auto edge_line = (*p_i_next - *p_j).unit();
            //criteria_w_i_next = -std::abs( edge_line.Dot(sweep_line) );

            //Prefer the edge that 'consumes' the minimal arc length.
            //const auto hand_a = plane_A.Project_Onto_Plane_Orthogonally(*p_i_next) - plane_A.Project_Onto_Plane_Orthogonally(*p_j);
            //criteria_w_i_next = hand_a.length();

            // Prefer the edge that most closely aligns with the local normal vector.
            //
            // Ideally considering only the most locally curved contour. TODO.
            ////const auto local_norm = (*p_i_next - *p_i).Cross(ortho_unit_A).unit();
            //const auto local_norm = (*p_j_next - *p_j).Cross(ortho_unit_A).unit();
            //const auto face_norm = (*p_i_next - *p_j).Cross(*p_i - *p_j).unit();
            ////criteria_w_i_next = 0.5 * ((*p_i_next - *p_i).Cross(*p_j - *p_i) ).length();
            ////const auto edge_line = (*p_i_next - *p_j).unit();
            //criteria_w_i_next = local_norm.Dot(face_norm) * -1.0;
        }
        if(N_edges_consumed_B < N_B){
            // Smallest face area.
            //criteria_w_j_next = 0.5 * ((*p_j_next - *p_j).Cross(*p_i - *p_j) ).length();

            // Shortest longest edge length.
            //criteria_w_j_next = (*p_j_next - *p_i).length();

            // Most 'vertical' cross-edge.
            //criteria_w_j_next = -std::abs( (*p_j_next - *p_i).Dot(ortho_unit_A) );

            // Smallest discrepancy between interior angles.
            //const auto angle_a = (*p_j_next - *p_j).angle(*p_i - *p_j);
            //const auto angle_b = (*p_j - *p_j_next).angle(*p_i - *p_j_next);
            //const auto angle_c = (*p_j - *p_i).angle(*p_j_next - *p_i);
            //const auto angle_max = std::max( {angle_a, angle_b, angle_c} );
            //const auto angle_min = std::min( {angle_a, angle_b, angle_c} );
            //criteria_w_j_next = angle_max - angle_min;

            // Smallest projected area onto the contour plane.
            contour_of_points<double> cop( std::list<vec3<double>>{{ *p_j, *p_j_next, *p_i }} );
            const auto proj_cop = cop.Project_Onto_Plane_Orthogonally(plane_B);
            criteria_w_j_next = proj_cop.Perimeter();
            //proj_cop.closed = true;
            //criteria_w_j_next = std::abs( proj_cop.Get_Signed_Area(true) );

            // Pick whichever is lagging in terms of perimeter.
            //criteria_w_j_next = accum_perimeter_B + (*p_j_next - *p_j).length() / total_perimeter_B;

            // Smallest face perimeter.
            //contour_of_points<double> cop( std::list<vec3<double>>{{ *p_j, *p_j_next, *p_i }} );
            //cop.closed = true;
            //criteria_w_j_next = cop.Perimeter();

            // Penalize adjacent moves that backtrack.

            // ... pointless because we can't backtrack? ...

            // Prefer the edge that most closely aligns with the rotational sweep plane (i.e., the hand of a clock
            // circulating around the contour).
            //const auto p_mid = (*p_j_next + *p_i) * 0.5;
            //const auto sweep_line = (p_mid - centre_avg).unit();
            //const auto edge_line = (*p_j_next - *p_i).unit();
            //criteria_w_j_next = -std::abs( edge_line.Dot(sweep_line) );

            //Prefer the edge that 'consumes' the minimal arc length.
            //const auto hand_a = plane_A.Project_Onto_Plane_Orthogonally(*p_j_next) - plane_A.Project_Onto_Plane_Orthogonally(*p_i);
            //criteria_w_j_next = hand_a.length();

            // Prefer the edge that most closely aligns with the local normal vector.
            //
            // Ideally considering only the most locally curved contour. TODO.
            ////const auto local_norm = (*p_j_next - *p_j).Cross(ortho_unit_A).unit();
            //const auto local_norm = (*p_i_next - *p_i).Cross(ortho_unit_A).unit();
            //const auto face_norm = (*p_j_next - *p_j).Cross(*p_i - *p_j).unit();
            ////criteria_w_i_next = 0.5 * ((*p_i_next - *p_i).Cross(*p_j - *p_i) ).length();
            ////const auto edge_line = (*p_i_next - *p_j).unit();
            //criteria_w_j_next = local_norm.Dot(face_norm) * -1.0;
        }

        const auto accept_i_next = [&](void){
            // Accept the i-next move.
            if(N_A < N_edges_consumed_A) throw std::logic_error("Looped contour A");
            const auto v_a = static_cast<size_t>( std::distance(beg_A, p_i) );
            const auto v_b = static_cast<size_t>( std::distance(beg_A, p_i_next) );
            const auto v_c = static_cast<size_t>( N_A + std::distance(beg_B, p_j) );
            faces.emplace_back( std::array<size_t, 3>{{ v_a, v_b, v_c }} );
            accum_perimeter_A += (*p_i_next - *p_i).length();
            N_edges_consumed_A += 1;
            if(N_edges_consumed_A <= N_A) p_i = p_i_next;
        };
        const auto accept_j_next = [&](void){
            // Accept the j-next move.
            if(N_B < N_edges_consumed_B) throw std::logic_error("Looped contour B");
            const auto v_a = static_cast<size_t>( std::distance(beg_A, p_i) );
            const auto v_b = static_cast<size_t>( N_A + std::distance(beg_B, p_j_next) );
            const auto v_c = static_cast<size_t>( N_A + std::distance(beg_B, p_j) );
            faces.emplace_back( std::array<size_t, 3>{{ v_a, v_b, v_c }} );
            accum_perimeter_B += (*p_j_next - *p_j).length();
            N_edges_consumed_B += 1;
            if(N_edges_consumed_B <= N_B) p_j = p_j_next;
        };

        const bool A_is_valid = std::isfinite(criteria_w_i_next);
        const bool B_is_valid = std::isfinite(criteria_w_j_next);
        if(!A_is_valid && !B_is_valid){
            throw std::logic_error("Terminated meshing early. Cannot continue.");
            // Note: Could be due to:
            //       - Non-finite vertex in input.
            //       - Invalid number of loops in the implementation above.
            //       - (Possibly) duplicate vertices??
            //       - Possibly something else.
        }else if( A_is_valid && !B_is_valid){
            accept_i_next();
        }else if( !A_is_valid && B_is_valid){
            accept_j_next();
        }else if(criteria_w_i_next < criteria_w_j_next){
            accept_i_next();
        }else{
            accept_j_next();
        }
    }
//FUNCINFO("Completed meshing with N_faces = " << faces.size() << " where N_A + N_B = " << (N_A + N_B));


/*
    ////////////////////////////////
    // Output the result for debugging.
    {
        fv_surface_mesh<double, uint64_t> amesh;
        for(const auto &p : contour_A.points) amesh.vertices.emplace_back(p);
        for(const auto &p : contour_B.points) amesh.vertices.emplace_back(p);
        for(const auto &fs : faces){
            amesh.faces.emplace_back( std::vector<uint64_t>{{fs[0], fs[1], fs[2]}} );
        }
        amesh.recreate_involved_face_index();

        std::ofstream os("/tmp/mesh_preprocessed.obj");
        if(!WriteFVSMeshToOBJ(amesh, os)){
            throw std::runtime_error("Unable to write mesh to file.");
        }
    }
    ////////////////////////////////
*/

    return faces;
}

