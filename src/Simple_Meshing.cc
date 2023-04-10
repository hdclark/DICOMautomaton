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
#include "YgorLog.h"
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

    [[maybe_unused]] const auto pi = std::acos(-1.0);

    contour_of_points<double> contour_A = A.get();
    contour_of_points<double> contour_B = B.get();

    const auto N_A = contour_A.points.size();
    const auto N_B = contour_B.points.size();
    if( N_A == 0 ){
        throw std::invalid_argument("Contour A contains no vertices. Cannot continue.");
    }
    if( N_B == 0 ){
        throw std::invalid_argument("Contour B contains no vertices. Cannot continue.");
    }

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
    if(ortho_unit_A.Dot(ortho_unit_B) <= 0.0){
        // Handle special cases
        if( (N_A == 1) && (N_B != 1) ){
            ortho_unit_A = ortho_unit_B;
        }else if( (N_A != 1) && (N_B == 1) ){
            ortho_unit_B = ortho_unit_A;

        // Otherwise, flip one of the contours, recurse, and adjust the face labels to point to the actual vertex
        // layout.
        //
        // Note: This effectively ignores contour orientation altogether.
        }else{
            YLOGWARN("Ignoring adjacent contours with opposite orientations. Recursing..");
            std::reverse( std::begin(contour_B.points), std::end(contour_B.points) );
            auto faces = Estimate_Contour_Correspondence(A, std::ref(contour_B) );

            for(auto &face_arr: faces){
                for(auto &v_i : face_arr){
                    if(N_A <= v_i) v_i = N_A + (N_B - 1) - (v_i - N_A);
                }
            }
            return faces;
        }
    }

    const auto plane_A = contour_A.Least_Squares_Best_Fit_Plane( ortho_unit_A );

    vec3<double> centroid_A = contour_A.Average_Point();
    vec3<double> centroid_B = contour_B.Average_Point();

    // Adjust the contours to make determining the initial correspondence easier.
    if( (N_A > 2) && (N_B > 2) ){
        const bool assume_planar_contours = true;
        const auto area_A = std::abs( contour_A.Get_Signed_Area(assume_planar_contours) );
        const auto area_B = std::abs( contour_B.Get_Signed_Area(assume_planar_contours) );
        const auto scale = std::sqrt( area_A / area_B );
        if(!std::isfinite(scale)){
            throw std::invalid_argument("Contour area ratio is not finite. Refusing to continue.");
        }
        centroid_A = contour_A.Centroid();
        centroid_B = contour_B.Centroid();

        // Shift and scale contours for easy correspondence estimation, especially with objects oriented obliquely to
        // contour slice orientation.
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
        //YLOGINFO("Shortest distance is between " << *p_i << " and " << *p_j);
    }

    // Estimates the radius of the circumsphere (i.e., the smallest sphere that intersects all vertices) that bounds the
    // embedded triangle ABC.
    //
    // Note: see https://en.wikipedia.org/wiki/Circumscribed_circle#Higher_dimensions
    const auto circumsphere_radius = []( const vec3<double> &A,
                                         const vec3<double> &B,
                                         const vec3<double> &C ) -> double {
        const auto a = A - C;
        const auto b = B - C;
        const auto face_area = 0.5 * a.Cross(b).length();
        auto circum_r = (a.length() * b.length() * (a-b).length()) / (4.0 * face_area );
        if(std::isnan(circum_r)) circum_r = std::numeric_limits<double>::infinity();
        return circum_r;
    };

    // Estimates the local curvature of a polygon using previous, current, and next vertices.
    const auto est_curvature = [&]( const vec3<double> &A,
                                    const vec3<double> &B,
                                    const vec3<double> &C ) -> double {
        const auto circum_r = circumsphere_radius(A, B, C);
        const auto curvature = std::pow(circum_r, -2.0);
        return curvature;
    };

