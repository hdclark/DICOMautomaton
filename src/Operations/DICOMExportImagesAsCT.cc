//DICOMExportImagesAsCT.cc - A part of DICOMautomaton 2017. Written by hal clark.

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
#include "YgorTAR.h"


OperationDoc OpArgDocDICOMExportImagesAsCT(void){
    OperationDoc out;
    out.name = "DICOMExportImagesAsCT";
    out.desc = "This operation exports the last Image_Array to DICOM CT-modality files.";

    out.notes.emplace_back(
        "There are various 'paranoia' levels that can be used to partially anonymize the output."
        " In particular, most metadata and UIDs are replaced, but the files may still be recognized"
        " by a determined individual by comparing the coordinate system and pixel values."
        " Do NOT rely on this routine to fully anonymize the data!"
    );


    out.args.emplace_back();
    out.args.back().name = "Filename";
    out.args.back().desc = "The filename (or full path name) to which the DICOM files should be written."
                           " The file format is a TAR file containing multiple CT-modality files.";
    out.args.back().default_val = "CTs.tar";
    out.args.back().expected = true;
    out.args.back().examples = { "/tmp/CTs.tar", 
                            "./CTs.tar",
                            "CTs.tar" };
    out.args.back().mimetype = "application/x-tar";

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
DICOMExportImagesAsCT(Drover DICOM_data, 
                        OperationArgPkg OptArgs,
                        std::map<std::string, std::string> /*InvocationMetadata*/,
                        std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
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


    //-----------------------------------------------------------------------------------------------------------------

    // Prepare an output stream for a tar archive.
    {
        std::ofstream ofs(FilenameOut, std::ios::out | std::ios::binary);
        if(!ofs) throw std::runtime_error("Unable to open TAR file for writing");
        ustar_archive_writer ustar(ofs);

        // This closure is invoked once per CT file. We simply add each to the TAR file.
        auto file_handler = [&](std::istream &is,
                                std::string suggested_filename,
                                long int filesize) -> void {
            const auto fname = suggested_filename;
            const auto fsize = static_cast<long int>(filesize);
            ustar.add_file(is, fname, fsize);
            return;
        };

        if(!DICOM_data.image_data.empty()){
            try{
                Write_CT_Images(DICOM_data.image_data.back(), file_handler, p);
            }catch(const std::exception &e){
                FUNCWARN("Unable to export Image_Array as DICOM CT-modality files: '" << e.what() << "'");
            }
        }
    } // TAR file finalization, stream flush, and file handle close all done automatically.

    return DICOM_data;
}
