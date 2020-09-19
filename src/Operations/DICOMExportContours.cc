// DICOMExportContours.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <exception>
#include <optional>
#include <list>
#include <map>
#include <memory>
#include <iosfwd>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Imebra_Shim.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


OperationDoc OpArgDocDICOMExportContours(){
    OperationDoc out;
    out.name = "DICOMExportContours";
    out.desc = "This operation exports the selected contours to a DICOM RTSTRUCT-modality file.";

    out.notes.emplace_back(
        "There are various 'paranoia' levels that can be used to partially anonymize the output."
        " In particular, most metadata and UIDs are replaced, but the files may still be recognized"
        " by a determined individual by comparing the contour data."
        " Do NOT rely on this routine to fully anonymize the data!"
    );


    out.args.emplace_back();
    out.args.back().name = "Filename";
    out.args.back().desc = "The filename (or full path name) to which the DICOM file should be written.";
    out.args.back().default_val = "/tmp/RTSTRUCT.dcm";
    out.args.back().expected = true;
    out.args.back().examples = { "/tmp/RTSTRUCT.dcm", "./RTSTRUCT.dcm", "RTSTRUCT.dcm" };
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

    out.args.emplace_back();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                           " all available ROIs. Be aware that input spaces are trimmed to a single space."
                           " If your ROI name has more than two sequential spaces, use regex to avoid them."
                           " All ROIs have to match the single regex, so use the 'or' token if needed."
                           " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "ROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                           " all available ROIs. Be aware that input spaces are trimmed to a single space."
                           " If your ROI name has more than two sequential spaces, use regex to avoid them."
                           " All ROIs have to match the single regex, so use the 'or' token if needed."
                           " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                                 R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                                 R"***(left_parotid|right_parotid)***" };

    return out;
}

Drover
DICOMExportContours(const Drover &DICOM_data, 
                    const OperationArgPkg& OptArgs,
                    const std::map<std::string, std::string>& /*InvocationMetadata*/,
                    const std::string& /*FilenameLex*/, const std::list<OperationArgPkg>& /*Children*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto FilenameOut = OptArgs.getValueStr("Filename").value();    
    const auto ParanoiaStr = OptArgs.getValueStr("ParanoiaLevel").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto LowRegex  = Compile_Regex("^lo?w?$");
    const auto MedRegex  = Compile_Regex("^me?d?i?u?m?$");
    const auto HighRegex = Compile_Regex("^hi?g?h?$");

    ParanoiaLevel p;
    if(std::regex_match(ParanoiaStr,LowRegex)){
        p = ParanoiaLevel::Low;
    }else if(std::regex_match(ParanoiaStr,MedRegex)){
        p = ParanoiaLevel::Medium;
    }else if(std::regex_match(ParanoiaStr,HighRegex)){
        p = ParanoiaLevel::High;
    }else{
        throw std::runtime_error("Specified paranoia level is not valid. Cannot continue.");
    }


    //-----------------------------------------------------------------------------------------------------------------

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    try{
        // This closure is invoked to handle writing the RTSTRUCT file.
        auto file_handler = [FilenameOut](std::istream &is,
                                          long int /*filesize*/) -> void {
            std::ofstream ofs(FilenameOut, std::ios::out | std::ios::binary);
            if(!ofs) throw std::runtime_error("Unable to open file for writing.");
            ofs << is.rdbuf();
            ofs.flush();
            if(!ofs) throw std::runtime_error("File stream not in good state after emitting DICOM file.");
            return;
        };

        Write_Contours(cc_ROIs, file_handler, p);

    }catch(const std::exception &e){
        FUNCWARN("Unable to export contours as DICOM RTSTRUCT file: '" << e.what() << "'");
    }

    return DICOM_data;
}

