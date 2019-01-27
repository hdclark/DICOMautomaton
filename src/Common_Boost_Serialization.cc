//Common_Boost_Serialization.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <boost/archive/basic_archive.hpp>
//For binary archives.
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/polymorphic_iarchive.hpp>
#include <boost/archive/polymorphic_oarchive.hpp>
//For plain-text archives.
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
//For XML archives.
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/math/special_functions/nonfinite_num_facets.hpp>
#include <boost/serialization/nvp.hpp>
#include <exception>
#include <fstream>
#include <string>    

#include "Common_Boost_Serialization.h"
//#include "YgorMathChebyshevIOBoostSerialization.h"
#include "KineticModel_1Compartment2Input_5Param_Chebyshev_Common.h"
#include "KineticModel_1Compartment2Input_5Param_LinearInterp_Common.h"
#include "KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Common.h"
#include "Structs.h"
#include "StructsIOBoostSerialization.h"

namespace boost {
namespace iostreams {
struct output;
}  // namespace iostreams
}  // namespace boost


bool
Common_Boost_Serialize_Drover(const Drover &in,
                              boost::filesystem::path Filename){

    //This routine serializes the entire Drover class to a single file.
    //
    // NOTE: Gzipped XML output is the default. XML dumps are large, but binary dumps are not portable.
    //       If you need to transport this data, inspect it manually, or ad-hoc transfer the data to another
    //       program that does not understand the file as-is, you should use an accompanying helper routine 
    //       to convert the format. In other words, you should not alter how *this* routine writes data.
    //

    return Common_Boost_Serialize_Drover_to_Gzip_XML(in,Filename);
}


bool
Common_Boost_Deserialize_Drover(Drover &out,
                                boost::filesystem::path Filename){

    //This routine attempts to deserialize an entire Drover class from a single file.
    //
    // An attempt was made to try parsing every possible combination of the default Boost.Serialization 
    // archives, namely
    //   - binary
    //   - XML
    //   - simple text
    // with one of
    //   - gzip compression
    //   - no compression.
    //   
    // This routine will try opening the file multiple times until the correct combination (if any) 
    // is found. The most anticipated combinations are therefore first.
    //
    // NOTE: By default, Boost.Serialize cannot deserialize NaN or +-Inf in text or xml. If you try, you get
    //       an unspecific invalid_input_stream exception with description: 'input stream error'. A 
    //       workaround is implemented here that uses Boost.Math locale facets to coax native reading and
    //       writing of NaN and +-Inf. 
    //

    {
        //Filter out non-reachable files.
        std::ifstream fi(Filename.string(), std::ios::binary | std::ios::ate);
        if(!fi) return false;

        //Filter out zero-length archives -- TODO: Needed with Boost.Serialize?
        const auto length = fi.tellg();
        if(length == 0) return false;
    }

    //XML, gzip compression.
    try{
        std::ifstream ifs(Filename.string(), std::ios::in | std::ios::binary);

        boost::iostreams::filtering_istream ifsb;
        ifsb.imbue(std::locale(std::locale().classic(), new boost::math::nonfinite_num_get<char>));
        ifsb.push(boost::iostreams::gzip_decompressor());
        ifsb.push(ifs);
 
        {
            boost::archive::xml_iarchive ar(ifsb, boost::archive::no_codecvt);
            ar & boost::serialization::make_nvp("dicom_data", out);
        }
        return true;
    }catch(const std::exception &){ }

    //Simple text, gzip compression.
    try{
        std::ifstream ifs(Filename.string(), std::ios::in | std::ios::binary);

        boost::iostreams::filtering_istream ifsb;
        ifsb.imbue(std::locale(std::locale().classic(), new boost::math::nonfinite_num_get<char>));
        ifsb.push(boost::iostreams::gzip_decompressor());
        ifsb.push(ifs);

        {
            boost::archive::text_iarchive ar(ifsb, boost::archive::no_codecvt);
            ar & boost::serialization::make_nvp("dicom_data", out);
        }
        return true;
    }catch(const std::exception &){ }

    //Binary, gzip compression.
    try{
        std::ifstream ifs(Filename.string(), std::ios::in | std::ios::binary);

        boost::iostreams::filtering_istream ifsb;
        ifsb.push(boost::iostreams::gzip_decompressor());
        ifsb.push(ifs);

        {
            boost::archive::binary_iarchive ar(ifsb);
            ar & boost::serialization::make_nvp("dicom_data", out);
        }
        return true;
    }catch(const std::exception &){ }

    //Binary, no compression.
    try{
        std::ifstream ifs(Filename.string(), std::ios::in | std::ios::binary);

        {
            boost::archive::binary_iarchive ar(ifs);
            ar & boost::serialization::make_nvp("dicom_data", out);
        }
        return true;
    }catch(const std::exception &){ }

    //Simple text, no compression.
    try{
        std::ifstream ifs(Filename.string(), std::ios::in);
        ifs.imbue(std::locale(std::locale().classic(), new boost::math::nonfinite_num_get<char>));

        {
            boost::archive::text_iarchive ar(ifs, boost::archive::no_codecvt);
            ar & boost::serialization::make_nvp("dicom_data", out);
        }
        return true;
    }catch(const std::exception &){ }

    //XML, no compression.
    try{
        std::ifstream ifs(Filename.string(), std::ios::in);
        ifs.imbue(std::locale(std::locale().classic(), new boost::math::nonfinite_num_get<char>));

        {
            boost::archive::xml_iarchive ar(ifs, boost::archive::no_codecvt);
            ar & boost::serialization::make_nvp("dicom_data", out);
        }
        return true;
    }catch(const std::exception &){ }

    //Unknown serialization file. Cannot open. Signal failure.
    return false;
}

