// MaskContours.cc - A part of DICOMautomaton 2026. Written by hal clark and OpenAI.
//
// Slice contour paths against one or more named polygonal regions. This operation deliberately works on
// Contour_Data / contour_collection objects; GPX is only one of the formats that can populate those objects.

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

#include "../GIS.h"
#include "../Regex_Selectors.h"
#include "../String_Parsing.h"
#include "../Structs.h"

#include "MaskContours.h"
#include "MaskContours_Internal.h"

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

OperationDoc OpArgDocMaskContours(){
    OperationDoc out;
    out.name = "MaskContours";
    out.tags.emplace_back("category: contour processing");
    out.desc =
        "Slices selected contours against one or more named polygonal regions. The original contours are retained; "
        "derived inside/outside path portions are appended and tagged with MaskContoursRegion, MaskContoursState, "
        "MaskContoursLabel, and MaskContoursDebounceDistance metadata.";

    out.notes.emplace_back(
        "Regions use the parsed_function syntax Region(name){Polygon(...); GeoPolygon(...); ...}. A Region is the union "
        "of all of its polygon children. Polygon uses native contour x,y coordinates. Polygon3D accepts x,y,z triples "
        "but masking is performed in the x-y footprint. GeoPolygon accepts latitude,longitude pairs and applies the same "
        "Mercator projection used by the GPX loader. Polygon closure is implicit; repeating the first vertex is optional."
    );
    out.notes.emplace_back(
        "DebounceDistance is measured along the source contour in native contour coordinate units. A short inside/outside "
        "run is absorbed only when it is bracketed by the opposite state. This suppresses GPS boundary chatter and brief "
        "leave-and-return excursions without erasing short runs at the beginning or end of a trace."
    );
    out.notes.emplace_back(
        "Derived contour collections are homogeneous in MaskContoursRegion and MaskContoursState so downstream metadata "
        "partitioning/filtering can select a named region and inside/outside state reliably."
    );
    out.notes.emplace_back(
        "The lower-mainland examples are intentionally coarse demonstration polygons, not authoritative park boundaries."
    );

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = CCWhitelistOpArgDoc();
    out.args.back().name = "ROISelection";
    out.args.back().default_val = "all";

    out.args.emplace_back();
    out.args.back().name = "Regions";
    out.args.back().desc =
        "Semicolon-separated Region functions. Each Region(name){...} contains one or more Polygon(x,y,...), "
        "Polygon3D(x,y,z,...), or GeoPolygon(latitude,longitude,...) child functions.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = {
        "Region(test){Polygon(0,0, 10,0, 10,10, 0,10)}",
        "Region(Fromme){GeoPolygon(49.327,-123.104, 49.392,-123.104, 49.392,-123.036, 49.327,-123.036)};"
        "Region(Seymour){GeoPolygon(49.315,-123.010, 49.390,-123.010, 49.390,-122.925, 49.315,-122.925)};"
        "Region(EagleMountain){GeoPolygon(49.275,-122.875, 49.345,-122.875, 49.345,-122.790, 49.275,-122.790)}"
    };

    out.args.emplace_back();
    out.args.back().name = "DebounceDistance";
    out.args.back().desc =
        "Maximum along-contour length of an interior run to absorb when it leaves one state and returns to that same "
        "state. Zero disables debouncing. For GPX data loaded by DICOMautomaton, coordinates are Mercator metres.";
    out.args.back().default_val = "0.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0", "5.0", "20.0", "100.0" };

    return out;
}

bool MaskContours(Drover &DICOM_data,
                  const OperationArgPkg& OptArgs,
                  std::map<std::string, std::string>& /*InvocationMetadata*/,
                  const std::string& /*FilenameLex*/){
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();
    const auto RegionSpec = OptArgs.getValueStr("Regions").value();
    const auto DebounceDistance = std::stod(OptArgs.getValueStr("DebounceDistance").value());

    if(!std::isfinite(DebounceDistance) || (DebounceDistance < 0.0)){
        throw std::invalid_argument("MaskContours: DebounceDistance must be finite and non-negative");
    }
    const auto regions = dcma::mask_contours::parse_regions(RegionSpec);

    auto cc_all = All_CCs(DICOM_data);
    auto cc_selected = Whitelist(cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection);
    if(cc_selected.empty()){
        throw std::invalid_argument("MaskContours: no contours selected");
    }

    // Build all derived data separately so appending it cannot invalidate references in the selection list.
    // Keep each appended collection homogeneous by (region,state), which makes the added metadata useful to
    // collection-level selectors and partitioning operations.
    auto contour_storage = std::make_shared<Contour_Data>();
    for(const auto &cc_refw : cc_selected){
        const auto &src_cc = cc_refw.get();
        for(const auto &region : regions){
            contour_collection<double> inside_cc;
            contour_collection<double> outside_cc;

            for(const auto &source : src_cc.contours){
                auto pieces = dcma::mask_contours::slice_contour(source, region, DebounceDistance);
                for(auto &piece : pieces){
                    const auto state_it = piece.metadata.find("MaskContoursState");
                    if(state_it == piece.metadata.end()){
                        throw std::logic_error("MaskContours: derived contour is missing MaskContoursState metadata");
                    }
                    if(state_it->second == "inside"){
                        inside_cc.contours.emplace_back(std::move(piece));
                    }else if(state_it->second == "outside"){
                        outside_cc.contours.emplace_back(std::move(piece));
                    }else{
                        throw std::logic_error("MaskContours: derived contour has an invalid MaskContoursState value");
                    }
                }
            }

            if(!inside_cc.contours.empty()) contour_storage->ccs.emplace_back(std::move(inside_cc));
            if(!outside_cc.contours.empty()) contour_storage->ccs.emplace_back(std::move(outside_cc));
        }
    }

    if(!contour_storage->ccs.empty()) DICOM_data.Consume(contour_storage);
    return true;
}
