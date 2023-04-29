//Complex_Branching_Meshing.cc - A part of DICOMautomaton 2020. Written by hal clark.

#include <string>
#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <list>
#include <stack>
#include <functional>
#include <numeric>
#include <iterator>
#include <thread>
#include <array>
#include <mutex>
#include <limits>
#include <cmath>
#include <memory>
#include <stdexcept>

#include <cstdlib> //Needed for exit() calls.
#include <utility> //Needed for std::pair.
#include <algorithm>
#include <optional>

#include "YgorMisc.h" //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorMath.h" //Needed for vec3 class.

#include "Structs.h"

#include "Complex_Branching_Meshing.h"

// HELPERS

// rotates a contour such that all the contour points lie in the XY plane
// and the contour normal is parallel to the z-axis
// modifies contour in place
void
Rotate_Contour_Onto_XY(contour_of_points<double> &cop, const vec3<double> &cont_normal) {
    //The rotation matrix R that rotates the contour normal to the direction of
    //the z-axis can be applied to all the contour points to move them to the XY plane
    auto unitNormContour = cont_normal.unit();

    vec3<double> unitNormDesired = vec3<double>(0, 0, 1);
    if((unitNormDesired.Cross(unitNormContour)).length() == 0) {
        return;
    };

    //obtaining R matrix as described here https://math.stackexchange.com/a/2672702
    num_array<double> I = num_array<double>().identity(3);
    auto k              = unitNormContour + unitNormDesired;
    auto K              = k.to_num_array();
    auto scale          = 2 / (K.transpose() * K).read_coeff(0, 0);
    auto R              = K * K.transpose() *= (scale);
    R                   = R - I;
    //apply rotation to all points
    for(auto &point : cop.points) {
        point = (R * point.to_num_array()).to_vec3();
    }
}

// retrieve element in stack underneath top element
int
Next_To_Top(std::stack<int> &s) {
    auto p = s.top();
    s.pop();
    auto res = s.top();
    s.push(p);
    return res;
}

// returns 0 if collinear, 1 if CW, 2 if CCW
// CW if looking from p, we go CW from q to r
// CCW if looking from p, we go CCW from q to r
int
orientation(const vec3<double> p, const vec3<double> q, const vec3<double> r) {
    auto val = (q.y - p.y) * (r.x - q.x) - (q.x - p.x) * (r.y - q.y);
    if(val == 0)
        return 0;
    return (val > 0) ? 1 : 2;
}

// MAIN METHODS

// returns contour of points of convex hull by following https://www.geeksforgeeks.org/convex-hull-using-graham-scan/
contour_of_points<double>
Contour_From_Convex_Hull_2(std::vector<contour_of_points<double>> &cops, const vec3<double> &cont_normal) {
    if(cops.size() == 0) {
        YLOGINFO("Returning empty contour of points.");
        return contour_of_points<double>();
    }

    contour_of_points<double> merged_cop;

    for(auto &cop : cops) {
        merged_cop.points.insert(merged_cop.points.end(), cop.points.begin(), cop.points.end());
    }

    contour_of_points<double> rotated_cop(merged_cop);
    Rotate_Contour_Onto_XY(rotated_cop, cont_normal);

    // creating vector from list for quick access
    std::vector<vec3<double>> points(rotated_cop.points.begin(), rotated_cop.points.end());

    // create vector of associated indices to manipulate when finding convex hull
    // useful for recreating contour of points for convex hull with same order as input
    std::vector<int> indices(points.size());
    std::iota(std::begin(indices), std::end(indices), 0);

    // returns point based on index from indices[i]
    const auto get_point = [&](int i) -> vec3<double> {
        return points[indices[i]];
    };

    // find leftest and lowest point
    double ymin = get_point(0).y;
    int min_i   = 0;

    for(int i = 1; i < indices.size(); ++i) {
        int y = get_point(i).y;
        if(y < ymin || (ymin == y && get_point(i).x < get_point(min_i).x)) {
            ymin  = y;
            min_i = i;
        }
    }
    std::iter_swap(indices.begin() + 0, indices.begin() + min_i);

    // sort rest of points to form a simple closed path CCW
    const auto p0 = get_point(0);

    // returns true if point A is closer to p0 than point B
    const auto compare_points = [&](const int index_A, const int index_B) -> bool {
        const auto point_A = points[index_A];
        const auto point_B = points[index_B];
        const int o        = orientation(p0, point_A, point_B);

        if(o == 0)
            return (point_B.sq_dist(p0) > point_A.sq_dist(p0)) ? true : false;

        return (o == 2) ? true : false;
    };

    std::sort(indices.begin() + 1, indices.end(), [&](const int A, const int B) -> bool {
        return compare_points(A, B);
    });

    int m = 1; // size of array for concave hull

    // only keep furthest point if multiple points form the same angle with p0
    for(int i = 1; i < points.size(); ++i) {
        while(i < points.size() - 1 && orientation(p0, get_point(i), get_point(i + 1)) == 0) {
            i++;
        }

        indices[m] = indices[i];
        m++;
    }

    if(m < 3) {
        throw std::runtime_error("Convex hull is not possible for less than 3 points");
    }

    // remove concave points
    std::stack<int> s;
    s.push(0);
    s.push(1);
    s.push(2);

    for(int i = 3; i < m; ++i) {
        while(s.size() > 1 && orientation(get_point(Next_To_Top(s)), get_point(s.top()), get_point(i)) != 2) {
            s.pop();
        }
        s.push(i);
    }

    // stack contains output points
    std::vector<int> kept_indices;
    while(!s.empty()) {
        kept_indices.emplace_back(indices[s.top()]);
        s.pop();
    }

    contour_of_points<double> convex_hull_contour;

    std::vector<vec3<double>> merged_points(merged_cop.points.begin(), merged_cop.points.end());

    for(auto i : kept_indices) {
        convex_hull_contour.points.emplace_back(merged_points[i]);
    }

    convex_hull_contour.closed = true;

    return convex_hull_contour;
}

