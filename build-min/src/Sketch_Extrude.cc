// Sketch_Extrude.cc - Extrusion helpers for planar sketch surface meshes.

#include "Sketch_Extrude.h"
#include "Sketch.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "YgorLog.h"
#include "YgorMathConstrainedDelaunay.h"
#include "YgorMathMonotoneDecomposition.h"

namespace {

constexpr double extrude_min_epsilon = 1.0E-9;
constexpr double interior_probe_relative_epsilon = 1.0E-6;
constexpr double interior_probe_absolute_epsilon = 1.0E-8;

static void store_error(std::string *error_message,
                        const std::string &message){
    if(error_message != nullptr) *error_message = message;
}

static bool points_coincident(const vec3<double> &a,
                              const vec3<double> &b,
                              double tolerance = 1.0E-6){
    return a.distance(b) <= tolerance;
}

static double extrusion_scale_factor(double reference_half_extent,
                                     double extrusion_length,
                                     double angle_degrees){
    if(reference_half_extent <= extrude_min_epsilon) return 1.0;
    if(!std::isfinite(extrusion_length) || !std::isfinite(angle_degrees)){
        return std::numeric_limits<double>::quiet_NaN();
    }
    if(std::abs(angle_degrees) <= extrude_min_epsilon) return 1.0;
    const auto pi = std::acos(-1.0);
    const auto angle_radians = angle_degrees * (pi / 180.0);
    const auto delta = std::tan(angle_radians) * std::abs(extrusion_length);
    return (reference_half_extent + delta) / reference_half_extent;
}

static Sketch::projection_t scale_projection_about(const Sketch::projection_t &point,
                                                   const Sketch::projection_t &centre,
                                                   double scale){
    return {
        centre.u + ((point.u - centre.u) * scale),
        centre.v + ((point.v - centre.v) * scale)
    };
}

static vec2<double> to_vec2(const Sketch::projection_t &point){
    return vec2<double>(point.u, point.v);
}

static Sketch::projection_t to_projection(const vec2<double> &point){
    return Sketch::projection_t{ point.x, point.y };
}

static long double signed_polygon_area(const std::vector<vec2<double>> &points){
    if(points.size() < 3U) return 0.0L;
    long double twice_area = 0.0L;
    for(std::size_t i = 0U; i < points.size(); ++i){
        const auto &a = points.at(i);
        const auto &b = points.at((i + 1U) % points.size());
        const auto ax = static_cast<long double>(a.x);
        const auto ay = static_cast<long double>(a.y);
        const auto bx = static_cast<long double>(b.x);
        const auto by = static_cast<long double>(b.y);
        twice_area += (ax * by) - (bx * ay);
    }
    return twice_area * 0.5;
}

static vec2<double> compute_interior_probe(const std::vector<vec2<double>> &loop){
    if(loop.empty()) return {};

    double min_x = loop.front().x;
    double max_x = loop.front().x;
    double min_y = loop.front().y;
    double max_y = loop.front().y;
    for(const auto &point : loop){
        min_x = std::min(min_x, point.x);
        max_x = std::max(max_x, point.x);
        min_y = std::min(min_y, point.y);
        max_y = std::max(max_y, point.y);
    }
    const auto span = std::max(max_x - min_x, max_y - min_y);
    const auto probe_offset = std::max(span * interior_probe_relative_epsilon,
                                       interior_probe_absolute_epsilon);
    const auto area = signed_polygon_area(loop);
    if(area == 0.0L) return loop.front();
    const int orientation_sign = (area >= 0.0L) ? 1 : -1;

    for(std::size_t i = 0U; i < loop.size(); ++i){
        const auto &a = loop.at(i);
        const auto &b = loop.at((i + 1U) % loop.size());
        const auto edge = b - a;
        const auto edge_length = edge.length();
        if(edge_length <= extrude_min_epsilon) continue;

        vec2<double> inward;
        if(orientation_sign >= 0){
            inward = vec2<double>(-edge.y, edge.x);
        }else{
            inward = vec2<double>(edge.y, -edge.x);
        }
        inward /= inward.length();
        return ((a + b) * 0.5) + inward * probe_offset;
    }

    return loop.front();
}

struct normalized_loop_t {
    std::vector<vec2<double>> points;
    vec2<double> interior_probe;
    std::size_t depth = 0U;
};

static void append_triangle(fv_surface_mesh<double, uint64_t> &mesh,
                            uint64_t a,
                            uint64_t b,
                            uint64_t c){
    if((a == b) || (b == c) || (a == c)) return;
    mesh.faces.emplace_back(std::vector<uint64_t>{ a, b, c });
}

// Post-process an extruded surface mesh to remove internal faces and ensure
// consistent normal orientation. Internal faces are those whose edges are
// shared by 3+ faces (non-manifold T-junctions). Among such faces, we keep
// the two with the most distinct normals (i.e., the outer surface pair) and
// discard the rest. After cleanup, face normals are re-oriented to be
// consistent across the mesh, pointing outward from the interior.
static void remove_internal_mesh_faces(fv_surface_mesh<double, uint64_t> &mesh){
    struct edge_key_t {
        uint64_t a, b;
        bool operator<(const edge_key_t &other) const {
            return std::tie(a, b) < std::tie(other.a, other.b);
        }
    };

    std::map<edge_key_t, std::vector<std::size_t>> edge_faces;
    for(std::size_t fi = 0U; fi < mesh.faces.size(); ++fi){
        const auto &f = mesh.faces[fi];
        if(f.size() != 3U) continue;
        for(int k = 0; k < 3; ++k){
            edge_key_t ek{ f[k], f[(k+1) % 3] };
            if(ek.a > ek.b) std::swap(ek.a, ek.b);
            edge_faces[ek].push_back(fi);
        }
    }

    auto face_normal = [&](std::size_t fi) -> vec3<double> {
        const auto &f = mesh.faces[fi];
        const auto &v0 = mesh.vertices[ f[0] ];
        const auto &v1 = mesh.vertices[ f[1] ];
        const auto &v2 = mesh.vertices[ f[2] ];
        return (v1 - v0).Cross(v2 - v0);
    };

    std::vector<bool> keep(mesh.faces.size(), true);

    for(const auto &[ek, fis] : edge_faces){
        if(fis.size() < 3U) continue;
        std::size_t best_i = 0U, best_j = 1U;
        double min_dot = 1.0;
        for(std::size_t ii = 0U; ii < fis.size(); ++ii){
            const auto ni = face_normal(fis[ii]).unit();
            for(std::size_t jj = ii + 1U; jj < fis.size(); ++jj){
                const auto nj = face_normal(fis[jj]).unit();
                const auto d = std::abs(ni.Dot(nj));
                if(d < min_dot){
                    min_dot = d;
                    best_i = ii;
                    best_j = jj;
                }
            }
        }
        for(std::size_t ii = 0U; ii < fis.size(); ++ii){
            if((ii != best_i) && (ii != best_j)){
                keep[fis[ii]] = false;
            }
        }
    }

    std::vector<std::vector<uint64_t>> new_faces;
    new_faces.reserve(mesh.faces.size());
    for(std::size_t fi = 0U; fi < mesh.faces.size(); ++fi){
        if(keep[fi]){
            new_faces.push_back(std::move(mesh.faces[fi]));
        }
    }
    mesh.faces.swap(new_faces);

    if(mesh.faces.empty()) return;

    edge_faces.clear();
    for(std::size_t fi = 0U; fi < mesh.faces.size(); ++fi){
        const auto &f = mesh.faces[fi];
        if(f.size() != 3U) continue;
        for(int k = 0; k < 3; ++k){
            edge_key_t ek{ f[k], f[(k+1) % 3] };
            if(ek.a > ek.b) std::swap(ek.a, ek.b);
            edge_faces[ek].push_back(fi);
        }
    }

    std::vector<int8_t> orientation(mesh.faces.size(), 0);
    orientation[0] = 1;

    std::vector<std::vector<std::size_t>> adj(mesh.faces.size());
    for(const auto &[ek, fis] : edge_faces){
        if(fis.size() == 2U){
            adj[fis[0]].push_back(fis[1]);
            adj[fis[1]].push_back(fis[0]);
        }
    }

    std::vector<std::size_t> stack;
    stack.reserve(mesh.faces.size());
    stack.push_back(0U);
    while(!stack.empty()){
        const auto fi = stack.back();
        stack.pop_back();
        const auto &f = mesh.faces[fi];
        for(const auto nfi : adj[fi]){
            if(orientation[nfi] != 0) continue;
            const auto &nf = mesh.faces[nfi];
            bool found = false;
            for(int fk = 0; fk < 3 && !found; ++fk){
                auto fv0 = f[fk];
                auto fv1 = f[(fk+1) % 3];
                if(fv0 > fv1) std::swap(fv0, fv1);
                for(int nk = 0; nk < 3; ++nk){
                    auto nv0 = nf[nk];
                    auto nv1 = nf[(nk+1) % 3];
                    if(nv0 > nv1) std::swap(nv0, nv1);
                    if(fv0 == nv0 && fv1 == nv1){
                        const bool same_dir = (f[fk] == nf[nk] && f[(fk+1) % 3] == nf[(nk+1) % 3]);
                        orientation[nfi] = same_dir ? (-orientation[fi]) : orientation[fi];
                        stack.push_back(nfi);
                        found = true;
                        break;
                    }
                }
            }
        }
    }

    for(std::size_t fi = 0U; fi < mesh.faces.size(); ++fi){
        if(orientation[fi] < 0){
            std::reverse(std::begin(mesh.faces[fi]), std::end(mesh.faces[fi]));
        }
    }

    mesh.recreate_involved_face_index();
}

struct extruded_path_t {
    std::vector<vec3<double>> points;
    bool closed = false;
};

static std::vector<vec3<double>> deduplicate_polyline_points(const std::vector<vec3<double>> &polyline,
                                                             bool *closed_out = nullptr){
    std::vector<vec3<double>> out;
    out.reserve(polyline.size());
    for(const auto &point : polyline){
        if(out.empty() || !points_coincident(out.back(), point)){
            out.push_back(point);
        }
    }

    bool closed = false;
    if((out.size() >= 2U) && points_coincident(out.front(), out.back())){
        out.pop_back();
        closed = true;
    }
    if(closed_out != nullptr) *closed_out = closed;
    return out;
}

static std::vector<Sketch::projection_t> project_polyline(const Sketch &sketch,
                                                          const std::vector<vec3<double>> &polyline){
    std::vector<Sketch::projection_t> out;
    out.reserve(polyline.size());
    for(const auto &point : polyline){
        out.push_back(sketch.project(point));
    }
    return out;
}

static std::vector<vec2<double>> project_polyline_vec2(const Sketch &sketch,
                                                       const std::vector<vec3<double>> &polyline){
    std::vector<vec2<double>> out;
    out.reserve(polyline.size());
    for(const auto &point : polyline){
        out.push_back(to_vec2(sketch.project(point)));
    }
    return out;
}

static Sketch::bounding_box_t projected_path_bounds(const Sketch &sketch,
                                                    const std::vector<extruded_path_t> &paths){
    Sketch::bounding_box_t bounds;
    for(const auto &path : paths){
        for(const auto &point : path.points){
            const auto projected = sketch.project(point);
            bounds.min.u = std::min(bounds.min.u, projected.u);
            bounds.min.v = std::min(bounds.min.v, projected.v);
            bounds.max.u = std::max(bounds.max.u, projected.u);
            bounds.max.v = std::max(bounds.max.v, projected.v);
        }
    }
    return bounds;
}

static int signed_triangle_orientation(const vec2<double> &a,
                                       const vec2<double> &b,
                                       const vec2<double> &c){
    return orient_sign(a, b, c);
}

static bool point_on_segment(const vec2<double> &p,
                             const vec2<double> &a,
                             const vec2<double> &b){
    return point_on_closed_segment(p, a, b);
}

static bool point_in_polygon(const vec2<double> &p,
                             const std::vector<vec2<double>> &polygon){
    if(polygon.size() < 3U) return false;
    bool inside = false;
    constexpr double tolerance = 1.0E-8;
    for(std::size_t i = 0U, j = polygon.size() - 1U; i < polygon.size(); j = i++){
        const auto &a = polygon.at(j);
        const auto &b = polygon.at(i);
        if(point_on_segment(p, a, b)) return true;
        if(std::abs(b.y - a.y) <= tolerance) continue;
        const bool intersects = ((a.y > p.y) != (b.y > p.y))
                             && (p.x < ((b.x - a.x) * (p.y - a.y) / (b.y - a.y)) + a.x);
        if(intersects) inside = !inside;
    }
    return inside;
}

static int winding_number(const vec2<double> &p,
                          const std::vector<vec2<double>> &polygon){
    if(polygon.size() < 3U) return 0;

    int winding = 0;
    const auto orientation = (signed_polygon_area(polygon) >= 0.0L) ? 1 : -1;
    for(std::size_t i = 0U; i < polygon.size(); ++i){
        const auto &a = polygon.at(i);
        const auto &b = polygon.at((i + 1U) % polygon.size());
        if(point_on_segment(p, a, b)) return orientation;

        if(a.y <= p.y){
            if((b.y > p.y) && (orient_sign(a, b, p) > 0)){
                ++winding;
            }
        }else if((b.y <= p.y) && (orient_sign(a, b, p) < 0)){
            --winding;
        }
    }
    return winding;
}

static std::vector<normalized_loop_t>
normalize_closed_loops(const std::vector<std::vector<vec2<double>>> &raw_loops){
    std::vector<normalized_loop_t> loops;
    loops.reserve(raw_loops.size());
    for(const auto &raw_loop : raw_loops){
        if(raw_loop.size() < 3U) continue;
        if(std::abs(signed_polygon_area(raw_loop)) <= extrude_min_epsilon) continue;
        loops.push_back(normalized_loop_t{ raw_loop, compute_interior_probe(raw_loop), 0U });
    }

    for(std::size_t i = 0U; i < loops.size(); ++i){
        for(std::size_t j = 0U; j < loops.size(); ++j){
            if(i == j) continue;
            if(point_in_polygon(loops.at(i).interior_probe, loops.at(j).points)){
                ++loops.at(i).depth;
            }
        }

        const bool should_be_ccw = ((loops.at(i).depth % 2U) == 0U);
        const bool is_ccw = (signed_polygon_area(loops.at(i).points) >= 0.0L);
        if(should_be_ccw != is_ccw){
            std::reverse(std::begin(loops.at(i).points), std::end(loops.at(i).points));
            loops.at(i).interior_probe = compute_interior_probe(loops.at(i).points);
        }
    }
    return loops;
}

static bool point_in_normalized_loops(const vec2<double> &p,
                                      const std::vector<normalized_loop_t> &loops){
    int winding = 0;
    for(const auto &loop : loops){
        winding += winding_number(p, loop.points);
    }
    return (winding != 0);
}

static bool merge_extruded_paths(extruded_path_t &lhs,
                                 extruded_path_t &rhs){
    if(lhs.closed || rhs.closed || (lhs.points.size() < 2U) || (rhs.points.size() < 2U)){
        return false;
    }

    if(points_coincident(lhs.points.back(), rhs.points.front())){
        lhs.points.insert(std::end(lhs.points), std::next(std::begin(rhs.points)), std::end(rhs.points));
    }else if(points_coincident(lhs.points.back(), rhs.points.back())){
        std::reverse(std::begin(rhs.points), std::end(rhs.points));
        lhs.points.insert(std::end(lhs.points), std::next(std::begin(rhs.points)), std::end(rhs.points));
    }else if(points_coincident(lhs.points.front(), rhs.points.back())){
        lhs.points.insert(std::begin(lhs.points), std::begin(rhs.points), std::prev(std::end(rhs.points)));
    }else if(points_coincident(lhs.points.front(), rhs.points.front())){
        std::reverse(std::begin(rhs.points), std::end(rhs.points));
        lhs.points.insert(std::begin(lhs.points), std::begin(rhs.points), std::prev(std::end(rhs.points)));
    }else{
        return false;
    }

    lhs.points = deduplicate_polyline_points(lhs.points, &lhs.closed);
    rhs.points.clear();
    rhs.closed = false;
    return true;
}

static std::size_t compute_discretization_segments(const Sketch &sketch,
                                                   Sketch::primitive_index_t idx,
                                                   double max_error,
                                                   std::size_t max_segments){
    const auto *primitive = sketch.primitive(idx);
    if(!primitive || max_error <= 0.0) return max_segments;

    switch(primitive->kind()){
        case Sketch::primitive_kind_t::vertex:
        case Sketch::primitive_kind_t::line:
            return 1U;
        case Sketch::primitive_kind_t::circle: {
            const auto *circle = dynamic_cast<const Sketch::circle_primitive_t*>(primitive);
            if(!circle || circle->radius <= 0.0) return max_segments;
            const auto n = static_cast<std::size_t>(std::ceil(2.0 * std::acos(-1.0) * std::sqrt(circle->radius / (8.0 * max_error))));
            return std::clamp(n, static_cast<std::size_t>(16U), max_segments);
        }
        case Sketch::primitive_kind_t::arc: {
            const auto *arc = dynamic_cast<const Sketch::arc_primitive_t*>(primitive);
            if(!arc || arc->radius <= 0.0) return max_segments;
            auto sweep = arc->stop_angle - arc->start_angle;
            if(sweep <= 0.0) sweep += 2.0 * std::acos(-1.0);
            const auto n = static_cast<std::size_t>(std::ceil(sweep * std::sqrt(arc->radius / (8.0 * max_error))));
            return std::clamp(n, static_cast<std::size_t>(16U), max_segments);
        }
        case Sketch::primitive_kind_t::bezier: {
            const auto samples = sketch.sample_primitive(idx, 48U);
            if(samples.size() < 2U) return max_segments;
            double max_dist_sq = 0.0;
            const auto &first = samples.front();
            for(const auto &p : samples){
                max_dist_sq = std::max(max_dist_sq, first.sq_dist(p));
            }
            const auto effective_radius = std::max(std::sqrt(max_dist_sq) * 0.5, max_error);
            const auto n = static_cast<std::size_t>(std::ceil(std::acos(-1.0) * std::sqrt(effective_radius / (8.0 * max_error))));
            return std::clamp(n, static_cast<std::size_t>(24U), max_segments);
        }
    }
    return max_segments;
}

static std::vector<extruded_path_t> collect_extruded_paths(const Sketch &sketch,
                                                           std::size_t curve_segments){
    std::vector<extruded_path_t> paths;
    for(std::size_t primitive_idx = 0U; primitive_idx < sketch.primitive_count(); ++primitive_idx){
        const auto *primitive_ptr = sketch.primitive(primitive_idx);
        if(primitive_ptr == nullptr) continue;
        if(primitive_ptr->kind() == Sketch::primitive_kind_t::vertex) continue;

        const auto sampled_points = sketch.sample_primitive(primitive_idx, curve_segments);
        if(sampled_points.size() < 2U) continue;

        bool closed = (primitive_ptr->kind() == Sketch::primitive_kind_t::circle);
        auto points = deduplicate_polyline_points(sampled_points, &closed);
        if(points.size() < 2U) continue;
        if(closed && (points.size() < 3U)) continue;
        paths.push_back(extruded_path_t{ std::move(points), closed });
    }

    bool merged_any = true;
    while(merged_any){
        merged_any = false;
        for(std::size_t i = 0U; i < paths.size(); ++i){
            if(paths.at(i).closed || paths.at(i).points.empty()) continue;
            for(std::size_t j = i + 1U; j < paths.size(); ++j){
                if(paths.at(j).closed || paths.at(j).points.empty()) continue;
                if(merge_extruded_paths(paths.at(i), paths.at(j))){
                    merged_any = true;
                    break;
                }
            }
            if(merged_any) break;
        }
    }

    paths.erase(std::remove_if(std::begin(paths), std::end(paths), [](const auto &path){
        return path.points.size() < 2U;
    }), std::end(paths));
    return paths;
}

static void append_extruded_polyline_sides(const Sketch &sketch,
                                           const extruded_path_t &path,
                                           const Sketch::projection_t &scale_centre,
                                           double near_offset,
                                           double far_offset,
                                           double near_scale,
                                           double far_scale,
                                           fv_surface_mesh<double, uint64_t> &mesh){
    if(path.points.size() < 2U) return;
    if(path.closed && (path.points.size() < 3U)) return;

    const auto normal = sketch.plane().normal();
    const auto projected_points = project_polyline(sketch, path.points);
    const uint64_t base_vertex = static_cast<uint64_t>(mesh.vertices.size());
    for(const auto &projected_point : projected_points){
        const auto near_point = sketch.lift(scale_projection_about(projected_point, scale_centre, near_scale));
        const auto far_point = sketch.lift(scale_projection_about(projected_point, scale_centre, far_scale));
        mesh.vertices.emplace_back(near_point + normal * near_offset);
        mesh.vertices.emplace_back(far_point + normal * far_offset);
    }

    bool closed_ccw = true;
    if(path.closed){
        closed_ccw = (signed_polygon_area(project_polyline_vec2(sketch, path.points)) >= 0.0L);
    }

    const auto emit_side_faces = [&](uint64_t i, uint64_t j) -> void {
        const auto near_i = base_vertex + (i * 2U);
        const auto far_i = near_i + 1U;
        const auto near_j = base_vertex + (j * 2U);
        const auto far_j = near_j + 1U;
        if(!path.closed || closed_ccw){
            append_triangle(mesh, near_i, near_j, far_j);
            append_triangle(mesh, near_i, far_j, far_i);
        }else{
            append_triangle(mesh, near_i, far_j, near_j);
            append_triangle(mesh, near_i, far_i, far_j);
        }
    };

    for(uint64_t i = 0U; (i + 1U) < static_cast<uint64_t>(path.points.size()); ++i){
        emit_side_faces(i, i + 1U);
    }
    if(path.closed){
        emit_side_faces(static_cast<uint64_t>(path.points.size() - 1U), 0U);
    }
}

static void append_extruded_end_caps(const Sketch &sketch,
                                     const std::vector<extruded_path_t> &paths,
                                     const Sketch::projection_t &scale_centre,
                                     double near_offset,
                                     double far_offset,
                                     double near_scale,
                                     double far_scale,
                                     fv_surface_mesh<double, uint64_t> &mesh,
                                     std::vector<fv_surface_mesh<double, uint64_t>> *cap_meshes){
    std::vector<std::vector<vec2<double>>> raw_closed_loops;
    std::vector<vec2<double>> cap_vertices_2d;
    std::vector<std::vector<uint64_t>> cap_edges_2d;
    YLOGDEBUG("Preparing " << paths.size() << " paths for constrained Delaunay triangulation");
    for(const auto &path : paths){
        if(!path.closed || (path.points.size() < 3U)) continue;
        auto projected_loop = project_polyline_vec2(sketch, path.points);
        if(projected_loop.size() < 3U) continue;
        raw_closed_loops.push_back(projected_loop);

        const auto base_index = static_cast<uint64_t>(cap_vertices_2d.size());
        for(std::size_t i = 0U; i < projected_loop.size(); ++i){
            cap_vertices_2d.push_back(projected_loop.at(i));
            const auto next_i = (i + 1U) % projected_loop.size();
            cap_edges_2d.push_back(std::vector<uint64_t>{
                base_index + static_cast<uint64_t>(i),
                base_index + static_cast<uint64_t>(next_i)
            });
        }
    }
    if(raw_closed_loops.empty() || (cap_vertices_2d.size() < 3U)) return;

    const auto closed_loops = normalize_closed_loops(raw_closed_loops);
    if(closed_loops.empty()) return;

    YLOGDEBUG("Sending " << cap_vertices_2d.size() << " vertices and " << cap_edges_2d.size() 
                         << " edge constraints for constrained Delaunay triangulation");
    fv_surface_mesh<double, uint64_t> planar_caps;
    try{
        planar_caps = Constrained_Delaunay_Triangulation_2<double, uint64_t>(cap_vertices_2d, cap_edges_2d);
    }catch(const std::exception &e){
        planar_caps.faces.clear();
        YLOGWARN("Delaunay triangulation failed: " << e.what());
    }
    if(planar_caps.faces.empty()){
        try{
            YLOGWARN("Attempting fallback triangulation via monotone decomposition");
            const auto monotone_pieces = Monotone_Decomposition_2<double, uint64_t>(raw_closed_loops);
            planar_caps = Triangulate_Monotone_Decomposition<double, uint64_t>(raw_closed_loops, monotone_pieces, false);
        }catch(const std::exception &e){
            planar_caps.faces.clear();
            YLOGWARN("Triangulation via monotone decomposition failed: " << e.what());
        }
    }
    if(planar_caps.faces.empty()) return;

    const auto normal = sketch.plane().normal();
    fv_surface_mesh<double, uint64_t> near_cap_mesh;
    fv_surface_mesh<double, uint64_t> far_cap_mesh;
    const uint64_t near_base = static_cast<uint64_t>(mesh.vertices.size());
    for(const auto &point : cap_vertices_2d){
        const auto projected_point = to_projection(point);
        const auto scaled_near = sketch.lift(scale_projection_about(projected_point, scale_centre, near_scale));
        mesh.vertices.emplace_back(scaled_near + normal * near_offset);
        near_cap_mesh.vertices.emplace_back(scaled_near + normal * near_offset);
    }
    const uint64_t far_base = static_cast<uint64_t>(mesh.vertices.size());
    for(const auto &point : cap_vertices_2d){
        const auto projected_point = to_projection(point);
        const auto scaled_far = sketch.lift(scale_projection_about(projected_point, scale_centre, far_scale));
        mesh.vertices.emplace_back(scaled_far + normal * far_offset);
        far_cap_mesh.vertices.emplace_back(scaled_far + normal * far_offset);
    }

    struct cap_face_candidate_t {
        uint64_t i0;
        uint64_t i1;
        uint64_t i2;
        double area_sign;
    };
    std::vector<cap_face_candidate_t> cap_candidates;
    for(const auto &face : planar_caps.faces){
        if(face.size() != 3U){
            // Skip any non-triangular faces from the Delaunay triangulation.
            // In particular, quad (4-vertex) faces would produce internal
            // non-manifold geometry and must not be added to the surface mesh.
            YLOGWARN("Skipping face with " << face.size() << " vertices"); 
            continue;
        }
        const auto &a = cap_vertices_2d.at(face.at(0));
        const auto &b = cap_vertices_2d.at(face.at(1));
        const auto &c = cap_vertices_2d.at(face.at(2));
        const vec2<double> centroid((a.x + b.x + c.x) / 3.0,
                                    (a.y + b.y + c.y) / 3.0);
        if(!point_in_normalized_loops(centroid, closed_loops)) continue;

        const auto orientation = signed_triangle_orientation(a, b, c);
        if(orientation == 0) continue;

        const auto i0 = static_cast<uint64_t>(face.at(0));
        const auto i1 = static_cast<uint64_t>(face.at(1));
        const auto i2 = static_cast<uint64_t>(face.at(2));
        cap_candidates.push_back(cap_face_candidate_t{ i0, i1, i2, static_cast<double>(orientation) });
    }

    if(cap_candidates.empty()) return;

    int64_t pos_count = 0;
    int64_t neg_count = 0;
    for(const auto &cand : cap_candidates){
        if(cand.area_sign > 0.0) ++pos_count;
        else ++neg_count;
    }

    if(pos_count >= neg_count){
        for(const auto &cand : cap_candidates){
            const auto i0 = cand.i0;
            const auto i1 = cand.i1;
            const auto i2 = cand.i2;
            if(cand.area_sign > 0.0){
                append_triangle(mesh, far_base + i0, far_base + i1, far_base + i2);
                append_triangle(mesh, near_base + i0, near_base + i2, near_base + i1);
                append_triangle(far_cap_mesh, i0, i1, i2);
                append_triangle(near_cap_mesh, i0, i2, i1);
            }else{
                append_triangle(mesh, far_base + i0, far_base + i2, far_base + i1);
                append_triangle(mesh, near_base + i0, near_base + i1, near_base + i2);
                append_triangle(far_cap_mesh, i0, i2, i1);
                append_triangle(near_cap_mesh, i0, i1, i2);
            }
        }
    }else{
        for(const auto &cand : cap_candidates){
            const auto i0 = cand.i0;
            const auto i1 = cand.i1;
            const auto i2 = cand.i2;
            if(cand.area_sign < 0.0){
                append_triangle(mesh, far_base + i0, far_base + i2, far_base + i1);
                append_triangle(mesh, near_base + i0, near_base + i1, near_base + i2);
                append_triangle(far_cap_mesh, i0, i2, i1);
                append_triangle(near_cap_mesh, i0, i1, i2);
            }else{
                append_triangle(mesh, far_base + i0, far_base + i1, far_base + i2);
                append_triangle(mesh, near_base + i0, near_base + i2, near_base + i1);
                append_triangle(far_cap_mesh, i0, i1, i2);
                append_triangle(near_cap_mesh, i0, i2, i1);
            }
        }
    }
    if(cap_meshes != nullptr){
        cap_meshes->clear();
        if(!near_cap_mesh.faces.empty()){
            near_cap_mesh.recreate_involved_face_index();
            cap_meshes->push_back(std::move(near_cap_mesh));
        }
        if(!far_cap_mesh.faces.empty()){
            far_cap_mesh.recreate_involved_face_index();
            cap_meshes->push_back(std::move(far_cap_mesh));
        }
    }
}

} // anonymous namespace

