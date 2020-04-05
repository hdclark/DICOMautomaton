//DICOMExportImagesAsDose.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <exception>
#include <optional>
#include <list>
#include <map>
#include <memory>
#include <ostream>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Imebra_Shim.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


OperationDoc OpArgDocDICOMExportImagesAsDose(void){
    OperationDoc out;
    out.name = "DICOMExportImagesAsDose";
    out.desc = "This operation exports the selected Image_Array to a DICOM dose file.";

    out.notes.emplace_back(
        "There are various 'paranoia' levels that can be used to partially anonymize the output."
        " In particular, most metadata and UIDs are replaced, but the files may still be recognized"
        " by a determined individual by comparing the coordinate system and pixel values."
        " Do NOT rely on this routine to fully anonymize the data!"
    );


    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "Filename";
    out.args.back().desc = "The filename (or full path name) to which the DICOM file should be written.";
    out.args.back().default_val = "/tmp/RD.dcm";
    out.args.back().expected = true;
    out.args.back().examples = { "/tmp/RD.dcm", "./RD.dcm", "RD.dcm" };
    out.args.back().mimetype = "application/dicom";

    out.args.emplace_back();
    out.args.back().name = "ParanoiaLevel";
    out.args.back().desc = "At low paranoia setting, only top-level UIDs are replaced."
                           " At medium paranoia setting, many UIDs, descriptions, and"
                           " labels are replaced, but the PatientID and FrameOfReferenceUID are retained."
                           " The high paranoia setting is the same as the medium setting, but the "
                           " PatientID and FrameOfReferenceUID are also replaced."
                           " (Note: this is not a full anonymization.)"
                           " Use the low setting if you want to retain linkage to the originating data set."
                           " Use the medium setting if you don't. Use the high setting if your TPS goes"
                           " overboard linking data sets by PatientID and/or FrameOfReferenceUID.";
    out.args.back().default_val = "medium";
    out.args.back().expected = true;
    out.args.back().examples = { "low", "medium", "high" };

    return out;
}

Drover
DICOMExportImagesAsDose(Drover DICOM_data, 
                        OperationArgPkg OptArgs,
                        std::map<std::string, std::string> /*InvocationMetadata*/,
                        std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto FilenameOut = OptArgs.getValueStr("Filename").value();    
    const auto ParanoiaStr = OptArgs.getValueStr("ParanoiaLevel").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto LowRegex  = Compile_Regex("^lo?w?$");
    const auto MedRegex  = Compile_Regex("^me?d?i?u?m?$");
    const auto HighRegex = Compile_Regex("^hi?g?h?$");

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

    const auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    IAs = Whitelist(IAs, "Modality", "RTDOSE");
    if(IAs.size() > 1){
        throw std::invalid_argument("Multiple image arrays were selected. This routine only supports export of a single image array.");
    }

    //-----------------------------------------------------------------------------------------------------------------

    for(auto & iap_it : IAs){
        try{
            Write_Dose_Array(*iap_it, FilenameOut, p);
        }catch(const std::exception &e){
            FUNCWARN("Unable to export Image_Array as DICOM RTDOSE file: '" << e.what() << "'");
        }
    }

    return DICOM_data;
}
