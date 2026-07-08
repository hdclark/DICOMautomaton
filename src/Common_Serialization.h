//Common_Serialization.h - A part of DICOMautomaton 2026. Written by hal clark.

#pragma once

#include <filesystem>
#include <string>

class Drover;
struct KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters;
struct KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters;
struct KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters;

// --- Default Serialization routines.
bool
Common_Serialize_Drover(const Drover &in, std::filesystem::path Filename);

bool
Common_Deserialize_Drover(Drover &out, const std::filesystem::path& Filename);


// --- Specific Serialization Routines ---
// Prefer the 'default' serialization routine above.

bool
Common_Serialize_Drover_to_Gzip_XML(const Drover &in, const std::filesystem::path& Filename);

bool
Common_Serialize_Drover_to_XML(const Drover &in, const std::filesystem::path& Filename);


// --- Kinetic model state serialization ---

std::string
Serialize(const KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters &state);

bool
Deserialize(const std::string &in,
            KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters &state);

std::string
Serialize(const KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters &state);

bool
Deserialize(const std::string &in,
            KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters &state);

std::string
Serialize(const KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters &state);

bool
Deserialize(const std::string &in,
            KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters &state);
