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
    out.back().name = "ParanoiaLevel";
    out.back().desc = "At low paranoia setting, only top-level UIDs are replaced."
                      " At medium paranoia setting, many UIDs, descriptions, and"
                      " labels are replaced, but the PatientID and FrameOfReferenceUID are retained."
                      " The high paranoia setting is the same as the medium setting, but the "
                      " PatientID and FrameOfReferenceUID are also replaced."
                      " (Note: this is not a full anonymization.)"
                      " Use the low setting if you want to retain linkage to the originating data set."
                      " Use the medium setting if you don't. Use the high setting if your TPS goes"
                      " overboard linking data sets by PatientID and/or FrameOfReferenceUID.";
    out.back().default_val = "medium";
    out.back().expected = true;
    out.back().examples = { "low", "medium", "high" };

    return out;
}

Drover
DICOMExportImagesAsDose(Drover DICOM_data, 
                        OperationArgPkg OptArgs,
                        std::map<std::string, std::string> /*InvocationMetadata*/,
                        std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto FilenameOut = OptArgs.getValueStr("Filename").value();    
    const auto ParanoiaStr = OptArgs.getValueStr("ParanoiaLevel").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto LowRegex  = std::regex("^lo?w?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto MedRegex  = std::regex("^me?d?i?u?m?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto HighRegex = std::regex("^hi?g?h?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    ParanoiaLevel p;
    if(false){
    }else if(std::regex_match(ParanoiaStr,LowRegex)){
        p = ParanoiaLevel::Low;
    }else if(std::regex_match(ParanoiaStr,MedRegex)){
        p = ParanoiaLevel::Medium;
    }else if(std::regex_match(ParanoiaStr,HighRegex)){
        p = ParanoiaLevel::High;
    }else{
        throw std::runtime_error("Specified paranoia level is not valid. Cannot continue.");
    }


    //-----------------------------------------------------------------------------------------------------------------

    if(!DICOM_data.image_data.empty()){
        try{
            Write_Dose_Array(DICOM_data.image_data.back(), FilenameOut, p);
        }catch(const std::exception &e){
            FUNCWARN("Unable to export Image_Array as DICOM RTDOSE file: '" << e.what() << "'");
        }
    }

    return std::move(DICOM_data);
}