namespace sketch_extrude {

bool build_extruded_surface_mesh(const Sketch &sketch,
                                 const extrusion_options_t &options,
                                 fv_surface_mesh<double, uint64_t> &mesh,
                                 std::vector<fv_surface_mesh<double, uint64_t>> *cap_meshes,
                                 std::string *error_message){
    mesh = {};
    if(cap_meshes != nullptr) cap_meshes->clear();

    if(!sketch.has_plane()){
        store_error(error_message, "Unable to extrude sketch without an embedded plane");
        return false;
    }
    if(!std::isfinite(options.into_frame_length)
    || !std::isfinite(options.out_of_frame_length)
    || !std::isfinite(options.into_frame_angle_degrees)
    || !std::isfinite(options.out_of_frame_angle_degrees)){
        store_error(error_message, "Extrusion lengths and angles must be finite");
        return false;
    }
    if((options.into_frame_length + options.out_of_frame_length) <= extrude_min_epsilon){
        store_error(error_message,
                    "Extrusion lengths may be negative, but they must sum to a positive distance between the caps");
        return false;
    }

    const double near_offset = -options.out_of_frame_length;
    const double far_offset = options.into_frame_length;
    try{
        std::size_t computed_segments = options.curve_segments;
        if(std::isfinite(options.max_discretization_error) && (options.max_discretization_error > 0.0)){
            computed_segments = 16U;
            for(std::size_t i = 0U; i < sketch.primitive_count(); ++i){
                const auto *p = sketch.primitive(i);
                if(!p || p->kind() == Sketch::primitive_kind_t::vertex) continue;
                if(p->tag == Sketch::geometry_tag_t::support) continue;
                computed_segments = std::max(computed_segments,
                    compute_discretization_segments(sketch, i, options.max_discretization_error, 1024U));
            }
        }
        const auto curve_segments = std::max<std::size_t>(computed_segments, 16U);
        const auto paths = collect_extruded_paths(sketch, curve_segments);
        const auto bounds = projected_path_bounds(sketch, paths);
        if(!bounds.is_valid()){
            store_error(error_message, "Sketch does not contain any extrudable primitives");
            return false;
        }

        const Sketch::projection_t scale_centre = {
            (bounds.min.u + bounds.max.u) * 0.5,
            (bounds.min.v + bounds.max.v) * 0.5
        };
        const auto half_extent_u = std::max(0.0, (bounds.max.u - bounds.min.u) * 0.5);
        const auto half_extent_v = std::max(0.0, (bounds.max.v - bounds.min.v) * 0.5);
        const auto reference_half_extent = std::max(half_extent_u, half_extent_v);
        const auto near_scale = extrusion_scale_factor(reference_half_extent,
                                                       options.out_of_frame_length,
                                                       options.out_of_frame_angle_degrees);
        const auto far_scale = extrusion_scale_factor(reference_half_extent,
                                                      options.into_frame_length,
                                                      options.into_frame_angle_degrees);
        if(!std::isfinite(near_scale) || !std::isfinite(far_scale)){
            store_error(error_message, "Extrusion scale factors must be finite");
            return false;
        }
        if((near_scale <= extrude_min_epsilon) || (far_scale <= extrude_min_epsilon)){
            store_error(error_message,
                        "Extrusion angles narrow the sketch past collapse; reduce the magnitude of the narrowing angle");
            return false;
        }

        bool emitted_geometry = false;
        for(const auto &path : paths){
            append_extruded_polyline_sides(sketch,
                                           path,
                                           scale_centre,
                                           near_offset,
                                           far_offset,
                                           near_scale,
                                           far_scale,
                                           mesh);
            emitted_geometry = emitted_geometry || !path.points.empty();
        }
        append_extruded_end_caps(sketch,
                                 paths,
                                 scale_centre,
                                 near_offset,
                                 far_offset,
                                 near_scale,
                                 far_scale,
                                 mesh,
                                 cap_meshes);

        if(!emitted_geometry || mesh.faces.empty()){
            store_error(error_message, "Sketch does not contain any extrudable primitives");
            mesh = {};
            if(cap_meshes != nullptr) cap_meshes->clear();
            return false;
        }
        mesh.recreate_involved_face_index();

        // Post-process the extruded mesh to remove internal faces and ensure
        // consistent normal orientation.
        remove_internal_mesh_faces(mesh);

        return true;
    }catch(const std::exception &e){
        YLOGWARN("Sketch extrusion failed with exception: '" << e.what() << "'");
        mesh = {};
        if(cap_meshes != nullptr) cap_meshes->clear();
        store_error(error_message, std::string("Sketch extrusion failed: ") + e.what());
        return false;
    }catch(...){
        YLOGWARN("Sketch extrusion failed with an unknown exception");
        mesh = {};
        if(cap_meshes != nullptr) cap_meshes->clear();
        store_error(error_message, "Sketch extrusion failed with an unknown exception");
        return false;
    }
}

} // namespace sketch_extrude