//------------------


bool
Common_Boost_Serialize_Drover_to_Gzip_Binary(const Drover &in,
                                             boost::filesystem::path Filename){

    try{
        std::ofstream ofs(Filename.string(), std::ios::trunc | std::ios::binary);

        boost::iostreams::filtering_streambuf<boost::iostreams::output> ofsb;
        boost::iostreams::gzip_params gzparams(boost::iostreams::gzip::best_speed);
        ofsb.push(boost::iostreams::gzip_compressor(gzparams));
        ofsb.push(ofs);

        {
            boost::archive::binary_oarchive ar(ofsb);
            ar & boost::serialization::make_nvp("dicom_data", in);
        }
    }catch(const std::exception &e){
        return false;
    }

    return true;
}


bool
Common_Boost_Serialize_Drover_to_Gzip_Simple_Text(const Drover &in,
                                                  boost::filesystem::path Filename){

    try{
        std::ofstream ofs(Filename.string(), std::ios::trunc | std::ios::binary);

        boost::iostreams::filtering_ostream ofsb;
        boost::iostreams::gzip_params gzparams(boost::iostreams::gzip::best_speed);
        ofsb.imbue(std::locale(std::locale().classic(), new boost::math::nonfinite_num_put<char>));
        ofsb.push(boost::iostreams::gzip_compressor(gzparams));
        ofsb.push(ofs);

        {
            boost::archive::text_oarchive ar(ofsb, boost::archive::no_codecvt);
            ar & boost::serialization::make_nvp("dicom_data", in);
        }
    }catch(const std::exception &e){
        return false;
    }

    return true;
}


bool
Common_Boost_Serialize_Drover_to_Gzip_XML(const Drover &in,
                                          boost::filesystem::path Filename){

    try{
        std::ofstream ofs(Filename.string(), std::ios::trunc | std::ios::binary);

        boost::iostreams::filtering_ostream ofsb;
        boost::iostreams::gzip_params gzparams(boost::iostreams::gzip::best_speed);
        ofsb.imbue(std::locale(std::locale().classic(), new boost::math::nonfinite_num_put<char>));
        ofsb.push(boost::iostreams::gzip_compressor(gzparams));
        ofsb.push(ofs);

        {
            boost::archive::xml_oarchive ar(ofsb, boost::archive::no_codecvt);
            ar & boost::serialization::make_nvp("dicom_data", in);
        }
    }catch(const std::exception &e){
        return false;
    }

    return true;
}


