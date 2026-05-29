// Sketch_Fillets.cc - Fillet helpers for planar sketches.

#include "Sketch_Fillets.h"
#include "Sketch.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr double fillet_min_epsilon = 1.0E-9;

static void store_error(std::string *error_message,
                        const std::string &message){
    if(error_message != nullptr) *error_message = message;
}

} // anonymous namespace

namespace sketch_fillets {

bool add_fillet(Sketch &sketch,
                std::size_t line_a_idx,
                std::size_t line_b_idx,
                double radius,
                std::string *error_message){
    const auto pi = std::acos(-1.0);
    if(!sketch.primitive(line_a_idx) || !sketch.primitive(line_b_idx)){
        store_error(error_message, "Fillet requires valid primitive indices");
        return false;
    }

    const auto *line_a = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(line_a_idx));
    const auto *line_b = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(line_b_idx));
    if(!line_a || !line_b){
        store_error(error_message, "Fillet requires two line primitives");
        return false;
    }

    if(!std::isfinite(radius) || (radius <= fillet_min_epsilon)){
        store_error(error_message, "Fillet radius must be finite and positive");
        return false;
    }

    Sketch::vertex_index_t shared_vertex = 0U;
    Sketch::vertex_index_t a_other = 0U;
    Sketch::vertex_index_t b_other = 0U;
    bool found = false;
    for(const auto va : line_a->vertices){
        for(const auto vb : line_b->vertices){
            if(va == vb){
                shared_vertex = va;
                a_other = (va == line_a->vertices[0]) ? line_a->vertices[1] : line_a->vertices[0];
                b_other = (vb == line_b->vertices[0]) ? line_b->vertices[1] : line_b->vertices[0];
                found = true;
                break;
            }
        }
        if(found) break;
    }
    if(!found){
        store_error(error_message, "Fillet requires two lines sharing a common vertex");
        return false;
    }

    const auto shared_pos = sketch.vertex(shared_vertex);
    const auto a_pos = sketch.vertex(a_other);
    const auto b_pos = sketch.vertex(b_other);

    const auto v1 = a_pos - shared_pos;
    const auto v2 = b_pos - shared_pos;
    const auto len1 = v1.length();
    const auto len2 = v2.length();

    if((len1 < fillet_min_epsilon) || (len2 < fillet_min_epsilon)){
        store_error(error_message, "Line segments too short for filleting");
        return false;
    }

    const auto cos_angle = std::clamp(v1.Dot(v2) / (len1 * len2), -1.0, 1.0);
    const auto angle = std::acos(cos_angle);

    if((std::abs(angle) < fillet_min_epsilon) || (std::abs(std::acos(-1.0) - angle) < fillet_min_epsilon)){
        store_error(error_message, "Lines are (nearly) collinear; cannot fillet");
        return false;
    }

    const auto half_angle = angle * 0.5;
    const auto sin_half = std::sin(half_angle);
    const auto tan_half = std::tan(half_angle);
    const auto tan_dist = radius / tan_half;
    const auto centre_dist = radius / sin_half;

    if((tan_dist >= len1) || (tan_dist >= len2)){
        store_error(error_message, "Fillet radius too large for the line segments");
        return false;
    }

    const auto u1 = v1 / len1;
    const auto u2 = v2 / len2;

    const auto bisector_raw = u1 + u2;
    const auto bisector_len = bisector_raw.length();
    if(bisector_len < fillet_min_epsilon){
        store_error(error_message, "Unable to compute angle bisector");
        return false;
    }
    const auto bisector = bisector_raw / bisector_len;

    const auto centre_pos = shared_pos + bisector * centre_dist;
    const auto tan1_pos = shared_pos + u1 * tan_dist;
    const auto tan2_pos = shared_pos + u2 * tan_dist;

    const auto centre_vertex = sketch.append_vertex(centre_pos);
    const auto tan1_vertex = sketch.append_vertex(tan1_pos);
    const auto tan2_vertex = sketch.append_vertex(tan2_pos);

    {
        auto *la = dynamic_cast<Sketch::line_primitive_t*>(sketch.primitive(line_a_idx));
        if(la){
            for(auto &v : la->vertices){
                if(v == shared_vertex) v = tan1_vertex;
            }
        }
    }
    {
        auto *lb = dynamic_cast<Sketch::line_primitive_t*>(sketch.primitive(line_b_idx));
        if(lb){
            for(auto &v : lb->vertices){
                if(v == shared_vertex) v = tan2_vertex;
            }
        }
    }

    sketch.add_arc(centre_vertex, tan1_vertex, tan2_vertex, Sketch::geometry_tag_t::normal);
    // Ensure the fillet arc takes the shorter (inside) path rather than wrapping
    // around the outside. The two tangent points define two possible arcs on
    // the circle; we want the arc whose sweep angle is <= pi.
    if(sketch.has_plane()){
        const auto last_arc_idx = sketch.primitive_count() - 1U;
        auto *arc = dynamic_cast<Sketch::arc_primitive_t*>(sketch.primitive(last_arc_idx));
        if(arc != nullptr){
            const auto arc_start_p = sketch.project(sketch.vertex(arc->start));
            const auto arc_stop_p = sketch.project(sketch.vertex(arc->stop));
            const auto centre_p = sketch.project(centre_pos);
            const auto start_angle = std::atan2(arc_start_p.v - centre_p.v, arc_start_p.u - centre_p.u);
            const auto stop_angle = std::atan2(arc_stop_p.v - centre_p.v, arc_stop_p.u - centre_p.u);
            double sweep = stop_angle - start_angle;
            if(sweep <= 0.0) sweep += 2.0 * pi;
            if(sweep > pi){
                std::swap(arc->start, arc->stop);
            }
        }
    }
    sketch.delete_unreferenced_vertices();
    sketch.refresh_geometry();
    return true;
}

} // namespace sketch_fillets
