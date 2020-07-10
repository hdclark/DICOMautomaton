//Common_Boost_Serialization.h - A part of DICOMautomaton 2016. Written by hal clark.

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <string>    

#ifdef DCMA_USE_GNU_GSL
    #include "KineticModel_1Compartment2Input_5Param_Chebyshev_Common.h"
    #include "KineticModel_1Compartment2Input_5Param_LinearInterp_Common.h"
    #include "KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Common.h"
#endif // DCMA_USE_GNU_GSL

#include "Structs.h"

class Drover;

#ifdef DCMA_USE_GNU_GSL
    struct KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters;
    struct KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters;
    struct KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters;
#endif // DCMA_USE_GNU_GSL


// --- Default Serialization routines.
bool
Common_Boost_Serialize_Drover(const Drover &in, boost::filesystem::path Filename);

bool
Common_Boost_Deserialize_Drover(Drover &out, const boost::filesystem::path& Filename);



// --- Specific Serialization Routines ---
// Prefer the 'default' serialization routine above.

bool
Common_Boost_Serialize_Drover_to_Gzip_Binary(const Drover &in, const boost::filesystem::path& Filename);
bool
Common_Boost_Serialize_Drover_to_Gzip_Simple_Text(const Drover &in, const boost::filesystem::path& Filename);
bool
Common_Boost_Serialize_Drover_to_Gzip_XML(const Drover &in, const boost::filesystem::path& Filename);

bool
Common_Boost_Serialize_Drover_to_Binary(const Drover &in, const boost::filesystem::path& Filename);
bool
Common_Boost_Serialize_Drover_to_Simple_Text(const Drover &in, const boost::filesystem::path& Filename);
bool
Common_Boost_Serialize_Drover_to_XML(const Drover &in, const boost::filesystem::path& Filename);



#ifdef DCMA_USE_GNU_GSL
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
#endif // DCMA_USE_GNU_GSL


