//GLSL_Shaders.cc - A part of DICOMautomaton 2025. Written by hal clark.

#include "GLSL_Shaders.h"

#include <cstdint>
#include <limits>
#include <string>

std::vector<glsl_shader_preset> get_glsl_shader_presets(){
    std::vector<glsl_shader_preset> presets;

    static const std::string invalid_hover_index =
        std::to_string(std::numeric_limits<uint32_t>::max()) + "u";
    const std::string hover_fragment_helpers =
        "uniform uint hovered_face_index;\n"
        "uniform uint hovered_edge_index;\n"
        "uniform usamplerBuffer primitive_face_indices;\n"
        "uniform usamplerBuffer primitive_edge_indices;\n"
        "\n"
        "const uint INVALID_HOVER_INDEX = " + invalid_hover_index + ";\n"
        "const vec3 HOVER_LIGHT_POSITION = vec3(1.0, 2.0, 3.0);\n"
        "\n"
        R"(
bool primitive_has_hovered_edge(){
    if(hovered_edge_index == INVALID_HOVER_INDEX){
        return false;
    }
    return any(equal(texelFetch(primitive_edge_indices, gl_PrimitiveID).xyz,
                     uvec3(hovered_edge_index)));
}

vec4 apply_hover_highlight(vec4 base_colour,
                           vec3 frag_pos_value,
                           vec3 frag_norm_value,
                           vec3 flat_norm_value,
                           bool use_smoothing_value){
    if(primitive_has_hovered_edge()){
        base_colour = vec4(mix(base_colour.rgb, vec3(0.40, 0.85, 1.00), 0.45), max(base_colour.a, 0.75));
    }
    if(hovered_face_index == INVALID_HOVER_INDEX){
        return base_colour;
    }
    if(texelFetch(primitive_face_indices, gl_PrimitiveID).r != hovered_face_index){
        return base_colour;
    }

    vec3 N = normalize(use_smoothing_value ? frag_norm_value : flat_norm_value);
    N = faceforward(N, vec3(0.0, 0.0, -1.0), N);
    vec3 L = normalize(HOVER_LIGHT_POSITION - frag_pos_value);
    float diff = max(dot(N, L), 0.0);
    vec3 shaded = mix(base_colour.rgb * 0.65, vec3(1.0, 0.95, 0.35), 0.45 + 0.55 * diff);
    return vec4(shaded, max(base_colour.a, 0.75));
}
)";
    const auto with_hover_support = [&hover_fragment_helpers](const char *fragment_shader) -> std::string {
        return hover_fragment_helpers + fragment_shader;
    };

    // --------------------------------- 1. Blinn-Phong (Standard Lighting) ---------------------------------
    presets.push_back({
        "Blinn-Phong",
        "Standard lighting with ambient, diffuse, and specular components."
        " Useful for seeing the overall form and identifying large-scale surface features.",

        // vertex shader
        R"(
in vec3 v_pos;
in vec3 v_norm;

uniform mat4 mvp_matrix;
uniform mat4 mv_matrix;
uniform mat3 norm_matrix;

out vec3 frag_pos;
out vec3 frag_norm;
flat out vec3 flat_norm;

void main(){
    gl_Position = mvp_matrix * vec4(v_pos, 1.0);
    vec4 p = mv_matrix * vec4(v_pos, 1.0);
    frag_pos = p.xyz / p.w;
    frag_norm = normalize(norm_matrix * v_norm);
    flat_norm = frag_norm;
}
)",

        // fragment shader
        with_hover_support(R"(
in vec3 frag_pos;
in vec3 frag_norm;
flat in vec3 flat_norm;

uniform vec4 diffuse_colour;
uniform vec4 user_colour;
uniform bool use_lighting;
uniform bool use_smoothing;

const vec3 LIGHT_POSITION = vec3(1.0, 2.0, 3.0);
const float AMBIENT_WEIGHT = 0.15;
const float DIFFUSE_WEIGHT = 0.65;
const float SPECULAR_WEIGHT = 0.20;
const float SPECULAR_POWER = 32.0;

out vec4 frag_colour;

void main(){
    vec4 base_colour = user_colour;
    if(use_lighting){
        vec3 N = normalize(use_smoothing ? frag_norm : flat_norm);
        N = faceforward(N, vec3(0.0, 0.0, -1.0), N);
        vec3 L = normalize(LIGHT_POSITION - frag_pos);
        vec3 V = normalize(-frag_pos);
        vec3 H = normalize(L + V);
        float diff = max(dot(N, L), 0.0);
        float spec = 0.0;
        if(diff > 0.0) spec = pow(max(dot(N, H), 0.0), SPECULAR_POWER);
        vec3 c = AMBIENT_WEIGHT * user_colour.rgb
               + DIFFUSE_WEIGHT * diff * diffuse_colour.rgb
               + SPECULAR_WEIGHT * spec * vec3(1.0);
        base_colour = vec4(c, user_colour.a);
    }
    frag_colour = apply_hover_highlight(base_colour, frag_pos, frag_norm, flat_norm, use_smoothing);
}
)")
    });

    // Shared vertex shader used by most presets.
    const std::string common_vert = presets.front().vertex_shader;

    // --------------------------------- 2. MatCap (Material Capture) ----------------------------------------
    presets.push_back({
        "MatCap",
        "Simulates a lit-sphere material capture using view-space normals."
        " High-contrast appearance makes it easy to spot pinching or irregular face distributions.",

        common_vert,

        // fragment shader
        with_hover_support(R"(
in vec3 frag_pos;
in vec3 frag_norm;
flat in vec3 flat_norm;

uniform vec4 user_colour;
uniform bool use_lighting;
uniform bool use_smoothing;

out vec4 frag_colour;

void main(){
    vec4 base_colour = user_colour;
    if(use_lighting){
        vec3 N = normalize(use_smoothing ? frag_norm : flat_norm);
        N = faceforward(N, vec3(0.0, 0.0, -1.0), N);
        vec2 uv = N.xy * 0.5 + 0.5;
        float r = length(uv - vec2(0.5));
        float highlight = smoothstep(0.5, 0.0, r);
        vec3 base = user_colour.rgb * (0.3 + 0.7 * highlight);
        float spec = pow(max(1.0 - 2.0 * r, 0.0), 3.0);
        vec3 c = base + vec3(0.4) * spec;
        base_colour = vec4(c, user_colour.a);
    }
    frag_colour = apply_hover_highlight(base_colour, frag_pos, frag_norm, flat_norm, use_smoothing);
}
)")
    });

    // --------------------------------- 3. Zebra Striping (Reflection Mapping) ------------------------------
    presets.push_back({
        "Zebra Striping",
        "Projects high-contrast black and white stripes via the reflection vector."
        " The gold standard for checking surface continuity (G1/G2).",

        common_vert,

        // fragment shader
        with_hover_support(R"(
in vec3 frag_pos;
in vec3 frag_norm;
flat in vec3 flat_norm;

uniform vec4 user_colour;
uniform bool use_lighting;
uniform bool use_smoothing;

out vec4 frag_colour;

void main(){
    vec4 base_colour = user_colour;
    if(use_lighting){
        vec3 N = normalize(use_smoothing ? frag_norm : flat_norm);
        N = faceforward(N, vec3(0.0, 0.0, -1.0), N);
        vec3 V = normalize(-frag_pos);
        vec3 R = reflect(-V, N);
        float stripe = sin(R.y * 25.0);
        float t = step(0.0, stripe);
        vec3 c = mix(vec3(0.05), vec3(0.95), t);
        base_colour = vec4(c, user_colour.a);
    }
    frag_colour = apply_hover_highlight(base_colour, frag_pos, frag_norm, flat_norm, use_smoothing);
}
)")
    });

    // --------------------------------- 4. Mean Curvature --------------------------------------------------
    presets.push_back({
        "Mean Curvature",
        "Colours the mesh based on approximate local curvature using screen-space derivatives."
        " Blue = flat, green = moderate, red = high curvature. Ideal for detecting surface noise.",

        common_vert,

        // fragment shader
        with_hover_support(R"(
in vec3 frag_pos;
in vec3 frag_norm;
flat in vec3 flat_norm;

uniform vec4 user_colour;
uniform bool use_lighting;
uniform bool use_smoothing;

out vec4 frag_colour;

void main(){
    vec4 base_colour = user_colour;
    if(use_lighting){
        vec3 N = normalize(frag_norm);
        N = faceforward(N, vec3(0.0, 0.0, -1.0), N);
        vec3 dNdx = dFdx(N);
        vec3 dNdy = dFdy(N);
        float curvature = length(dNdx) + length(dNdy);
        float t = clamp(curvature * 15.0, 0.0, 1.0);
        vec3 c;
        if(t < 0.5){
            c = mix(vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 0.0), t * 2.0);
        }else{
            c = mix(vec3(0.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), (t - 0.5) * 2.0);
        }
        base_colour = vec4(c, user_colour.a);
    }
    frag_colour = apply_hover_highlight(base_colour, frag_pos, frag_norm, flat_norm, use_smoothing);
}
)")
    });

    // --------------------------------- 5. Gooch Shading (Technical Illustration) ---------------------------
    presets.push_back({
        "Gooch Shading",
        "Cool-to-warm tones that mimic technical drawings."
        " Preserves detail in highlights and shadows, keeping silhouettes and internal edges visible.",

        common_vert,

        // fragment shader
        with_hover_support(R"(
in vec3 frag_pos;
in vec3 frag_norm;
flat in vec3 flat_norm;

uniform vec4 user_colour;
uniform bool use_lighting;
uniform bool use_smoothing;

const vec3 LIGHT_POSITION = vec3(1.0, 2.0, 3.0);

out vec4 frag_colour;

void main(){
    vec4 base_colour = user_colour;
    if(use_lighting){
        vec3 N = normalize(use_smoothing ? frag_norm : flat_norm);
        N = faceforward(N, vec3(0.0, 0.0, -1.0), N);
        vec3 L = normalize(LIGHT_POSITION - frag_pos);
        float NdotL = dot(N, L);
        float t = (NdotL + 1.0) * 0.5;
        vec3 cool = vec3(0.0, 0.0, 0.55) + 0.25 * user_colour.rgb;
        vec3 warm = vec3(0.3, 0.3, 0.0) + 0.5 * user_colour.rgb;
        vec3 c = mix(cool, warm, t);
        vec3 V = normalize(-frag_pos);
        float edge = max(dot(N, V), 0.0);
        c *= smoothstep(0.0, 0.3, edge) * 0.7 + 0.3;
        base_colour = vec4(c, user_colour.a);
    }
    frag_colour = apply_hover_highlight(base_colour, frag_pos, frag_norm, flat_norm, use_smoothing);
}
)")
    });

    // --------------------------------- 6. Normal Visualization ---------------------------------------------
    presets.push_back({
        "Normal Visualization",
        "Maps the surface normal XYZ components directly to RGB colour values."
        " Abrupt colour jumps indicate flipped normals or non-manifold geometry.",

        common_vert,

        // fragment shader
        with_hover_support(R"(
in vec3 frag_pos;
in vec3 frag_norm;
flat in vec3 flat_norm;

uniform vec4 user_colour;
uniform bool use_lighting;
uniform bool use_smoothing;

out vec4 frag_colour;

void main(){
    vec3 N = normalize(use_smoothing ? frag_norm : flat_norm);
    vec4 base_colour = vec4(N * 0.5 + 0.5, user_colour.a);
    frag_colour = apply_hover_highlight(base_colour, frag_pos, frag_norm, flat_norm, use_smoothing);
}
)")
    });

    // --------------------------------- 7. Wireframe-on-Shaded ---------------------------------------------
    presets.push_back({
        "Wireframe-on-Shaded",
        "Overlays detected triangle edges on a shaded surface using screen-space derivatives."
        " Useful for checking edge flow and detecting degenerate triangles.",

        common_vert,

        // fragment shader
        with_hover_support(R"(
in vec3 frag_pos;
in vec3 frag_norm;
flat in vec3 flat_norm;

uniform vec4 diffuse_colour;
uniform vec4 user_colour;
uniform bool use_lighting;
uniform bool use_smoothing;

const vec3 LIGHT_POSITION = vec3(1.0, 2.0, 3.0);

out vec4 frag_colour;

void main(){
    vec4 base_colour = user_colour;
    if(use_lighting){
        vec3 N = normalize(use_smoothing ? frag_norm : flat_norm);
        N = faceforward(N, vec3(0.0, 0.0, -1.0), N);
        vec3 L = normalize(LIGHT_POSITION - frag_pos);
        float diff = max(dot(N, L), 0.0);
        vec3 shaded = 0.15 * user_colour.rgb + 0.65 * diff * diffuse_colour.rgb;
        vec3 dNx = dFdx(frag_norm);
        vec3 dNy = dFdy(frag_norm);
        float edge = length(dNx) + length(dNy);
        float wire = smoothstep(0.0, 0.3, edge);
        vec3 c = mix(shaded, vec3(0.0, 1.0, 0.0), wire);
        base_colour = vec4(c, user_colour.a);
    }
    frag_colour = apply_hover_highlight(base_colour, frag_pos, frag_norm, flat_norm, use_smoothing);
}
)")
    });

    // --------------------------------- 8. Fresnel-Alpha Transparency --------------------------------------
    presets.push_back({
        "Fresnel-Alpha",
        "Emulates a glassy material with Fresnel-based transparency."
        " Edges are opaque and centres are transparent, useful for viewing occluded details.",

        common_vert,

        // fragment shader
        with_hover_support(R"(
in vec3 frag_pos;
in vec3 frag_norm;
flat in vec3 flat_norm;

uniform vec4 user_colour;
uniform bool use_lighting;
uniform bool use_smoothing;

const vec3 LIGHT_POSITION = vec3(1.0, 2.0, 3.0);

out vec4 frag_colour;

void main(){
    vec4 base_colour = user_colour;
    if(use_lighting){
        vec3 N = normalize(use_smoothing ? frag_norm : flat_norm);
        N = faceforward(N, vec3(0.0, 0.0, -1.0), N);
        vec3 V = normalize(-frag_pos);
        float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
        vec3 L = normalize(LIGHT_POSITION - frag_pos);
        float diff = max(dot(N, L), 0.0);
        vec3 c = user_colour.rgb * (0.15 + 0.65 * diff) + fresnel * vec3(0.3);
        float alpha = mix(0.1, 0.9, fresnel);
        base_colour = vec4(c, alpha);
    }
    frag_colour = apply_hover_highlight(base_colour, frag_pos, frag_norm, flat_norm, use_smoothing);
}
)")
    });

    return presets;
}
