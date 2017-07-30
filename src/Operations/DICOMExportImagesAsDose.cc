//DICOMExportImagesAsDose.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <string>    
#include <map>
#include <experimental/optional>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.

#include "../Structs.h"

#include "../Imebra_Shim.h"


std::list<OperationArgDoc> OpArgDocDICOMExportImagesAsDose(void){
    std::list<OperationArgDoc> out;

    out.emplace_back();
    out.back().name = "Filename";
    out.back().desc = "The filename (or full path name) to which the DICOM file should be written.";
    out.back().default_val = "/tmp/RD.dcm";
    out.back().expected = true;
    out.back().examples = { "/tmp/RD.dcm", 
                            "./RD.dcm",
                            "RD.dcm" };

    return out;
}

Drover
DICOMExportImagesAsDose(Drover DICOM_data, 
                        OperationArgPkg OptArgs,
                        std::map<std::string, std::string> /*InvocationMetadata*/,
                        std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    auto fname = OptArgs.getValueStr("Filename").value();    
    //-----------------------------------------------------------------------------------------------------------------

    try{
        Write_Dose_Array(DICOM_data.image_data.back(), fname);
    }catch(const std::exception &e){
        FUNCWARN("Unable to export Image_Array as DICOM RTDOSE file: '" << e.what() << "'");
    }

    return std::move(DICOM_data);
}
