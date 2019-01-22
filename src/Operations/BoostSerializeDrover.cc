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


OperationDoc OpArgDocBoost_Serialize_Drover(void){
    OperationDoc out;
    out.name = "Boost_Serialize_Drover";
    out.desc = 
        "This operation exports all loaded state to a serialized format that can be loaded again later."
        " Is is especially useful for suspending long-running operations with intermittant interactive sub-operations.";


    out.args.emplace_back();
    out.args.back().name = "Filename";
    out.args.back().desc = "The filename (or full path name) to which the serialized data should be written."
                           " The file format is gzipped XML, which should be portable across most CPUs.";
    out.args.back().default_val = "/tmp/boost_serialized_drover.xml.gz";
    out.args.back().expected = true;
    out.args.back().examples = { "/tmp/out.xml.gz", 
                            "./out.xml.gz",
                            "out.xml.gz" };
    out.args.back().mimetype = "application/octet-stream";

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