// assumes there are only two contours in cops
// modifies convex hull by adding midpoints and including more original contour points
// returns closed modified convex hull, two non-convex hull contours (one for each original contour), and list of midpoints
// the non-convex hull contours start and end with closest point on convex hull
std::tuple<contour_of_points<double>, contour_of_points<double>, contour_of_points<double>, std::vector<vec3<double>>>
Modify_Convex_Hull(const contour_of_points<double> &convex_hull, const std::vector<contour_of_points<double>> cops,
                   const vec3<double> &pseudo_vert_offset) {
    vec3<double> start_A;
    vec3<double> end_A;
    vec3<double> start_B;
    vec3<double> end_B;
    vec3<double> midpoint_for_end_A;
    vec3<double> midpoint_for_start_A;

    bool found_first = false;

    int prev_contour_index = -1;
    vec3<double> prev_point;
    int first_point_contour_index = 0;

    for(auto it = convex_hull.points.begin(); it != convex_hull.points.end(); it++) {
        int curr_contour_index = 0;
        for(auto &cop : cops) {
            if(std::find(cop.points.begin(), cop.points.end(), *it) != cop.points.end()) {
                if(prev_contour_index != -1 && prev_contour_index != curr_contour_index) {
                    vec3<double> midpoint = (*it + prev_point) / 2 + pseudo_vert_offset;
                    if(!found_first) {
                        end_A              = prev_point;
                        start_B            = *it;
                        midpoint_for_end_A = midpoint;
                        found_first        = true;
                    } else {
                        end_B                = prev_point;
                        start_A              = *it;
                        midpoint_for_start_A = midpoint;
                    }
                }
                break;
            }
            curr_contour_index += 1;
        }
        if(it == convex_hull.points.begin()) {
            first_point_contour_index = curr_contour_index;
        }
        prev_contour_index = curr_contour_index;
        prev_point         = *it;

        // check last point and first point
        if(it == std::prev(convex_hull.points.end()) && first_point_contour_index != curr_contour_index) {
            end_B                 = *it;
            start_A               = *(convex_hull.points.begin());
            vec3<double> midpoint = (*it + *(convex_hull.points.begin())) / 2 + pseudo_vert_offset;
            midpoint_for_start_A  = midpoint;
            break;
        }
    }

    // swap points depending on orientation
    bool convex_hull_ccw = convex_hull.Is_Counter_Clockwise();
    if(cops[first_point_contour_index].Is_Counter_Clockwise() != convex_hull_ccw) {
        std::swap(start_A, end_A);
        std::swap(midpoint_for_start_A, midpoint_for_end_A);
    }
    if(cops[1 - first_point_contour_index].Is_Counter_Clockwise() != convex_hull_ccw) {
        std::swap(start_B, end_B);
    }

    auto d1 = start_A.sq_dist(midpoint_for_start_A);
    auto d2 = end_A.sq_dist(midpoint_for_end_A);
    if (std::max(d1,d2) > 2 * std::min(d1,d2)) throw std::runtime_error("Complex 2 to 1 meshing is not suitable for this contour. Aborted");

    const auto get_convex_and_non_convex_points =
        [&](const int contour_index, vec3<double> start_point,
            vec3<double> end_point) -> std::tuple<std::list<vec3<double>>, std::list<vec3<double>>> {
        std::list<vec3<double>> split_section_1;
        std::list<vec3<double>> consecutive_section_2;
        std::list<vec3<double>> split_section_3;

        bool is_consecutive_section_convex_hull = false;
        bool found_start_point                  = false;
        bool found_end_point                    = false;
        bool filling_split_section              = true;
        bool on_split_section_3                 = false;

        for(auto point : cops[contour_index].points) {
            if(point == start_point) {
                if(found_end_point) { // already found end point before, so doing 3rd portion
                    on_split_section_3    = true;
                    filling_split_section = true;
                } else {
                    filling_split_section = false;
                }
                found_start_point = true;
            }

            if(point == end_point) {
                if(found_start_point) { // already found start point, so doing 3rd portion
                    is_consecutive_section_convex_hull = true;
                    filling_split_section              = true;
                    on_split_section_3                 = true;
                    consecutive_section_2.emplace_back(point); // don't want the end point to be in the third portion
                } else {
                    filling_split_section = false;
                    split_section_1.emplace_back(point);
                }
                found_end_point = true;
                continue; // since we placed the point in the previous section already
            }

            if(filling_split_section) {
                if(on_split_section_3) {
                    split_section_3.emplace_back(point);
                } else {
                    split_section_1.emplace_back(point);
                }
            } else {
                consecutive_section_2.emplace_back(point);
            }
        }

        split_section_3.insert(split_section_3.end(), split_section_1.begin(), split_section_1.end());

        std::list<vec3<double>> &convex_hull_points = split_section_3;
        std::list<vec3<double>> &other_points       = consecutive_section_2;

        if(is_consecutive_section_convex_hull) {
            std::swap(convex_hull_points, other_points);
        }

        // add start and end points to non convex hull points to ensure proper meshing
        other_points.emplace_front(convex_hull_points.back());
        other_points.emplace_back(convex_hull_points.front());

        return std::make_tuple(convex_hull_points, other_points);
    };

    // create better convex hull by chaining original points
    auto [convex_hull_A, other_A] = get_convex_and_non_convex_points(first_point_contour_index, start_A, end_A);
    auto [convex_hull_B, other_B] = get_convex_and_non_convex_points(1 - first_point_contour_index, start_B, end_B);

    // flip contour B if end point is closer to midpoint for end A
    if(end_B.sq_dist(midpoint_for_end_A) < start_B.sq_dist(midpoint_for_end_A)) {
        convex_hull_B.reverse();
        other_B.reverse();
    }

    // add midpoints in contour
    contour_of_points<double> modified_convex_cop;
    modified_convex_cop.points.insert(modified_convex_cop.points.end(), convex_hull_A.begin(), convex_hull_A.end());
    modified_convex_cop.points.emplace_back(midpoint_for_end_A);
    modified_convex_cop.points.insert(modified_convex_cop.points.end(), convex_hull_B.begin(), convex_hull_B.end());
    modified_convex_cop.points.emplace_back(midpoint_for_start_A);
    modified_convex_cop.closed = true;

    std::vector<vec3<double>> midpoints;
    midpoints.emplace_back(midpoint_for_end_A);
    midpoints.emplace_back(midpoint_for_start_A);

    return std::make_tuple(modified_convex_cop, other_A, other_B, midpoints);
}

