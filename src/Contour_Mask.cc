// Contour_Mask.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "YgorMath.h"
#include "YgorString.h"

#include "GIS.h"
#include "Regex_Selectors.h"
#include "String_Parsing.h"
#include "Structs.h"

#include "Contour_Mask.h"

namespace dcma::mask_contours {

namespace {

void validate_polygon(mask_polygon &poly){
    if(poly.vertices.size() >= 2
    && poly.vertices.front().sq_dist(poly.vertices.back()) <= 1.0E-20){
        poly.vertices.pop_back(); // Closure is implicit.
    }
    if(poly.vertices.size() < 3){
        throw std::invalid_argument("MaskContours: each polygon requires at least three distinct vertices");
    }

    double twice_area = 0.0;
    for(std::size_t i = 0; i < poly.vertices.size(); ++i){
        const auto &a = poly.vertices.at(i);
        const auto &b = poly.vertices.at((i+1) % poly.vertices.size());
        if(a.sq_dist(b) <= 1.0E-20){
            throw std::invalid_argument("MaskContours: polygon contains sequential duplicate vertices");
        }
        twice_area += a.x * b.y - b.x * a.y;
    }
    if(std::abs(twice_area) <= 1.0E-12){
        throw std::invalid_argument("MaskContours: polygon has zero projected area");
    }

    const auto n = poly.vertices.size();
    for(std::size_t i = 0; i < n; ++i){
        const auto i2 = (i+1) % n;
        for(std::size_t j = i+1; j < n; ++j){
            const auto j2 = (j+1) % n;
            if((i == j) || (i2 == j) || (j2 == i)) continue; // Adjacent edges share a vertex.
            if((i == 0) && (j2 == 0)) continue;
            const auto &a = poly.vertices.at(i);
            const auto &b = poly.vertices.at(i2);
            const auto &c = poly.vertices.at(j);
            const auto &d = poly.vertices.at(j2);
            if(segments_intersect_beyond_shared_endpoints(a, b, c, d)
            || point_on_closed_segment(a, c, d)
            || point_on_closed_segment(b, c, d)
            || point_on_closed_segment(c, a, b)
            || point_on_closed_segment(d, a, b)){
                throw std::invalid_argument("MaskContours: polygon is not simple (self-intersection detected)");
            }
        }
    }
}

mask_polygon parse_polygon(const parsed_function &f){
    mask_polygon out;
    const auto fname = Canonicalize_String2(f.name, CANONICALIZE::TO_LOWER);

    if(fname == "polygon"){
        if((f.parameters.size() < 6) || ((f.parameters.size() % 2) != 0)){
            throw std::invalid_argument("MaskContours: Polygon expects x,y pairs for at least three vertices");
        }
        for(std::size_t i = 0; i < f.parameters.size(); i += 2){
            const auto x = get_as<double>(f.parameters.at(i).raw);
            const auto y = get_as<double>(f.parameters.at(i+1).raw);
            if(!x || !y) throw std::invalid_argument("MaskContours: Polygon contains a non-numeric coordinate");
            out.vertices.emplace_back(x.value(), y.value());
        }
    }else if(fname == "polygon3d"){
        if((f.parameters.size() < 9) || ((f.parameters.size() % 3) != 0)){
            throw std::invalid_argument("MaskContours: Polygon3D expects x,y,z triples for at least three vertices");
        }
        for(std::size_t i = 0; i < f.parameters.size(); i += 3){
            const auto x = get_as<double>(f.parameters.at(i).raw);
            const auto y = get_as<double>(f.parameters.at(i+1).raw);
            const auto z = get_as<double>(f.parameters.at(i+2).raw);
            if(!x || !y || !z) throw std::invalid_argument("MaskContours: Polygon3D contains a non-numeric coordinate");
            out.vertices.emplace_back(x.value(), y.value());
        }
    }else if(fname == "geopolygon"){
        if((f.parameters.size() < 6) || ((f.parameters.size() % 2) != 0)){
            throw std::invalid_argument("MaskContours: GeoPolygon expects latitude,longitude pairs for at least three vertices");
        }
        for(std::size_t i = 0; i < f.parameters.size(); i += 2){
            const auto lat = get_as<double>(f.parameters.at(i).raw);
            const auto lon = get_as<double>(f.parameters.at(i+1).raw);
            if(!lat || !lon) throw std::invalid_argument("MaskContours: GeoPolygon contains a non-numeric coordinate");
            const auto xy = dcma::gis::project_mercator(lat.value(), lon.value());
            out.vertices.emplace_back(xy.first, xy.second);
        }
    }else{
        throw std::invalid_argument("MaskContours: Region children must be Polygon, Polygon3D, or GeoPolygon functions");
    }

    validate_polygon(out);
    return out;
}

bool point_in_polygon_xy(const vec3<double> &p, const mask_polygon &poly){
    return point_in_polygon_or_on_boundary(poly.vertices, vec2<double>(p.x, p.y));
}

std::vector<double> edge_intersection_parameters(const vec3<double> &a,
                                                  const vec3<double> &b,
                                                  const mask_polygon &poly){
    std::vector<double> ts;
    const vec2<double> a_xy(a.x, a.y);
    const vec2<double> b_xy(b.x, b.y);
    const auto r = b_xy - a_xy;
    const auto rnorm = r.length();
    if(rnorm <= std::numeric_limits<double>::epsilon()) return ts;

    for(std::size_t i = 0; i < poly.vertices.size(); ++i){
        const auto &c = poly.vertices.at(i);
        const auto &d = poly.vertices.at((i+1) % poly.vertices.size());
        const auto s = d - c;
        const double denom = vec3<double>(r.x, r.y, 0.0).Cross(vec3<double>(s.x, s.y, 0.0)).z;
        const auto scale = std::max({1.0, std::abs(r.x), std::abs(r.y), std::abs(s.x), std::abs(s.y)});
        if(std::abs(denom) <= 1.0E-12 * scale * scale) continue; // Parallel/collinear: classification handles it.

        const auto qp = c - a_xy;
        const double t = vec3<double>(qp.x, qp.y, 0.0).Cross(vec3<double>(s.x, s.y, 0.0)).z / denom;
        const double u = vec3<double>(qp.x, qp.y, 0.0).Cross(vec3<double>(r.x, r.y, 0.0)).z / denom;
        constexpr double eps = 1.0E-11;
        if((-eps <= t) && (t <= 1.0+eps) && (-eps <= u) && (u <= 1.0+eps)){
            ts.emplace_back(std::max(0.0, std::min(1.0, t)));
        }
    }
    return ts;
}

vec3<double> lerp(const vec3<double> &a, const vec3<double> &b, double t){
    return a + (b-a)*t;
}

std::vector<contour_of_points<double>> contours_from_atoms(const contour_of_points<double> &source,
                                                           const std::vector<atomic_segment> &atoms,
                                                           const mask_region &region,
                                                           double debounce_distance){
    std::vector<contour_of_points<double>> out;
    if(atoms.empty()) return out;

    auto start_piece = [&](bool inside) -> contour_of_points<double> {
        contour_of_points<double> c = source;
        c.points.clear();
        c.closed = false;
        c.metadata["MaskContoursRegion"] = region.name;
        c.metadata["MaskContoursState"] = inside ? "inside" : "outside";
        c.metadata["MaskContoursLabel"] = region.name + ":" + (inside ? "inside" : "outside");
        c.metadata["MaskContoursDebounceDistance"] = to_string_max_precision(debounce_distance);
        return c;
    };

    bool state = atoms.front().inside;
    auto piece = start_piece(state);
    piece.points.emplace_back(atoms.front().a);

    for(const auto &atom : atoms){
        if(atom.inside != state){
            if(piece.points.size() >= 2) out.emplace_back(std::move(piece));
            state = atom.inside;
            piece = start_piece(state);
            piece.points.emplace_back(atom.a);
        }else if(piece.points.empty()){
            piece.points.emplace_back(atom.a);
        }else if(piece.points.back().sq_dist(atom.a) > 1.0E-20){
            piece.points.emplace_back(atom.a);
        }
        if(piece.points.empty() || piece.points.back().sq_dist(atom.b) > 1.0E-20){
            piece.points.emplace_back(atom.b);
        }
    }
    if(piece.points.size() >= 2) out.emplace_back(std::move(piece));

    // Preserve closure when the entire source contour belongs to one state. Split contours are paths by definition.
    if(source.closed && (out.size() == 1)){
        auto &c = out.front();
        if(c.points.size() >= 2 && c.points.front().sq_dist(c.points.back()) <= 1.0E-20){
            c.points.pop_back();
        }
        c.closed = true;
    }
    return out;
}

} // namespace

std::vector<mask_region> parse_regions(const std::string &spec){
    const auto funcs = parse_functions(spec);
    if(funcs.empty()) throw std::invalid_argument("MaskContours: Regions did not contain any Region functions");

    std::vector<mask_region> out;
    for(const auto &f : funcs){
        if(Canonicalize_String2(f.name, CANONICALIZE::TO_LOWER) != "region"){
            throw std::invalid_argument("MaskContours: top-level functions must be Region(name){...}");
        }
        if(f.parameters.size() != 1 || f.parameters.front().raw.empty()){
            throw std::invalid_argument("MaskContours: Region requires exactly one non-empty name parameter");
        }
        if(f.children.empty()){
            throw std::invalid_argument("MaskContours: Region requires at least one polygon child");
        }

        mask_region r;
        r.name = f.parameters.front().raw;
        for(const auto &child : f.children){
            r.polygons.emplace_back(parse_polygon(child));
        }
        out.emplace_back(std::move(r));
    }

    for(std::size_t i = 0; i < out.size(); ++i){
        for(std::size_t j = i+1; j < out.size(); ++j){
            if(out.at(i).name == out.at(j).name){
                throw std::invalid_argument("MaskContours: region names must be unique");
            }
        }
    }
    return out;
}

bool point_in_region_xy(const vec3<double> &p, const mask_region &r){
    for(const auto &poly : r.polygons){
        if(point_in_polygon_xy(p, poly)) return true;
    }
    return false;
}

double atomic_segment::length() const {
    return a.distance(b);
}

std::vector<atomic_segment> atomize_path(const std::vector<vec3<double>> &points,
                                         bool closed,
                                         const mask_region &region){
    std::vector<atomic_segment> out;
    if(points.size() < 2) return out;

    const std::size_t segment_count = closed ? points.size() : (points.size()-1);
    for(std::size_t i = 0; i < segment_count; ++i){
        const auto &a = points.at(i);
        const auto &b = points.at((i+1) % points.size());
        if(a.sq_dist(b) <= 1.0E-24) continue;

        std::vector<double> ts = {0.0, 1.0};
        for(const auto &poly : region.polygons){
            auto pts = edge_intersection_parameters(a,b,poly);
            ts.insert(ts.end(), pts.begin(), pts.end());
        }
        std::sort(ts.begin(), ts.end());
        ts.erase(std::unique(ts.begin(), ts.end(), [](double x, double y){ return std::abs(x-y) <= 1.0E-10; }), ts.end());

        for(std::size_t k = 1; k < ts.size(); ++k){
            const auto t0 = ts.at(k-1);
            const auto t1 = ts.at(k);
            if((t1-t0) <= 1.0E-12) continue;
            atomic_segment s;
            s.a = lerp(a,b,t0);
            s.b = lerp(a,b,t1);
            s.inside = point_in_region_xy(lerp(a,b,0.5*(t0+t1)), region);
            if(s.length() > 1.0E-12) out.emplace_back(std::move(s));
        }
    }
    return out;
}

void debounce_atoms(std::vector<atomic_segment> &atoms, double debounce_distance){
    if((debounce_distance <= 0.0) || atoms.size() < 3) return;

    // Repeatedly absorb a short interior run when it is bracketed by the opposite state. This intentionally does not
    // discard short runs at the start/end of a trace, because those are not "left and returned" excursions.
    bool changed = true;
    while(changed){
        changed = false;

        struct run { std::size_t first; std::size_t last; bool inside; double length; };
        std::vector<run> runs;
        for(std::size_t i = 0; i < atoms.size(); ++i){
            if(runs.empty() || (runs.back().inside != atoms.at(i).inside)){
                runs.push_back({i, i, atoms.at(i).inside, atoms.at(i).length()});
            }else{
                runs.back().last = i;
                runs.back().length += atoms.at(i).length();
            }
        }

        for(std::size_t i = 1; i+1 < runs.size(); ++i){
            if((runs.at(i).length <= debounce_distance)
            && (runs.at(i-1).inside == runs.at(i+1).inside)){
                for(std::size_t j = runs.at(i).first; j <= runs.at(i).last; ++j){
                    atoms.at(j).inside = runs.at(i-1).inside;
                }
                changed = true;
                break;
            }
        }
    }
}

std::vector<contour_of_points<double>> slice_contour(const contour_of_points<double> &source,
                                                     const mask_region &region,
                                                     double debounce_distance){
    std::vector<vec3<double>> points(source.points.begin(), source.points.end());
    auto atoms = atomize_path(points, source.closed, region);
    debounce_atoms(atoms, debounce_distance);
    return contours_from_atoms(source, atoms, region, debounce_distance);
}

} // namespace dcma::mask_contours

