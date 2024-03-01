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
#include <cstdint>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"

#include "../Structs.h"
#include "../DCMA_DICOM.h"
#include "../Imebra_Shim.h"
#include "../Regex_Selectors.h"


OperationDoc OpArgDocDICOMExportContours(){
    OperationDoc out;
    out.name = "DICOMExportContours";
    out.desc = "This operation exports the selected contours to a DICOM RTSTRUCT-modality file.";

    out.notes.emplace_back(
        "There are various 'paranoia' levels that can be used to partially"
        " de-identify / anonymize the output for purposes of retaining or breaking linkage"
        " to originating data sets.."
        " Note that the 'paranoia' option is **not** sufficient to de-identify / anonymize data since other tags"
        " may contain personally identifying information."
        " Beyond metadata and UIDs, personally identifying information may still be found"
        " by a determined individual via inspection of the contour vertex data."
        " **Do not rely on this routine to de-identify / anonymize data.**"
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
    out.args.back().desc = "Controls how metadata is emitted."
                           "\n\n"
                           "At the `low` paranoia setting, top-level UIDs are replaced."
                           " Use the `low` setting if you want to retain linkage to the originating data set."
                           "\n\n"
                           "At low `medium` paranoia setting, many UIDs, descriptions, and"
                           " labels are replaced, but the PatientID and FrameOfReferenceUID are retained."
                           " Use the `medium` setting if you do not want to retain linkage to the originating"
                           " data set."
                           "\n\n"
                           "The low `high` paranoia setting is the same as the `medium` setting, but the "
                           " PatientID and FrameOfReferenceUID are also replaced."
                           " Use the `high` setting if your treatment planning system or other processing"
                           " software goes overboard linking data sets using (possibly only) PatientID"
                           " and/or FrameOfReferenceUID.";
    out.args.back().default_val = "medium";
    out.args.back().expected = true;
    out.args.back().examples = { "low", "medium", "high" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "Encoding";
    out.args.back().desc = "Controls the transfer syntax of the emitted DICOM file."
                           " Options include `explicit` and `implicit`."
                           "\n\n"
                           "`Explicit` transfer syntax directly encodes the DICOM Value Representation (VR)"
                           " types of tags, which can make them appropriate for distribution and archival."
                           " However, `explicit` transfer syntax imposes limits on some VR types (e.g., maximum"
                           " string length) and will likely produce larger files than `implicit` transfer syntax."
                           "\n\n"
                           "`Implicit` transfer syntax does not encode the DICOM VR, instead writing tags in"
                           " a standardized way. However, the DICOM dictionary, which codifies this encoding,"
                           " may differ from implementation to implementation or over time. `Implicit` transfer"
                           " syntax will likely produce smaller files than `explicit` transfer syntax, but there"
                           " is increased risk of data misinterpretation."
                           "\n\n"
                           "Note that little-endian encoding is always used.";
    out.args.back().default_val = "explicit";
    out.args.back().expected = true;
    out.args.back().examples = { "explicit", "implicit" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = CCWhitelistOpArgDoc();
    out.args.back().name = "ROISelection";
    out.args.back().default_val = "all";

    return out;
}

bool DICOMExportContours(Drover &DICOM_data,
                           const OperationArgPkg& OptArgs,
                           std::map<std::string, std::string>& /*InvocationMetadata*/,
                           const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto FilenameOut = OptArgs.getValueStr("Filename").value();    
    const auto ParanoiaStr = OptArgs.getValueStr("ParanoiaLevel").value();
    const auto EncodingStr = OptArgs.getValueStr("Encoding").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto LowRegex  = Compile_Regex("^lo?w?$");
    const auto MedRegex  = Compile_Regex("^me?d?i?u?m?$");
    const auto HighRegex = Compile_Regex("^hi?g?h?$");

    const auto ExRegex = Compile_Regex("^ex?p?l?i?c?i?t?$");
    const auto ImRegex = Compile_Regex("^im?p?l?i?c?i?t?$");

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

    DCMA_DICOM::Encoding enc = DCMA_DICOM::Encoding::ELE;
    if(std::regex_match(EncodingStr,ExRegex)){
        enc = DCMA_DICOM::Encoding::ELE;
    }else if(std::regex_match(EncodingStr,ImRegex)){
        enc = DCMA_DICOM::Encoding::ILE;
    }else{
        throw std::runtime_error("Specified encoding is not valid. Cannot continue.");
    }


    //-----------------------------------------------------------------------------------------------------------------

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    // This closure is invoked to handle writing the RTSTRUCT file.
    auto file_handler = [FilenameOut,enc,p](std::istream &is,
                                            int64_t /*filesize*/) -> void {
        std::ofstream ofs(FilenameOut, std::ios::out | std::ios::binary);
        if(!ofs) throw std::runtime_error("Unable to open file for writing.");
        ofs << is.rdbuf();
        ofs.flush();
        if(!ofs) throw std::runtime_error("File stream not in good state after emitting DICOM file.");
        return;
    };
    try{
        Write_Contours(cc_ROIs, file_handler, enc, p);
    }catch(const std::exception &e){
        YLOGWARN("Unable to export contours as DICOM RTSTRUCT file: '" << e.what() << "'");
    }

    return true;
}