// creates faces by connecting non convex hull points to the midpoints based on distance
// returns faces and associated ordered points
std::tuple<std::vector<std::array<size_t, 3>>, std::vector<vec3<double>>>
Mesh_Inner_Points_With_Midpoints(const std::list<vec3<double>> &left_points,
                                 const std::list<vec3<double>> &right_points,
                                 const std::vector<vec3<double>> &midpoints, const vec3<double> &pseudo_vert_offset) {
    if(midpoints.size() != 2) {
        YLOGWARN("Unable to handle " << midpoints.size() << " midpoints at this time.");
        throw std::runtime_error("Unable to handle !=2 midpoints at this time.");
    }

    // compare distances by using midpoints on the same plane
    auto midpoint1_flattened = midpoints[0] - pseudo_vert_offset;
    auto midpoint2_flattened = midpoints[1] - pseudo_vert_offset;

    int midpoint1_position = left_points.size() + right_points.size();
    int midpoint2_position = midpoint1_position + 1;

    std::vector<std::array<size_t, 3>> faces;

    // adds face
    // iterators must be of same vector
    const auto add_points_with_midpoint =
        [&](std::list<vec3<double>>::iterator point1_it, std::list<vec3<double>>::iterator point2_it,
            std::list<vec3<double>>::iterator beg_it, const int offset, const int midpoint_index) -> void {
        const auto v_a = static_cast<size_t>(offset + std::distance(beg_it, point1_it));
        const auto v_b = static_cast<size_t>(offset + std::distance(beg_it, point2_it));
        const auto v_c = static_cast<size_t>(midpoint_index);
        faces.emplace_back(std::array<size_t, 3> { { v_a, v_b, v_c } });
    };

    const auto add_point_with_two_midpoints = [&](std::list<vec3<double>>::iterator point1_it,
                                                  std::list<vec3<double>>::iterator beg_it, const int offset,
                                                  const int midpoint1_index, const int midpoint2_index) -> void {
        const auto v_a = static_cast<size_t>(offset + std::distance(beg_it, point1_it));
        const auto v_b = static_cast<size_t>(midpoint1_index);
        const auto v_c = static_cast<size_t>(midpoint2_index);
        faces.emplace_back(std::array<size_t, 3> { { v_a, v_b, v_c } });
    };

    const auto create_faces_with_points = [&](std::list<vec3<double>> points, const int offset) -> void {
        bool switched         = false;
        auto closer_midpoint  = midpoint1_flattened;
        auto closer_position  = midpoint1_position;
        auto further_midpoint = midpoint2_flattened;
        auto further_position = midpoint2_position;

        if(points.begin()->sq_dist(closer_midpoint) > points.begin()->sq_dist(further_midpoint)) {
            std::swap(closer_midpoint, further_midpoint);
            std::swap(closer_position, further_position);
        }

        for(auto it = std::next(points.begin()); it != points.end(); it++) {
            // assumes once switched, will not go back
            auto point = *it;
            if(point.sq_dist(closer_midpoint) < point.sq_dist(further_midpoint) && !switched) {
                // create face with prev point and closer_midpoint
                add_points_with_midpoint(it, std::prev(it), points.begin(), offset, closer_position);
            } else {
                if(!switched) {
                    // connect to both points (two faces)
                    add_points_with_midpoint(it, std::prev(it), points.begin(), offset, closer_position);
                    add_point_with_two_midpoints(it, points.begin(), offset, closer_position, further_position);
                    switched = true;
                } else {
                    // connect to prev point and further_midpoint
                    add_points_with_midpoint(it, std::prev(it), points.begin(), offset, further_position);
                }
            }
        }
    };

    create_faces_with_points(left_points, 0);
    create_faces_with_points(right_points, left_points.size());

    // make vector of all points (order is important for faces to be parsed correctly)
    std::vector<vec3<double>> all_points(left_points.begin(), left_points.end());
    all_points.insert(all_points.end(), right_points.begin(), right_points.end());
    all_points.emplace_back(midpoints[0]);
    all_points.emplace_back(midpoints[1]);

    return std::make_tuple(faces, all_points);
}