/*
    // Alternative that isn't actually curvature, but might be useful.
    const auto est_curvature = [&]( const vec3<double> &A,
                                    const vec3<double> &B,
                                    const vec3<double> &C ) -> double {
        const auto a = A - C;
        const auto b = B - C;
        bool OK;
        auto angle = a.angle(b, &OK);
        if(!OK) angle = pi; // Maybe not the best choice???
        const auto curvature = std::abs( angle / pi );
        return curvature;
    };
    // Computes the curvature-weighted perimeter of a contour.
    const auto weighted_perimeter = [&]( const contour_of_points<double> &c ) -> double {
        double wp_tot = 0.0;

        const auto beg = std::begin(c.points);
        const auto end = std::end(c.points);
        const auto last = std::prev(end);
        for(auto itB = beg; itB != end; ++itB){
            auto itA = (itB == beg ) ? std::prev(end) : std::prev(itB);
            auto itC = (itB == last) ? beg : std::next(itB);

            const auto dperim = itB->distance(*itA);
            const auto curvature = est_curvature(*itA, *itB, *itC);

            wp_tot += dperim * curvature;
        }
        return wp_tot;
    };
    const auto total_wperimeter_A = weighted_perimeter(contour_A);
    const auto total_wperimeter_B = weighted_perimeter(contour_B);
    YLOGINFO("total_wperimeter_A = " << total_wperimeter_A);
    YLOGINFO("total_wperimeter_B = " << total_wperimeter_B);
*/

    // Walk the contours, creating a sort of triangle strip.
    //std::set<size_t> used_verts_A;
    //std::set<size_t> used_verts_B;
    //size_t i = std::distance( beg_A, p_i );
    //size_t j = std::distance( beg_B, p_j );
    double accum_perimeter_A = 0.0;
    double accum_perimeter_B = 0.0;

    double accum_wperimeter_A = 0.0;
    double accum_wperimeter_B = 0.0;

    vec3<double> prev_face_N( std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::quiet_NaN() );

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

        double candidate_A_dwperim = 0.0;
        double candidate_B_dwperim = 0.0;

        if(N_edges_consumed_A < N_A){
            // Smallest face area.
            //criteria_w_i_next = 0.5 * ((*p_i_next - *p_i).Cross(*p_j - *p_i) ).length();

            // Alternate back-and-forth between two ~orthogonal metrics.
            if( (N_edges_consumed_A + N_edges_consumed_B) % 2 == 0){
                // Shortest longest edge length.
                const auto edge_length = (*p_i_next - *p_j).length();
                criteria_w_i_next = edge_length;
            }else{
                // Most 'vertical' cross-edge.
                criteria_w_i_next = -std::abs( (*p_i_next - *p_j).unit().Dot(ortho_unit_A) );
            }

            // Smallest discrepancy between interior angles.
            //bool OK;
            //const auto inf = 5000.0;
            //const auto angle_a = (OK) ? (*p_i_next - *p_i).angle(*p_j - *p_i, &OK) : inf;
            //const auto angle_b = (OK) ? (*p_i - *p_i_next).angle(*p_j - *p_i_next, &OK) : inf;
            //const auto angle_c = (OK) ? (*p_i - *p_j).angle(*p_i_next - *p_j, &OK) : inf;
            //const auto angle_max = std::max( {angle_a, angle_b, angle_c} );
            //const auto angle_min = std::min( {angle_a, angle_b, angle_c} );
            //if(OK){
            //    criteria_w_i_next = angle_max;
            //    //criteria_w_i_next = angle_max - angle_min;
            //}else{
            //    criteria_w_i_next = - inf;
            //}

            //// Smallest projected area onto the contour plane.
            //contour_of_points<double> cop( std::list<vec3<double>>{{ *p_i, *p_i_next, *p_j }} );
            ////cop.closed = true;
            //const auto proj_cop = cop.Project_Onto_Plane_Orthogonally(plane_A);
            //criteria_w_i_next = proj_cop.Perimeter();
            ////proj_cop.closed = true;
            ////criteria_w_i_next = std::abs( proj_cop.Get_Signed_Area(true) );

            //// Whichever face is most disaligned from the previous face.
            //if(prev_face_N.isfinite()){
            //    const auto cand_face_N = (*p_i_next - *p_j).Cross(*p_i - *p_j).unit();
            //    const auto agree = - std::abs( cand_face_N.Dot(prev_face_N) );
            //    if(cand_face_N.isfinite()) criteria_w_i_next = agree;
            //}


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

            //// Prefer the edge that makes the smallest rotational advancement.
            //const auto proj_p_j = plane_A.Project_Onto_Plane_Orthogonally(*p_j);
            //const auto angle = (*p_i_next - centroid_A).angle(proj_p_j - centroid_A);
            //const auto orien = (*p_i_next - centroid_A).Cross(proj_p_j - centroid_A).Dot(ortho_unit_A);
            //const double sign = (orien < 0) ? 1.0 : -1.0;
            //criteria_w_i_next = sign * angle;

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

            //// Ratio of triangle area to circumcircle (actually circumsphere) area.
            //const auto a = *p_i_next - *p_j;
            //const auto b = *p_i - *p_j;
            //const auto face_area = 0.5 * a.Cross(b).length();
            //if(face_area == 0.0){
            //    criteria_w_i_next = 0.0; // Acceptable, but won't beat any legitimate candidates.
            //}else{
            //    const auto circum_r = (a.length() * b.length() * (a-b).length()) / (4.0 * face_area );
            //    const auto circum_a = pi * circum_r * circum_r;
            //    criteria_w_i_next = - face_area / circum_a;
            //}

            //// Weighted perimeter.
            //const auto beg = std::begin(contour_A.points);
            //const auto end = std::end(contour_A.points);
            //const auto last = std::prev(end);
            //const auto p_i_prev = (p_i == beg) ? last : std::prev(p_i);

            //const auto curvature = est_curvature(*p_i_prev, *p_i, *p_i_next);
            //const auto dperim = p_i->distance(*p_i_prev);
            //const auto dwperim = dperim * curvature;
            //const auto d_wperimeter = (accum_wperimeter_A + dwperim) / total_wperimeter_A;
            //candidate_A_dwperim = dwperim;
            //criteria_w_i_next = - d_wperimeter;
        }
        if(N_edges_consumed_B < N_B){
            // Smallest face area.
            //criteria_w_j_next = 0.5 * ((*p_j_next - *p_j).Cross(*p_i - *p_j) ).length();

            // Alternate back-and-forth between two ~orthogonal metrics.
            if( (N_edges_consumed_A + N_edges_consumed_B) % 2 == 0){
                // Shortest longest edge length.
                const auto edge_length = (*p_j_next - *p_i).length();
                criteria_w_j_next = edge_length;
            }else{
                // Most 'vertical' cross-edge.
                criteria_w_j_next = -std::abs( (*p_j_next - *p_i).unit().Dot(ortho_unit_B) );
            }

            // Smallest discrepancy between interior angles.
            //bool OK = true;
            //const auto inf = 5000.0;
            //const auto angle_a = (OK) ? (*p_j_next - *p_j).angle(*p_i - *p_j, &OK) : inf;
            //const auto angle_b = (OK) ? (*p_j - *p_j_next).angle(*p_i - *p_j_next, &OK) : inf;
            //const auto angle_c = (OK) ? (*p_j - *p_i).angle(*p_j_next - *p_i, &OK) : inf;
            //const auto angle_max = std::max( {angle_a, angle_b, angle_c} );
            //const auto angle_min = std::min( {angle_a, angle_b, angle_c} );
            //if(OK){
            //    criteria_w_j_next = angle_max;
            //    //criteria_w_j_next = angle_max - angle_min;
            //}else{
            //    criteria_w_j_next = - inf;
            //}

            //// Smallest projected area onto the contour plane.
            //contour_of_points<double> cop( std::list<vec3<double>>{{ *p_j, *p_j_next, *p_i }} );
            ////cop.closed = true;
            //const auto proj_cop = cop.Project_Onto_Plane_Orthogonally(plane_B);
            //criteria_w_j_next = proj_cop.Perimeter();
            ////proj_cop.closed = true;
            ////criteria_w_j_next = std::abs( proj_cop.Get_Signed_Area(true) );

            //// Whichever face is most disaligned from the previous face.
            //if(prev_face_N.isfinite()){
            //    const auto cand_face_N = (*p_j_next - *p_j).Cross(*p_i - *p_j).unit();
            //    const auto agree = - std::abs( cand_face_N.Dot(prev_face_N) );
            //    if(cand_face_N.isfinite()) criteria_w_j_next = agree;
            //}

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

            //// Prefer the edge that makes the smallest rotational advancement.
            //const auto proj_p_i = plane_B.Project_Onto_Plane_Orthogonally(*p_i);
            //const auto angle = (*p_j_next - centroid_B).angle(proj_p_i - centroid_B);
            //const auto orien = (*p_j_next - centroid_B).Cross(proj_p_i - centroid_B).Dot(ortho_unit_B);
            //const double sign = (orien < 0) ? 1.0 : -1.0;
            //criteria_w_j_next = sign * angle;

            //Prefer the edge that 'consumes' the minimal arc length.
            //const auto hand_a = plane_A.Project_Onto_Plane_Orthogonally(*p_j_next) - plane_A.Project_Onto_Plane_Orthogonally(*p_i);
            //criteria_w_j_next = hand_a.length();

            //// Prefer the edge that most closely aligns with the local normal vector.
            ////
            //// Ideally considering only the most locally curved contour. TODO.
            ////const auto local_norm = (*p_j_next - *p_j).Cross(ortho_unit_A).unit();
            //const auto local_norm = (*p_i_next - *p_i).Cross(ortho_unit_A).unit();
            //const auto face_norm = (*p_j_next - *p_j).Cross(*p_i - *p_j).unit();
            ////criteria_w_i_next = 0.5 * ((*p_i_next - *p_i).Cross(*p_j - *p_i) ).length();
            ////const auto edge_line = (*p_i_next - *p_j).unit();
            //criteria_w_j_next = local_norm.Dot(face_norm) * -1.0;

            //// Ratio of triangle area to circumcircle (actually circumsphere) area.
            //const auto a = *p_j_next - *p_j;
            //const auto b = *p_i - *p_j;
            //const auto face_area = 0.5 * a.Cross(b).length();
            //if(face_area == 0.0){
            //    criteria_w_j_next = 0.0; // Acceptable, but won't beat any legitimate candidates.
            //}else{
            //    const auto circum_r = (a.length() * b.length() * (a-b).length()) / (4.0 * face_area );
            //    const auto circum_a = pi * circum_r * circum_r;
            //    criteria_w_j_next = - face_area / circum_a;
            //}

            //// Weighted perimeter.
            //const auto beg = std::begin(contour_B.points);
            //const auto end = std::end(contour_B.points);
            //const auto last = std::prev(end);
            //const auto p_j_prev = (p_j == beg) ? last : std::prev(p_j);

            //const auto curvature = est_curvature(*p_j_prev, *p_j, *p_j_next);
            //const auto dperim = p_j->distance(*p_j_prev);
            //const auto dwperim = dperim * curvature;
            //const auto d_wperimeter = (accum_wperimeter_B + dwperim) / total_wperimeter_B;
            //candidate_B_dwperim = dwperim;
            //criteria_w_j_next = d_wperimeter;
        }

        const auto accept_i_next = [&](){
            // Accept the i-next move.
            if(N_A < N_edges_consumed_A) throw std::logic_error("Looped contour A");

            if(N_edges_consumed_A % 2 == 0){
                prev_face_N = (*p_i_next - *p_j).Cross(*p_i - *p_j).unit();
            }else{
                prev_face_N.x = std::numeric_limits<double>::infinity();
            }

            const auto v_a = static_cast<size_t>( std::distance(beg_A, p_i) );
            const auto v_b = static_cast<size_t>( std::distance(beg_A, p_i_next) );
            const auto v_c = static_cast<size_t>( N_A + std::distance(beg_B, p_j) );
            faces.emplace_back( std::array<size_t, 3>{{ v_a, v_b, v_c }} );
            accum_perimeter_A += (*p_i_next - *p_i).length();
            accum_wperimeter_A += candidate_A_dwperim;
            N_edges_consumed_A += 1;
            if(N_edges_consumed_A <= N_A) p_i = p_i_next;
        };
        const auto accept_j_next = [&](){
            // Accept the j-next move.
            if(N_B < N_edges_consumed_B) throw std::logic_error("Looped contour B");

            if(N_edges_consumed_A % 2 == 0){
                prev_face_N = (*p_j_next - *p_j).Cross(*p_i - *p_j).unit();
            }else{
                prev_face_N.x = std::numeric_limits<double>::infinity();
            }

            const auto v_a = static_cast<size_t>( std::distance(beg_A, p_i) );
            const auto v_b = static_cast<size_t>( N_A + std::distance(beg_B, p_j_next) );
            const auto v_c = static_cast<size_t>( N_A + std::distance(beg_B, p_j) );
            faces.emplace_back( std::array<size_t, 3>{{ v_a, v_b, v_c }} );
            accum_perimeter_B += (*p_j_next - *p_j).length();
            accum_wperimeter_B += candidate_B_dwperim;
            N_edges_consumed_B += 1;
            if(N_edges_consumed_B <= N_B) p_j = p_j_next;
        };

        const bool A_is_valid = std::isfinite(criteria_w_i_next);
        const bool B_is_valid = std::isfinite(criteria_w_j_next);
        if(!A_is_valid && !B_is_valid){
YLOGWARN("Terminated meshing early. Mesh may be incomplete.");
//            throw std::logic_error("Terminated meshing early. Cannot continue.");
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
    //YLOGINFO("Completed meshing with N_faces = " << faces.size() << " where N_A + N_B = " << (N_A + N_B));


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


// Low-level routine that joins the vertices of one contour to the vertices of multiple contours. Returns a list of
// faces where the vertex indices refer to the vertices of the contours in the order provided by the caller. This
// routine may create additional vertices.
contour_of_points<double>
Minimally_Amalgamate_Contours(
        const vec3<double> &ortho_unit,
        const vec3<double> &pseudo_vert_offset,
        std::list<std::reference_wrapper<contour_of_points<double>>> B ){

    if(B.empty()){
        throw std::invalid_argument("No contours supplied in B. Cannot continue.");
    }
/*
    contour_of_points<double> contour_A = A.get();
    const auto N_A = contour_A.points.size();
    if( N_A == 0 ){
        throw std::invalid_argument("Contour A contains no vertices. Cannot continue.");
    }

    // Determine contour orientations. Single-vertex contours can take any orientation, so use a reasonable default.
    auto ortho_unit_A = vec3<double>(0.0, 0.0, 1.0);
    try{
        ortho_unit_A = contour_A.Estimate_Planar_Normal();
    }catch(const std::exception &){}

    const auto centroid_A = A.get().Centroid();
*/

    const auto has_consistent_orientation = [&]( std::reference_wrapper<contour_of_points<double>> cop_refw ) -> bool {
        // Determine if the contour has a consistent orientation as that provided by the user.
        auto l_ortho_unit = ortho_unit;
        try{
            l_ortho_unit = cop_refw.get().Estimate_Planar_Normal();
        }catch(const std::exception &){}
        return (0.0 < ortho_unit.Dot(l_ortho_unit));
    };

    // Confirm all contour orientations are consistent.
    std::list< contour_of_points<double> > reversed_cops_storage;
    for(auto crw_it = std::begin(B); crw_it != std::end(B); ){
        if(!has_consistent_orientation(*crw_it)){
            YLOGWARN("Found contour with inconsistent orientation. Making a reversed copy. This may discard information");
            reversed_cops_storage.emplace_back( crw_it->get() );
            reversed_cops_storage.back().points.reverse();
            (*crw_it) = std::ref(reversed_cops_storage.back());
        }
        ++crw_it;
    }
    for(const auto &cop_refw : B){
        if(cop_refw.get().closed == false){
            throw std::invalid_argument("Found open contour. Refusing to continue.");
        }
    }

    // Seed the first contour into the amalgamated contour.
    if(B.empty()){
        throw std::invalid_argument("No contours remaining in B. Cannot continue.");
    }
    contour_of_points<double> amal = B.front().get();
    B.pop_front();
    if( amal.points.size() < 3 ){
        throw std::invalid_argument("Seed contour in B contains insufficient vertices. Cannot continue.");
    }

    std::list< decltype(std::begin(amal.points)) > pseudo_verts;
    const auto is_a_pseudo_vert = [&]( decltype(std::begin(amal.points)) it ) -> bool {
        for(const auto &pv : pseudo_verts){
            if( it == pv ) return true;
        }
        return false;
    };

    while(true){
        double shortest_criteria = std::numeric_limits<double>::infinity();
        auto closest_cop = std::begin(B);
        decltype(std::begin(amal.points)) closest_a_v1_it;
        decltype(std::begin(amal.points)) closest_a_v2_it;
        decltype(std::begin(B.front().get().points)) closest_b_v1_it;
        decltype(std::begin(B.front().get().points)) closest_b_v2_it;
        vec3<double> closest_edge_1_midpoint;
        vec3<double> closest_edge_2_midpoint;

        // Cycle through all remaining candidate contours.
        for(auto b_it = std::begin(B); b_it != std::end(B); ++b_it){
            // Cycle through edge-edge pairings to identify a reasonable place to fuse contours.
            auto a_v1_it = std::prev(std::end(amal.points));
            auto a_v2_it = std::begin(amal.points);


            const auto a_end = std::end(amal.points);
            while(a_v2_it != a_end){
                // Disregard this edge if any of the verts are ficticious.
                if( is_a_pseudo_vert(a_v1_it)
                ||  is_a_pseudo_vert(a_v2_it) ){  
                    a_v1_it = a_v2_it;
                    ++a_v2_it;
                    continue;
                }
                auto b_v1_it = std::prev(std::end(b_it->get().points));
                auto b_v2_it = std::begin(b_it->get().points);
                const auto b_end = std::end(b_it->get().points);
                while(b_v2_it != b_end){

                    const auto d_a = a_v1_it->distance( *a_v2_it );
                    const auto d_b = b_v1_it->distance( *b_v2_it );
                    
                    const auto machine_eps = std::sqrt(10.0 * std::numeric_limits<double>::epsilon() );
                    if( (d_a < machine_eps)
                    ||  (d_b < machine_eps) ){
                        b_v1_it = b_v2_it;
                        ++b_v2_it;
                        continue;
                    }

                    const auto edge_1_length = a_v1_it->distance( *b_v2_it );
                    const auto edge_2_length = a_v2_it->distance( *b_v1_it );

                    //const auto edge_1 = line_segment<double>(*a_v1_it, *b_v2_it);
                    //const auto edge_2 = line_segment<double>(*a_v2_it, *b_v1_it);
                    //const auto edge_1_centroid_dist = edge_1.Closest_Point_To(centroid_A).distance(centroid_A);
                    //const auto edge_2_centroid_dist = edge_2.Closest_Point_To(centroid_A).distance(centroid_A);

                    //const auto total_criteria = edge_1_length + edge_2_length + edge_1_centroid_dist + edge_2_centroid_dist;
                    const auto total_criteria = edge_1_length + edge_2_length;

                    if( !std::isfinite(shortest_criteria)
                    ||  (total_criteria < shortest_criteria) ){
                        shortest_criteria = total_criteria;
                        closest_cop = b_it;
                        closest_a_v1_it = a_v1_it;
                        closest_a_v2_it = a_v2_it;
                        closest_b_v1_it = b_v1_it;
                        closest_b_v2_it = b_v2_it;
                        closest_edge_1_midpoint = (*a_v1_it + *b_v2_it) * 0.5;
                        closest_edge_2_midpoint = (*a_v2_it + *b_v1_it) * 0.5;
                    }
                    b_v1_it = b_v2_it;
                    ++b_v2_it;
                }
                a_v1_it = a_v2_it;
                ++a_v2_it;
            }
        }

        if(!std::isfinite(shortest_criteria)){
            // No (valid) contours remaining. so terminate the search.
            break;
        }

        // Adjust the closest contour so the selected edges break naturally at the list front and back.
        //std::rotate( std::begin(closest_cop->points),
        //             closest_b_v1_it,
        //             std::end(closest_cop->points) );
        contour_of_points<double> cop_copy;
        cop_copy.closed = true;
        if( closest_b_v2_it == std::begin(closest_cop->get().points) ){
            cop_copy.points.insert( std::end(cop_copy.points),
                                    std::begin(closest_cop->get().points),
                                    std::end(closest_cop->get().points) );
        }else{
            cop_copy.points.insert( std::end(cop_copy.points),
                                    closest_b_v2_it,
                                    std::end(closest_cop->get().points) );
            cop_copy.points.insert( std::end(cop_copy.points),
                                    std::begin(closest_cop->get().points),
                                    closest_b_v2_it );
        }

        // Insert ficticious verts on the front and back.
        //
        // Note: these vertices are offset so that later contour interpolation (i.e., mesh "slicing") on the original
        //       planes will return (approximately) the original contours.
        //
        cop_copy.points.emplace_front(closest_edge_1_midpoint + pseudo_vert_offset);
        cop_copy.points.emplace_back(closest_edge_2_midpoint + pseudo_vert_offset);
        pseudo_verts.emplace_back( std::begin(cop_copy.points) );
        pseudo_verts.emplace_back( std::prev( std::end(cop_copy.points) ) );
        

        // Merge the points into amal.
        amal.points.splice( closest_a_v2_it, // Before this point.
                            cop_copy.points );

        B.erase(closest_cop);
    }

    return amal;
}

/*contour_of_points<double>
Minimally_Amalgamate_Contours_N_to_N(
        const vec3<double> &ortho_unit,
        const vec3<double> &pseudo_vert_offset,
        std::list<std::reference_wrapper<contour_of_points<double>>> B ){
    if(B.empty()) {
        throw std::invalid_argument("No contours supplied in B. Cannot continue.");
    }
    
    const auto has_consistent_orientation = [&]( std::reference_wrapper<contour_of_points<double>> cop_refw ) -> bool {
        // Determine if the contour has a consistent orientation as that provided by the user.
        auto l_ortho_unit = ortho_unit;
        try{
            l_ortho_unit = cop_refw.get().Estimate_Planar_Normal();
        }catch(const std::exception &){}
        return (0.0 < ortho_unit.Dot(l_ortho_unit));
    };

    // Confirm all contour orientations are consistent.
    std::list< contour_of_points<double> > reversed_cops_storage;
    for(auto crw_it = std::begin(B); crw_it != std::end(B); ){
        if(!has_consistent_orientation(*crw_it)){
            YLOGWARN("Found contour with inconsistent orientation. Making a reversed copy. This may discard information");
            reversed_cops_storage.emplace_back( crw_it->get() );
            reversed_cops_storage.back().points.reverse();
            (*crw_it) = std::ref(reversed_cops_storage.back());
        }
        ++crw_it;
    }
    for(const auto &cop_refw : B){
        if(cop_refw.get().closed == false){
            throw std::invalid_argument("Found open contour. Refusing to continue.");
        }
    }
    contour_of_points<double> amal = B.front().get();
    B.pop_front();
    if(amal.points.size() < 3) {
        throw std::invalid_argument("Seed contour in B contains insufficient vertices. Cannot continue.");
    }
    
    std::list<decltype(std::begin(amal.points))> pseudo_verts;
    const auto is_a_pseudo_vert = [&]( decltype(std::begin(amal.points)) it ) -> bool {
        for(const auto &pv : pseudo_verts){
            if( it == pv ) return true;
        }
        return false;
    };

    while(true) {
        double shortest_criteria = std::numeric_limits<double>::infinity();
        auto closest_cop = std::begin(B);
        decltype(std::begin(amal.points)) closest_a_v_it;
        decltype(std::begin(B.front().get().points)) closest_b_it;

        for(auto b_it = std::begin(B); b_it != std::end(B); ++b_it){
            auto a_v_it = std::begin(amal.points);
            if(is_a_pseudo_vert(a_v_it)) {
                
            }
        }
    }
}*/