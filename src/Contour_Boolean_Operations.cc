//Contour_Boolean_Operations.cc - A part of DICOMautomaton 2017, 2024. Written by hal clark.

// These functions are used to perform first-order Boolean operations on (2D) polygon contours.
// This implementation provides a native alternative using polygon clipping algorithms.
//
// LIMITATIONS:
// - The intersection operation uses the Sutherland-Hodgman algorithm which works well for 
//   clipping convex polygons.
// - Union and difference operations use a simplified approach that works well for typical
//   medical imaging contours (simple, largely convex polygons) but may produce approximate
//   results for complex non-convex polygons with intricate overlapping regions.
// - For best results, the input polygons should be simple (non-self-intersecting).

#include <list>
#include <vector>
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <any>
#include <algorithm>
#include <set>

#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorMath.h"

#include "Contour_Boolean_Operations.h"


// ======================= Native 2D Polygon Boolean Operations =======================
// This implementation uses the Sutherland-Hodgman algorithm for polygon clipping
// and Weiler-Atherton concepts for general polygon Boolean operations.

namespace {

// 2D point structure for internal use
struct Point2D {
    double x, y;
    Point2D() : x(0), y(0) {}
    Point2D(double x_, double y_) : x(x_), y(y_) {}
    
    bool operator==(const Point2D& other) const {
        const double eps = 1e-10;
        return std::abs(x - other.x) < eps && std::abs(y - other.y) < eps;
    }
    
