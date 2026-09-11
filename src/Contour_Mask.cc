// Contour_Mask.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <initializer_list>
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

std::string canonical_region_name(const std::string &name){
    std::string out;
    for(const auto c : name){
        const auto uc = static_cast<unsigned char>(c);
        if(std::isalnum(uc)) out.push_back(static_cast<char>(std::tolower(uc)));
    }
    return out;
}

mask_region make_geographic_region(const std::string &name,
                                   const std::initializer_list<std::pair<double, double>> &lat_lon_vertices){
    mask_region region;
    region.name = name;
    mask_polygon polygon;
    for(const auto &lat_lon : lat_lon_vertices){
        const auto xy = dcma::gis::project_mercator(lat_lon.first, lat_lon.second);
        polygon.vertices.emplace_back(xy.first, xy.second);
    }
    validate_polygon(polygon);
    region.polygons.emplace_back(std::move(polygon));
    return region;
}

const std::vector<mask_region> &predefined_regions(){
    // These irregular footprints encompass the concentrated trail networks while excluding nearby urban areas.
    static const std::vector<mask_region> regions = {
        make_geographic_region("Fromme", {
            {49.326,-123.092}, {49.342,-123.101}, {49.369,-123.088}, {49.386,-123.068},
            {49.374,-123.046}, {49.345,-123.052}
        }),
        make_geographic_region("Burke", {
            {49.278,-122.755}, {49.298,-122.765}, {49.333,-122.744}, {49.351,-122.708},
            {49.331,-122.672}, {49.300,-122.686}
        }),
        make_geographic_region("Eagle Mountain", {
            {49.276,-122.883}, {49.300,-122.878}, {49.335,-122.855}, {49.347,-122.821},
            {49.326,-122.797}, {49.294,-122.817}
        }),
        make_geographic_region("Cypress Mountain", {
            {49.337,-123.219}, {49.357,-123.239}, {49.389,-123.235}, {49.407,-123.205},
            {49.393,-123.174}, {49.362,-123.181}
        }),
        make_geographic_region("Grouse Mountain Bike Park", {
            {49.337,-123.137}, {49.354,-123.149}, {49.380,-123.137}, {49.392,-123.116},
            {49.373,-123.095}, {49.349,-123.105}
        }),
        make_geographic_region("Seymour Mountain", {
            {49.312,-123.018}, {49.332,-123.028}, {49.366,-123.012}, {49.391,-122.980},
            {49.377,-122.944}, {49.344,-122.951}, {49.320,-122.976}
        }),
        make_geographic_region("Burnaby Mountain", {
            {49.267,-122.963}, {49.282,-122.966}, {49.296,-122.945}, {49.294,-122.913},
            {49.277,-122.897}, {49.263,-122.925}
        }),
        make_geographic_region("Delta Watershed", {
            {49.087,-122.937}, {49.103,-122.948}, {49.127,-122.939}, {49.143,-122.918},
            {49.130,-122.892}, {49.104,-122.899}
        }),
        make_geographic_region("Bert Flinn Park", {
            {49.289,-122.865}, {49.303,-122.873}, {49.322,-122.859}, {49.327,-122.836},
            {49.311,-122.820}, {49.292,-122.836}
        }),
        make_geographic_region("Thornhill", {
            {49.211,-122.594}, {49.229,-122.606}, {49.253,-122.594}, {49.267,-122.568},
            {49.250,-122.541}, {49.226,-122.552}
        }),
        make_geographic_region("Woodlot 0007", {
            {49.176,-122.596}, {49.193,-122.607}, {49.217,-122.596}, {49.229,-122.570},
            {49.211,-122.545}, {49.187,-122.558}
        }),
        make_geographic_region("Bear Mountain", {
            {49.158,-122.323}, {49.177,-122.335}, {49.205,-122.321}, {49.218,-122.292},
            {49.198,-122.264}, {49.173,-122.278}
        }),
        make_geographic_region("Red Mountain", {
            {49.142,-122.347}, {49.158,-122.358}, {49.179,-122.346}, {49.188,-122.321},
            {49.170,-122.300}, {49.149,-122.315}
        }),
        make_geographic_region("Sumas Mountain", {
            {49.086,-122.238}, {49.108,-122.252}, {49.139,-122.236}, {49.153,-122.202},
            {49.131,-122.170}, {49.101,-122.187}
        }),
        make_geographic_region("Vedder Mountain", {
            {49.071,-122.006}, {49.090,-122.020}, {49.118,-122.006}, {49.130,-121.975},
            {49.110,-121.947}, {49.083,-121.961}
        }),
        make_geographic_region("Ledgeview", {
            {49.045,-122.217}, {49.061,-122.228}, {49.082,-122.217}, {49.091,-122.194},
            {49.074,-122.174}, {49.052,-122.187}
        }),
        make_geographic_region("Whistler Mountain Bike Park", {
            {50.041,-122.979}, {50.064,-122.997}, {50.096,-122.979}, {50.112,-122.946},
            {50.087,-122.914}, {50.057,-122.930}
        }),
        make_geographic_region("Mount Washington Bike Park", {
            {49.724,-125.322}, {49.743,-125.338}, {49.770,-125.326}, {49.786,-125.299},
            {49.768,-125.270}, {49.740,-125.282}
        }),
        make_geographic_region("Revelstoke Bike Park", {
            {50.939,-118.190}, {50.960,-118.205}, {50.988,-118.188}, {51.000,-118.153},
            {50.977,-118.126}, {50.950,-118.145}
        }),
        make_geographic_region("Kicking Horse Bike Park", {
            {51.277,-117.070}, {51.298,-117.087}, {51.329,-117.071}, {51.344,-117.035},
            {51.320,-117.006}, {51.291,-117.023}
        }),
        make_geographic_region("Sun Peaks Bike Park", {
            {50.866,-119.928}, {50.885,-119.944}, {50.913,-119.927}, {50.926,-119.895},
            {50.904,-119.867}, {50.877,-119.883}
        }),
        make_geographic_region("Kamloops Bike Ranch", {
            {50.661,-120.279}, {50.672,-120.286}, {50.687,-120.278}, {50.692,-120.260},
            {50.680,-120.248}, {50.666,-120.258}
        }),
        make_geographic_region("Winsport Bike Park", {
            {51.073,-114.220}, {51.082,-114.227}, {51.094,-114.219}, {51.098,-114.204},
            {51.088,-114.193}, {51.076,-114.202}
        }),
        make_geographic_region("Hinton Bike Park", {
            {53.390,-117.583}, {53.403,-117.593}, {53.422,-117.581}, {53.430,-117.558},
            {53.416,-117.538}, {53.398,-117.550}
        }),
    };
    return regions;
}

mask_region lookup_predefined_region(const std::string &name){
    auto key = canonical_region_name(name);
    if(key == "seymour") key = "seymourmountain";
    const auto &regions = predefined_regions();
    const auto it = std::find_if(regions.begin(), regions.end(), [&](const auto &region){
        return canonical_region_name(region.name) == key;
    });
    if(it == regions.end()){
        throw std::invalid_argument("MaskContours: unknown NamedRegion '" + name + "'");
    }
    return *it;
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
        const auto fname = Canonicalize_String2(f.name, CANONICALIZE::TO_LOWER);
        if(fname == "region"){
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
        }else if(fname == "namedregion"){
            if(f.parameters.size() != 1 || f.parameters.front().raw.empty() || !f.children.empty()){
                throw std::invalid_argument("MaskContours: NamedRegion requires exactly one name parameter and no children");
            }
            out.emplace_back(lookup_predefined_region(f.parameters.front().raw));
        }else{
            throw std::invalid_argument("MaskContours: top-level functions must be Region(name){...} or NamedRegion(name)");
        }
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
