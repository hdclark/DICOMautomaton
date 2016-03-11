//BoostSerializeDrover.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <string>    
#include <map>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.

#include "../Structs.h"

#include "../Common_Boost_Serialization.h"

Drover
Boost_Serialize_Drover(Drover DICOM_data, std::map<std::string, std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    const boost::filesystem::path out_fname("/tmp/boost_serialized_drover.bin.gz");

    const auto res = Common_Boost_Serialize_Drover(DICOM_data, out_fname);
    if(res){
        FUNCINFO("Dumped serialization to file " << out_fname.string());
    }else{
        throw std::runtime_error("Unable dump serialization to file " + out_fname.string());
    }

    return std::move(DICOM_data);
}
