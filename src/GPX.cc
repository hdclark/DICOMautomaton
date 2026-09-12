//GPX.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "YgorMath.h"

#include "GIS.h"
#include "Regex_Selectors.h"
#include "String_Parsing.h"
#include "GPX.h"

namespace {

using named_boundary_contour_t = dcma::gpx::named_boundary_contour;
using split_open_contour_segment_t = dcma::gpx::split_open_contour_segment;
static const std::string unnamed_boundary_prefix = "Unnamed Boundary ";

plane<double>
boundary_plane(){
    return plane<double>( vec3<double>(0.0, 0.0, 1.0),
                          vec3<double>(0.0, 0.0, 0.0) );
}

std::string
canonicalize_boundary_spec(std::string in){
    in.erase( std::remove_if(std::begin(in), std::end(in),
                             [](unsigned char c){
                                 return !std::isalnum(c);
                             }),
              std::end(in) );
    std::transform(std::begin(in), std::end(in), std::begin(in),
                   [](unsigned char c){
                       return std::tolower(c);
                   } );
    return in;
}

named_boundary_contour_t
make_projected_boundary(std::string name,
                        const std::vector<std::pair<double,double>> &lat_lon_vertices){
    named_boundary_contour_t out;
    out.name = std::move(name);
    out.contour.closed = true;

    for(const auto &[lat, lon] : lat_lon_vertices){
        const auto [x, y] = dcma::gis::project_mercator(lat, lon);
        out.contour.points.emplace_back(x, y, 0.0);
    }

    if(out.contour.points.size() < 3){
        throw std::invalid_argument("Boundary contour requires at least three vertices");
    }
    return out;
}

named_boundary_contour_t
make_projected_rectangle(std::string name,
                         double lat_min,
                         double lon_min,
                         double lat_max,
                         double lon_max){
    return make_projected_boundary(std::move(name),
                                   { { lat_min, lon_min },
                                     { lat_min, lon_max },
                                     { lat_max, lon_max },
                                     { lat_max, lon_min } });
}

std::vector<bool>
point_boundary_membership(const vec3<double> &point,
                          const std::vector<named_boundary_contour_t> &boundaries){
    auto out = std::vector<bool>(boundaries.size(), false);
    const auto plane = boundary_plane();
    for(size_t i = 0; i < boundaries.size(); ++i){
        out.at(i) = boundaries.at(i).contour.Is_Point_In_Polygon_Projected_Orthogonally(plane, point);
    }
    return out;
}

std::string
classify_segment_location(const contour_of_points<double> &segment,
                          const std::vector<named_boundary_contour_t> &boundaries){
    if(boundaries.empty()){
        return "Track";
    }

    std::vector<int64_t> inside_counts(boundaries.size(), 0);
    const auto plane = boundary_plane();
    for(const auto &point : segment.points){
        for(size_t i = 0; i < boundaries.size(); ++i){
            if(boundaries.at(i).contour.Is_Point_In_Polygon_Projected_Orthogonally(plane, point)){
                ++inside_counts.at(i);
            }
        }
    }

    const auto best_count_it = std::max_element(std::begin(inside_counts), std::end(inside_counts));
    if( (2 * (*best_count_it)) > static_cast<int64_t>(segment.points.size()) ){
        const auto idx = static_cast<size_t>(std::distance(std::begin(inside_counts), best_count_it));
        return boundaries.at(idx).name;
    }

    const auto &first_point = segment.points.front();
    const auto &last_point = segment.points.back();
    for(size_t i = 0; i < boundaries.size(); ++i){
        if(boundaries.at(i).contour.Is_Point_In_Polygon_Projected_Orthogonally(plane, last_point)){
            return boundaries.at(i).name;
        }
    }
    for(size_t i = 0; i < boundaries.size(); ++i){
        if(boundaries.at(i).contour.Is_Point_In_Polygon_Projected_Orthogonally(plane, first_point)){
            return boundaries.at(i).name + " transit";
        }
    }

    const auto ref_point = segment.Average_Point();

    size_t best_idx = 0;
    double best_dist_sq = std::numeric_limits<double>::infinity();
    for(size_t i = 0; i < boundaries.size(); ++i){
        const auto d = boundaries.at(i).contour.Centroid() - ref_point;
        const auto dist_sq = d.Dot(d);
        if(dist_sq < best_dist_sq){
            best_dist_sq = dist_sq;
            best_idx = i;
        }
    }
    return boundaries.at(best_idx).name + " transit";
}

split_open_contour_segment_t
make_segment(contour_of_points<double> contour,
             const std::vector<named_boundary_contour_t> &boundaries){
    contour.closed = false;

    split_open_contour_segment_t out;
    out.location_name = classify_segment_location(contour, boundaries);
    out.contour = std::move(contour);
    return out;
}

} // anonymous namespace

