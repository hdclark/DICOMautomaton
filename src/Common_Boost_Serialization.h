//Common_Boost_Serialization.h - A part of DICOMautomaton 2016. Written by hal clark.

#include <string>    

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "Structs.h"

#include "KineticModel_1Compartment2Input_5Param_LinearInterp_Common.h"
#include "KineticModel_1Compartment2Input_5Param_Chebyshev_Common.h"
#include "KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Common.h"


// --- Default Serialization routines.
bool
Common_Boost_Serialize_Drover(const Drover &in, boost::filesystem::path Filename);

bool
Common_Boost_Deserialize_Drover(Drover &out, boost::filesystem::path Filename);



// --- Specific Serialization Routines ---
// Prefer the 'default' serialization routine above.

bool
Common_Boost_Serialize_Drover_to_Gzip_Binary(const Drover &in, boost::filesystem::path Filename);
bool
Common_Boost_Serialize_Drover_to_Gzip_Simple_Text(const Drover &in, boost::filesystem::path Filename);
bool
Common_Boost_Serialize_Drover_to_Gzip_XML(const Drover &in, boost::filesystem::path Filename);

bool
Common_Boost_Serialize_Drover_to_Binary(const Drover &in, boost::filesystem::path Filename);
bool
Common_Boost_Serialize_Drover_to_Simple_Text(const Drover &in, boost::filesystem::path Filename);
bool
Common_Boost_Serialize_Drover_to_XML(const Drover &in, boost::filesystem::path Filename);



// --- Pharmacokinetic model state ---

// Single-compartment, dual-input, 5-parameter model with direct linear interpolation approach.
std::string 
Serialize(const KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters &state);

bool 
Deserialize(const std::string &s, KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters &state);


// Single-compartment, dual-input, 5-parameter model with Chebyshev polynomial approach.
std::string 
Serialize(const KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters &state);

bool 
Deserialize(const std::string &s, KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters &state);


// Single-compartment, dual-input, reduced-3-parameter model with Chebyshev polynomial approach.
std::string 
Serialize(const KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters &state);

bool 
Deserialize(const std::string &s, KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters &state);



