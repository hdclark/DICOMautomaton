//Common_Serialization.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "YgorIOgzip.h"
#include "YgorIOXMLSerialization.h"

#include "Common_Serialization.h"
#include "KineticModel_1Compartment2Input_5Param_Chebyshev_Common.h"
#include "KineticModel_1Compartment2Input_5Param_LinearInterp_Common.h"
#include "KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Common.h"
#include "Structs.h"
#include "StructsIOSerialization.h"

namespace {

bool
Common_Deserialize_Drover_from_XML_stream(Drover &out, std::istream &is){
    ygor::serialization::xml_iarchive ar(is);
    ar & ygor::serialization::make_nvp("dicom_data", out);
    return true;
}

template <typename T>
std::string
Serialize_to_XML_string(const T &state){
    auto tmp = state;
    std::stringstream ss;
    ygor::serialization::xml_oarchive ar(ss);
    ar & ygor::serialization::make_nvp("state", tmp);
    return ss.str();
}

template <typename T>
bool
Deserialize_from_XML_string(const std::string &in, T &state){
    try{
        std::stringstream ss(in);
        ygor::serialization::xml_iarchive ar(ss);
        ar & ygor::serialization::make_nvp("state", state);
    }catch(const std::exception &){
        return false;
    }
    return true;
}

} // namespace.


bool
Common_Serialize_Drover(const Drover &in,
                        std::filesystem::path Filename){
    return Common_Serialize_Drover_to_Gzip_XML(in, Filename);
}


bool
Common_Deserialize_Drover(Drover &out,
                          const std::filesystem::path& Filename){
    try{
        if(!std::filesystem::exists(Filename) || std::filesystem::file_size(Filename) == 0ULL) return false;
    }catch(const std::exception &){
        return false;
    }

    // Gzipped XML is the default format, but retain uncompressed XML input.
    try{
        std::ifstream ifs(Filename.string(), std::ios::binary);
        if(!ifs) return false;
        ygor::io::gzip_istream gzis(ifs);
        return Common_Deserialize_Drover_from_XML_stream(out, gzis);
    }catch(const std::exception &){ }

    try{
        std::ifstream ifs(Filename.string(), std::ios::binary);
        if(!ifs) return false;
        return Common_Deserialize_Drover_from_XML_stream(out, ifs);
    }catch(const std::exception &){ }

    return false;
}


bool
Common_Serialize_Drover_to_Gzip_XML(const Drover &in,
                                    const std::filesystem::path& Filename){
    try{
        std::ofstream ofs(Filename.string(), std::ios::binary | std::ios::trunc);
        if(!ofs) return false;

        ygor::io::gzip_ostream gzos(ofs);
        ygor::serialization::xml_oarchive ar(gzos);
        ar & ygor::serialization::make_nvp("dicom_data", in);
    }catch(const std::exception &){
        return false;
    }

    return true;
}


bool
Common_Serialize_Drover_to_XML(const Drover &in,
                               const std::filesystem::path& Filename){
    try{
        std::ofstream ofs(Filename.string(), std::ios::binary | std::ios::trunc);
        if(!ofs) return false;

        ygor::serialization::xml_oarchive ar(ofs);
        ar & ygor::serialization::make_nvp("dicom_data", in);
    }catch(const std::exception &){
        return false;
    }

    return true;
}


std::string
Serialize(const KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters &state){
    return Serialize_to_XML_string(state);
}


bool
Deserialize(const std::string &in,
            KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters &state){
    return Deserialize_from_XML_string(in, state);
}


std::string
Serialize(const KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters &state){
    return Serialize_to_XML_string(state);
}


bool
Deserialize(const std::string &in,
            KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters &state){
    return Deserialize_from_XML_string(in, state);
}


std::string
Serialize(const KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters &state){
    return Serialize_to_XML_string(state);
}


bool
Deserialize(const std::string &in,
            KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters &state){
    return Deserialize_from_XML_string(in, state);
}
