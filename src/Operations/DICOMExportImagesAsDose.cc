//DICOMExportImagesAsDose.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <string>    
#include <map>
#include <experimental/optional>
#include <regex>

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

    out.emplace_back();
    out.back().name = "ExtraParanoid";
    out.back().desc = "Normally only top-level UIDs are replaced."
                      " Enabling this mode causes all UIDs, descriptions, and"
                      " labels to be replaced. (Note: this is not a full anonymization.)";
    out.back().default_val = "false";
    out.back().expected = true;
    out.back().examples = { "true", "false" };

    return out;
}

Drover
DICOMExportImagesAsDose(Drover DICOM_data, 
                        OperationArgPkg OptArgs,
                        std::map<std::string, std::string> /*InvocationMetadata*/,
                        std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto FilenameOut = OptArgs.getValueStr("Filename").value();    
    const auto ExtraParanoidStr = OptArgs.getValueStr("ExtraParanoid").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto TrueRegex = std::regex("^tr?u?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto ExtraParanoid = std::regex_match(ExtraParanoidStr, TrueRegex);

    //-----------------------------------------------------------------------------------------------------------------

    if(!DICOM_data.image_data.empty()){
        try{
            Write_Dose_Array(DICOM_data.image_data.back(), FilenameOut, ExtraParanoid);
        }catch(const std::exception &e){
            FUNCWARN("Unable to export Image_Array as DICOM RTDOSE file: '" << e.what() << "'");
        }
    }

    return std::move(DICOM_data);
}