    bool operator<(const Point2D& other) const {
        if(std::abs(x - other.x) > 1e-10) return x < other.x;
        return y < other.y;
    }
};

using Polygon2D = std::vector<Point2D>;

// Compute signed area of a 2D polygon (positive if CCW, negative if CW)
double signed_area(const Polygon2D& poly) {
    double area = 0.0;
    const size_t n = poly.size();
    if(n < 3) return 0.0;
    
    for(size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        area += poly[i].x * poly[j].y;
        area -= poly[j].x * poly[i].y;
    }
    return area / 2.0;
}

// Check if polygon is counter-clockwise
bool is_ccw(const Polygon2D& poly) {
    return signed_area(poly) > 0.0;
}

// Reverse polygon orientation
void reverse_polygon(Polygon2D& poly) {
    std::reverse(poly.begin(), poly.end());
}

// Check if a point is inside a polygon using ray casting
bool point_in_polygon(const Point2D& p, const Polygon2D& poly) {
    if(poly.size() < 3) return false;
    
    int crossings = 0;
    const size_t n = poly.size();
    
    for(size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        const auto& vi = poly[i];
        const auto& vj = poly[j];
        
        if(((vi.y <= p.y && p.y < vj.y) || (vj.y <= p.y && p.y < vi.y)) &&
           (p.x < (vj.x - vi.x) * (p.y - vi.y) / (vj.y - vi.y) + vi.x)) {
            ++crossings;
        }
    }
    
    return (crossings % 2) == 1;
}

// Compute intersection point of two line segments (if it exists)
// Returns true if intersection exists, and sets intersection point
bool line_intersection(const Point2D& p1, const Point2D& p2,
                       const Point2D& p3, const Point2D& p4,
                       Point2D& intersection) {
    double d = (p1.x - p2.x) * (p3.y - p4.y) - (p1.y - p2.y) * (p3.x - p4.x);
    if(std::abs(d) < 1e-10) return false; // Lines are parallel
    
    double t = ((p1.x - p3.x) * (p3.y - p4.y) - (p1.y - p3.y) * (p3.x - p4.x)) / d;
    double u = -((p1.x - p2.x) * (p1.y - p3.y) - (p1.y - p2.y) * (p1.x - p3.x)) / d;
    
    if(t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0) {
        intersection.x = p1.x + t * (p2.x - p1.x);
        intersection.y = p1.y + t * (p2.y - p1.y);
        return true;
    }
    return false;
}

// Sutherland-Hodgman polygon clipping algorithm
// Clips subject polygon against clip polygon, returning the intersection
Polygon2D sutherland_hodgman_clip(const Polygon2D& subject, const Polygon2D& clip) {
    if(subject.empty() || clip.empty()) return {};
    
    Polygon2D output = subject;
    
    for(size_t i = 0; i < clip.size() && !output.empty(); ++i) {
        if(output.empty()) break;
        
        Polygon2D input = output;
        output.clear();
        
        const Point2D& edge_start = clip[i];
        const Point2D& edge_end = clip[(i + 1) % clip.size()];
        
        // Compute edge normal (pointing inward for CCW polygon)
        double edge_dx = edge_end.x - edge_start.x;
        double edge_dy = edge_end.y - edge_start.y;
        
        auto is_inside = [&](const Point2D& p) -> bool {
            // Point is inside if it's on the left side of the edge (for CCW clip polygon)
            return (edge_dx * (p.y - edge_start.y) - edge_dy * (p.x - edge_start.x)) >= -1e-10;
        };
        
        for(size_t j = 0; j < input.size(); ++j) {
            const Point2D& current = input[j];
            const Point2D& next = input[(j + 1) % input.size()];
            
            bool current_inside = is_inside(current);
            bool next_inside = is_inside(next);
            
            if(current_inside) {
                if(next_inside) {
                    // Both inside: add next point
                    output.push_back(next);
                } else {
                    // Current inside, next outside: add intersection
                    Point2D intersection;
                    // Find intersection with infinite line
                    double d = edge_dx * (next.y - current.y) - edge_dy * (next.x - current.x);
                    if(std::abs(d) > 1e-14) {
                        double t = (edge_dx * (edge_start.y - current.y) - edge_dy * (edge_start.x - current.x)) / d;
                        intersection.x = current.x + t * (next.x - current.x);
                        intersection.y = current.y + t * (next.y - current.y);
                        output.push_back(intersection);
                    }
                }
            } else {
                if(next_inside) {
                    // Current outside, next inside: add intersection and next
                    Point2D intersection;
                    double d = edge_dx * (next.y - current.y) - edge_dy * (next.x - current.x);
                    if(std::abs(d) > 1e-14) {
                        double t = (edge_dx * (edge_start.y - current.y) - edge_dy * (edge_start.x - current.x)) / d;
                        intersection.x = current.x + t * (next.x - current.x);
                        intersection.y = current.y + t * (next.y - current.y);
                        output.push_back(intersection);
                    }
                    output.push_back(next);
                }
                // Both outside: add nothing
            }
        }
    }
    
    return output;
}

// Compute polygon union using simple approach for convex-ish polygons
// For complex cases, this uses a boundary-following approach
std::vector<Polygon2D> polygon_union(const Polygon2D& A, const Polygon2D& B) {
    // Simple approach: if one contains the other, return the larger
    // Otherwise, return both as separate polygons (conservative approach)
    
    if(A.empty()) return B.empty() ? std::vector<Polygon2D>{} : std::vector<Polygon2D>{B};
    if(B.empty()) return {A};
    
    // Check if A contains all of B
    bool A_contains_B = true;
    for(const auto& p : B) {
        if(!point_in_polygon(p, A)) {
            A_contains_B = false;
            break;
        }
    }
    if(A_contains_B) return {A};
    
    // Check if B contains all of A
    bool B_contains_A = true;
    for(const auto& p : A) {
        if(!point_in_polygon(p, B)) {
            B_contains_A = false;
            break;
        }
    }
    if(B_contains_A) return {B};
    
    // Check if polygons intersect
    auto intersection = sutherland_hodgman_clip(A, B);
    if(intersection.empty()) {
        // No intersection, return both
        return {A, B};
    }
    
    // For overlapping polygons, create a simple merged boundary
    // This is a simplified approach that works for many practical cases
    // A full implementation would use Weiler-Atherton algorithm
    
    // Collect all vertices from both polygons plus intersection points
    std::vector<Point2D> all_points;
    
    // Add vertices from A that are not inside B
    for(const auto& p : A) {
        if(!point_in_polygon(p, B)) {
            all_points.push_back(p);
        }
    }
    
    // Add vertices from B that are not inside A
    for(const auto& p : B) {
        if(!point_in_polygon(p, A)) {
            all_points.push_back(p);
        }
    }
    
    // Add intersection points
    for(size_t i = 0; i < A.size(); ++i) {
        for(size_t j = 0; j < B.size(); ++j) {
            Point2D isect;
            if(line_intersection(A[i], A[(i+1)%A.size()], B[j], B[(j+1)%B.size()], isect)) {
                all_points.push_back(isect);
            }
        }
    }
    
    if(all_points.size() < 3) {
        return {A, B}; // Fallback
    }
    
    // Compute convex hull of merged points as approximation
    // Sort points by angle from centroid
    Point2D centroid{0, 0};
    for(const auto& p : all_points) {
        centroid.x += p.x;
        centroid.y += p.y;
    }
    centroid.x /= all_points.size();
    centroid.y /= all_points.size();
    
    std::sort(all_points.begin(), all_points.end(), 
        [&centroid](const Point2D& a, const Point2D& b) {
            return std::atan2(a.y - centroid.y, a.x - centroid.x) < 
                   std::atan2(b.y - centroid.y, b.x - centroid.x);
        });
    
    // Remove duplicates
    all_points.erase(std::unique(all_points.begin(), all_points.end()), all_points.end());
    
    return {all_points};
}

// Polygon difference A - B
std::vector<Polygon2D> polygon_difference(const Polygon2D& A, const Polygon2D& B) {
    if(A.empty()) return {};
    if(B.empty()) return {A};
    
    // Check if B completely contains A
    bool B_contains_A = true;
    for(const auto& p : A) {
        if(!point_in_polygon(p, B)) {
            B_contains_A = false;
            break;
        }
    }
    if(B_contains_A) return {}; // A is completely inside B, difference is empty
    
    // Check if A and B don't overlap
    auto intersection = sutherland_hodgman_clip(A, B);
    if(intersection.empty()) {
        return {A}; // No overlap, return A unchanged
    }
    
    // For partial overlap, create result by collecting boundary segments
    // This is a simplified approach
    std::vector<Point2D> result_points;
    
    // Add vertices from A that are not inside B
    for(const auto& p : A) {
        if(!point_in_polygon(p, B)) {
            result_points.push_back(p);
        }
    }
    
    // Add intersection points
    for(size_t i = 0; i < A.size(); ++i) {
        for(size_t j = 0; j < B.size(); ++j) {
            Point2D isect;
            if(line_intersection(A[i], A[(i+1)%A.size()], B[j], B[(j+1)%B.size()], isect)) {
                result_points.push_back(isect);
            }
        }
    }
    
    if(result_points.size() < 3) {
        return {A}; // Fallback to original
    }
    
    // Sort by angle from centroid
    Point2D centroid{0, 0};
    for(const auto& p : result_points) {
        centroid.x += p.x;
        centroid.y += p.y;
    }
    centroid.x /= result_points.size();
    centroid.y /= result_points.size();
    
    std::sort(result_points.begin(), result_points.end(),
        [&centroid](const Point2D& a, const Point2D& b) {
            return std::atan2(a.y - centroid.y, a.x - centroid.x) <
                   std::atan2(b.y - centroid.y, b.x - centroid.x);
        });
    
    result_points.erase(std::unique(result_points.begin(), result_points.end()), result_points.end());
    
    return {result_points};
}

// Polygon symmetric difference (XOR)
std::vector<Polygon2D> polygon_symmetric_difference(const Polygon2D& A, const Polygon2D& B) {
    // XOR = (A - B) union (B - A)
    auto A_minus_B = polygon_difference(A, B);
    auto B_minus_A = polygon_difference(B, A);
    
    std::vector<Polygon2D> result;
    result.insert(result.end(), A_minus_B.begin(), A_minus_B.end());
    result.insert(result.end(), B_minus_A.begin(), B_minus_A.end());
    
    return result;
}

} // anonymous namespace