bool
Common_Boost_Serialize_Drover_to_Binary(const Drover &in,
                                        boost::filesystem::path Filename){

    try{
        std::ofstream ofs(Filename.string(), std::ios::trunc | std::ios::binary);

        {
            boost::archive::binary_oarchive ar(ofs);
            ar & boost::serialization::make_nvp("dicom_data", in);
        }
    }catch(const std::exception &e){
        return false;
    }

    return true;
}


bool
Common_Boost_Serialize_Drover_to_Simple_Text(const Drover &in,
                                             boost::filesystem::path Filename){

    try{
        std::ofstream ofs(Filename.string(), std::ios::trunc);
        ofs.imbue(std::locale(std::locale().classic(), new boost::math::nonfinite_num_put<char>));

        {
            boost::archive::text_oarchive ar(ofs, boost::archive::no_codecvt);
            ar & boost::serialization::make_nvp("dicom_data", in);
        }
    }catch(const std::exception &e){
        return false;
    }

    return true;
}


bool
Common_Boost_Serialize_Drover_to_XML(const Drover &in,
                                     boost::filesystem::path Filename){

    try{
        std::ofstream ofs(Filename.string(), std::ios::trunc);
        ofs.imbue(std::locale(std::locale().classic(), new boost::math::nonfinite_num_put<char>));

        {
            boost::archive::xml_oarchive ar(ofs, boost::archive::no_codecvt);
            ar & boost::serialization::make_nvp("dicom_data", in);
        }
    }catch(const std::exception &e){
        return false;
    }

    return true;
}


//=====================================================================================================================

std::string 
Serialize(const KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters &state){
    //Will throw if serialization fails.

    std::stringstream ss;
    ss.imbue(std::locale(std::locale().classic(), new boost::math::nonfinite_num_put<char>));

    {
        boost::archive::text_oarchive ar(ss, boost::archive::no_codecvt);
        ar & boost::serialization::make_nvp("model_state", state);
    }

    return ss.str();
}


bool
Deserialize(const std::string &s,
            KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters &state){

    //Simple text, no compression.
    try{
        std::stringstream ss(s);
        ss.imbue(std::locale(std::locale().classic(), new boost::math::nonfinite_num_get<char>));

        {
            boost::archive::text_iarchive ar(ss, boost::archive::no_codecvt);
            ar & boost::serialization::make_nvp("model_state", state);
        }
        return true;
    }catch(const std::exception &){ }

    return false;
}                                                                            


std::string 
Serialize(const KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters &state){
    //Will throw if serialization fails.

    std::stringstream ss;
    ss.imbue(std::locale(std::locale().classic(), new boost::math::nonfinite_num_put<char>));

    {
        boost::archive::text_oarchive ar(ss, boost::archive::no_codecvt);
        ar & boost::serialization::make_nvp("model_state", state);
    }

    return ss.str();
}


bool
Deserialize(const std::string &s,
            KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters &state){

    //Simple text, no compression.
    try{
        std::stringstream ss(s);
        ss.imbue(std::locale(std::locale().classic(), new boost::math::nonfinite_num_get<char>));

        {
            boost::archive::text_iarchive ar(ss, boost::archive::no_codecvt);
            ar & boost::serialization::make_nvp("model_state", state);
        }
        return true;
    }catch(const std::exception &){ }

    return false;
}                                                                            


std::string 
Serialize(const KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters &state){
    //Will throw if serialization fails.

    std::stringstream ss;
    ss.imbue(std::locale(std::locale().classic(), new boost::math::nonfinite_num_put<char>));

    {
        boost::archive::text_oarchive ar(ss, boost::archive::no_codecvt);
        ar & boost::serialization::make_nvp("model_state", state);
    }

    return ss.str();
}


bool
Deserialize(const std::string &s,
            KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters &state){

    //Simple text, no compression.
    try{
        std::stringstream ss(s);
        ss.imbue(std::locale(std::locale().classic(), new boost::math::nonfinite_num_get<char>));

        {
            boost::archive::text_iarchive ar(ss, boost::archive::no_codecvt);
            ar & boost::serialization::make_nvp("model_state", state);
        }
        return true;
    }catch(const std::exception &){ }

    return false;
}                                                                            

