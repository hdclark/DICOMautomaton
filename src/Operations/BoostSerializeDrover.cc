//BoostSerializeDrover.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <string>    
#include <map>
#include <experimental/optional>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.

#include "../Structs.h"

#include "../Common_Boost_Serialization.h"

Drover
Boost_Serialize_Drover(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string, std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    auto fname_opt = OptArgs.getValueStr("filename");    
    const auto fname = fname_opt.value_or("/tmp/boost_serialized_drover.bin.gz");

    const boost::filesystem::path apath(fname);

    const auto res = Common_Boost_Serialize_Drover(DICOM_data, apath);
    if(res){
        FUNCINFO("Dumped serialization to file " << apath);
    }else{
        throw std::runtime_error("Unable dump serialization to file " + apath.string());
    }

    return std::move(DICOM_data);
}