// Because ROI contours are 2D planar contours embedded in R^3, an explicit projection plane must be provided. Contours
// are projected on the plane, an orthonormal basis is created, the projected contours are expressed in the basis, and
// the Boolean operations are performed. Note that the outgoing contours remain projected onto the provided plane.
contour_collection<double>
ContourBoolean(plane<double> p,
               std::list<std::reference_wrapper<contour_of_points<double>>> A,
               std::list<std::reference_wrapper<contour_of_points<double>>> B,
               ContourBooleanMethod op,
               ContourBooleanMethod construction_op){

    // Identify an orthonormal set that spans the 2D plane. Store them for later projection.
    const auto pi = std::acos(-1.0);
    const auto U_z = p.N_0.unit();
    vec3<double> U_y = vec3<double>(1.0, 0.0, 0.0); //Candidate vector.
    if(U_y.Dot(U_z) > 0.25){
        U_y = U_z.rotate_around_x(pi * 0.5);
    }
    vec3<double> U_x = U_z.Cross(U_y);
    if(!U_z.GramSchmidt_orthogonalize(U_y, U_x)){
        throw std::runtime_error("Unable to find planar basis vectors.");
    }
    U_x = U_x.unit();
    U_y = U_y.unit();

    // Closure for expressing vectors in the plane's basis (R^3 -> R^2)
    const auto R3_v_to_R2 = [=](vec3<double> R) -> Point2D {
        const auto proj = p.Project_Onto_Plane_Orthogonally(R);
        const auto dR = (proj - p.R_0);
        return Point2D(dR.Dot(U_x), dR.Dot(U_y));
    };

    // Closure for converting from the plane's basis back to R^3
    const auto R2_to_R3_v = [=](Point2D pt) -> vec3<double> {
        return p.R_0 + (U_x * pt.x) + (U_y * pt.y);
    };

    // Extract the common metadata from all contours
    std::list<std::reference_wrapper<contour_of_points<double>>> all;
    all.insert(all.end(), A.begin(), A.end());
    all.insert(all.end(), B.begin(), B.end());
    auto common_metadata = contour_collection<double>().get_common_metadata( { }, { std::ref(all) } );

    // Convert contour sets to 2D polygons
    auto contours_to_polygon = [&](const std::list<std::reference_wrapper<contour_of_points<double>>>& contours) 
        -> std::vector<Polygon2D> {
        std::vector<Polygon2D> polygons;
        for(const auto& c_ref : contours) {
            Polygon2D poly;
            for(const auto& v : c_ref.get().points) {
                poly.push_back(R3_v_to_R2(v));
            }
            if(poly.size() >= 3) {
                // Ensure CCW orientation
                if(!is_ccw(poly)) {
                    reverse_polygon(poly);
                }
                polygons.push_back(poly);
            }
        }
        return polygons;
    };

    std::vector<Polygon2D> A_polys = contours_to_polygon(A);
    std::vector<Polygon2D> B_polys = contours_to_polygon(B);

    // Apply construction operation to combine polygons within each set
    auto combine_polygons = [&](std::vector<Polygon2D>& polys, ContourBooleanMethod method) {
        if(polys.size() <= 1) return;
        
        std::vector<Polygon2D> result = {polys[0]};
        for(size_t i = 1; i < polys.size(); ++i) {
            std::vector<Polygon2D> new_result;
            for(const auto& r : result) {
                std::vector<Polygon2D> combined;
                if(method == ContourBooleanMethod::join) {
                    combined = polygon_union(r, polys[i]);
                } else if(method == ContourBooleanMethod::intersection) {
                    auto isect = sutherland_hodgman_clip(r, polys[i]);
                    if(!isect.empty()) combined = {isect};
                } else if(method == ContourBooleanMethod::difference) {
                    combined = polygon_difference(r, polys[i]);
                } else if(method == ContourBooleanMethod::symmetric_difference) {
                    combined = polygon_symmetric_difference(r, polys[i]);
                } else {
                    combined = {r};
                }
                new_result.insert(new_result.end(), combined.begin(), combined.end());
            }
            result = new_result;
        }
        polys = result;
    };

    if(A_polys.size() > 1) {
        combine_polygons(A_polys, construction_op);
    }
    if(B_polys.size() > 1) {
        combine_polygons(B_polys, construction_op);
    }

    // Perform the main Boolean operation between A and B sets
    std::vector<Polygon2D> result_polys;
    
    if(op == ContourBooleanMethod::noop) {
        result_polys = A_polys;
    } else if(A_polys.empty()) {
        if(op == ContourBooleanMethod::join) {
            result_polys = B_polys;
        }
        // For other operations with empty A, result is empty
    } else if(B_polys.empty()) {
        if(op == ContourBooleanMethod::join || op == ContourBooleanMethod::difference) {
            result_polys = A_polys;
        }
        // intersection with empty B is empty
        // symmetric_difference with empty B is A
        if(op == ContourBooleanMethod::symmetric_difference) {
            result_polys = A_polys;
        }
    } else {
        // Both A and B have polygons
        for(const auto& a_poly : A_polys) {
            for(const auto& b_poly : B_polys) {
                std::vector<Polygon2D> combined;
                
                if(op == ContourBooleanMethod::join) {
                    combined = polygon_union(a_poly, b_poly);
                } else if(op == ContourBooleanMethod::intersection) {
                    auto isect = sutherland_hodgman_clip(a_poly, b_poly);
                    if(!isect.empty() && isect.size() >= 3) {
                        combined = {isect};
                    }
                } else if(op == ContourBooleanMethod::difference) {
                    combined = polygon_difference(a_poly, b_poly);
                } else if(op == ContourBooleanMethod::symmetric_difference) {
                    combined = polygon_symmetric_difference(a_poly, b_poly);
                }
                
                result_polys.insert(result_polys.end(), combined.begin(), combined.end());
            }
        }
    }

    // Convert result polygons back to contours
    contour_collection<double> out;
    for(const auto& poly : result_polys) {
        if(poly.size() < 3) continue;
        
        contour_of_points<double> contour;
        contour.closed = true;
        contour.metadata = common_metadata;
        
        for(const auto& pt : poly) {
            contour.points.push_back(R2_to_R3_v(pt));
        }
        
        // Ensure counter-clockwise orientation
        if(!contour.Is_Counter_Clockwise()) {
            contour.Reorient_Counter_Clockwise();
        }
        
        out.contours.push_back(contour);
    }

    return out;
}