// meshes 2 to 1 (will need to mesh convex hull contour with other contour outside of this routine)
// returns non-convex hull faces, non-convex hull points, and convex hull contour (with midpoints)
std::tuple<std::vector<std::array<size_t, 3>>, std::vector<vec3<double>>, contour_of_points<double>>
Mesh_With_Convex_Hull_2(const std::list<std::reference_wrapper<contour_of_points<double>>> &A,
                        const vec3<double> &ortho_unit_A, const vec3<double> &pseudo_vert_offset) {
    if(A.size() != 2) {
        throw std::runtime_error("Convex hull is currently only possible for 2 contours. Aborted.");
    }
    std::vector<contour_of_points<double>> list_of_cops;
    for(auto &cop : A) {
        auto new_cop = cop.get();
        if (new_cop.points.front() == new_cop.points.back()) new_cop.points.pop_back();
        list_of_cops.emplace_back(new_cop);
    }
    auto convex_hull_cop = Contour_From_Convex_Hull_2(list_of_cops, ortho_unit_A);
    auto [modified_convex_cop, left_points, right_points, midpoints] =
        Modify_Convex_Hull(convex_hull_cop, list_of_cops, pseudo_vert_offset);
    auto [faces_from_non_convex, ordered_non_convex_points] =
        Mesh_Inner_Points_With_Midpoints(left_points.points, right_points.points, midpoints, pseudo_vert_offset);

    return std::make_tuple(faces_from_non_convex, ordered_non_convex_points, modified_convex_cop);
}