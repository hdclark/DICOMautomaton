//BoostSerializeDrover.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <boost/filesystem.hpp>
#include <experimental/optional>
#include <list>
#include <map>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>    

#include "../Common_Boost_Serialization.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


std::list<OperationArgDoc> OpArgDocBoost_Serialize_Drover(void){
    std::list<OperationArgDoc> out;

    out.emplace_back();
    out.back().name = "Filename";
    out.back().desc = "The filename (or full path name) to which the serialized data should be written";
    out.back().default_val = "/tmp/boost_serialized_drover.bin.gz";
    out.back().expected = true;
    out.back().examples = { "/tmp/out.bin.gz", 
                            "./out.bin.gz",
                            "out.bin.gz" };
    out.back().mimetype = "application/octet-stream";

    return out;
}

Drover
Boost_Serialize_Drover(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string, std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    auto fname = OptArgs.getValueStr("Filename").value();    
    //-----------------------------------------------------------------------------------------------------------------

    const boost::filesystem::path apath(fname);

    const auto res = Common_Boost_Serialize_Drover(DICOM_data, apath);
    if(res){
        FUNCINFO("Dumped serialization to file " << apath);
    }else{
        throw std::runtime_error("Unable dump serialization to file " + apath.string());
    }

    return DICOM_data;
}