namespace dcma {
namespace gpx {

std::vector<named_boundary_contour>
generate_predefined_boundary_contours_lower_mainland_mtb(){
    return {
        make_projected_rectangle("Cypress Mountain", 49.369, -123.237, 49.410, -123.172),
        make_projected_rectangle("Fromme",          49.338, -123.104, 49.377, -123.048),
        make_projected_rectangle("Seymour",         49.340, -122.999, 49.392, -122.925),
        make_projected_rectangle("Eagle Mountain",  49.053, -122.301, 49.131, -122.202),
        make_projected_rectangle("Burke",           49.282, -122.746, 49.351, -122.652),
        make_projected_rectangle("Burnaby Mountain",49.253, -122.965, 49.297, -122.880),
        make_projected_rectangle("Bert Flinn Park", 49.276, -122.885, 49.318, -122.831),
        make_projected_rectangle("Watershed Park",  49.087, -122.923, 49.136, -122.839),
        make_projected_rectangle("Thornhill",       49.208, -122.577, 49.258, -122.493),
        make_projected_rectangle("Bear Mountain",   49.139, -122.272, 49.183, -122.207),
        make_projected_rectangle("Red Mountain",    49.161, -122.334, 49.206, -122.258),
        make_projected_rectangle("Woodlot 0007",    49.197, -122.239, 49.247, -122.155),
        make_projected_rectangle("Vedder Mountain", 49.025, -122.038, 49.083, -121.950),
    };
}

std::vector<named_boundary_contour>
parse_boundary_contours(const std::string &boundary_spec){
    const auto canonical = canonicalize_boundary_spec(boundary_spec);
    if( canonical.empty()
    ||  (canonical == "default")
    ||  (canonical == "predefined")
    ||  (canonical == "lowermainlandmtb")
    ||  (canonical == "metrovancouvermtb") ){
        return generate_predefined_boundary_contours_lower_mainland_mtb();
    }

    const auto parsed = parse_functions(boundary_spec);
    if(parsed.empty()){
        throw std::invalid_argument("BoundaryContours could not be parsed");
    }

    const auto regex_boundary = Compile_Regex("bo?u?n?d?a?r?y?|co?n?t?o?u?r?|po?l?y?g?o?n?|pa?t?h?");

    std::vector<named_boundary_contour> out;
    int64_t unnamed_boundary_count = 0;
    for(const auto &func : parsed){
        if(!std::regex_match(func.name, regex_boundary)){
            throw std::invalid_argument("BoundaryContours contains an unsupported boundary function");
        }

        size_t first_numeric = 0;
        std::string boundary_name;
        if( !func.parameters.empty()
        &&  !func.parameters.front().number ){
            boundary_name = func.parameters.front().raw;
            first_numeric = 1;
        }else{
            boundary_name = unnamed_boundary_prefix + std::to_string(++unnamed_boundary_count);
        }

        const auto remaining = func.parameters.size() - first_numeric;
        if( (remaining < 6)
        ||  ((remaining % 2) != 0) ){
            throw std::invalid_argument("Each boundary contour requires at least three latitude/longitude pairs");
        }

        std::vector<std::pair<double,double>> lat_lon_vertices;
        for(size_t i = first_numeric; i < func.parameters.size(); i += 2){
            const auto &lat = func.parameters.at(i);
            const auto &lon = func.parameters.at(i + 1);
            if( !lat.number || !lon.number ){
                throw std::invalid_argument("BoundaryContours latitude/longitude entries must be numeric");
            }
            lat_lon_vertices.emplace_back(lat.number.value(), lon.number.value());
        }

        out.emplace_back( make_projected_boundary(std::move(boundary_name), lat_lon_vertices) );
    }
    return out;
}

std::vector<split_open_contour_segment>
split_open_contour_on_boundary_crossings(const contour_of_points<double> &track,
                                         const std::vector<named_boundary_contour> &boundaries,
                                         double debounce_distance_metres){
    if(debounce_distance_metres < 0.0){
        throw std::invalid_argument("Debounce distance must be non-negative");
    }

    if(track.points.empty()){
        return {};
    }

    if(track.closed || (track.points.size() < 2) || boundaries.empty()){
        return { make_segment(track, boundaries) };
    }

    std::vector<vec3<double>> points(std::begin(track.points), std::end(track.points));

    std::vector<split_open_contour_segment> out;
    contour_of_points<double> current_segment = track;
    current_segment.points.clear();
    current_segment.closed = false;
    current_segment.points.push_back(points.front());

    auto previous_membership = point_boundary_membership(points.front(), boundaries);
    auto distance_since_last_split = debounce_distance_metres;
    auto split_is_armed = true;

    for(size_t i = 1; i < points.size(); ++i){
        const auto &prev = points.at(i - 1);
        const auto &curr = points.at(i);

        current_segment.points.push_back(curr);

        const auto d = curr - prev;
        distance_since_last_split += std::sqrt(d.Dot(d));
        if( (debounce_distance_metres <= 0.0)
        ||  (debounce_distance_metres <= distance_since_last_split) ){
            split_is_armed = true;
        }

        const auto current_membership = point_boundary_membership(curr, boundaries);
        const auto boundary_crossed = (previous_membership != current_membership);

        if(boundary_crossed && split_is_armed){
            if(2 <= current_segment.points.size()){
                out.emplace_back( make_segment(current_segment, boundaries) );
            }

            current_segment = track;
            current_segment.points.clear();
            current_segment.closed = false;
            current_segment.points.push_back(curr);

            distance_since_last_split = 0.0;
            split_is_armed = (debounce_distance_metres <= 0.0);
        }

        previous_membership = current_membership;
    }

    if(2 <= current_segment.points.size()){
        out.emplace_back( make_segment(current_segment, boundaries) );
    }

    if(out.empty()){
        out.emplace_back( make_segment(track, boundaries) );
    }

    return out;
}

} // namespace gpx
} // namespace dcma
