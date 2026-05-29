//GLSL_Shaders.h - A part of DICOMautomaton 2025. Written by hal clark.

#pragma once

#include <string>
#include <vector>

// A shader preset containing GLSL source for both vertex and fragment shaders.
// Note: the vertex_shader and fragment_shader strings do NOT contain a #version directive.
// The caller should prepend "#version <version>\n" as needed.
struct glsl_shader_preset {
    std::string name;
    std::string description;
    std::string vertex_shader;
    std::string fragment_shader;
};

// Returns the list of available shader presets.
// The first entry is always the default Blinn-Phong shader.
std::vector<glsl_shader_preset> get_glsl_shader_presets();

