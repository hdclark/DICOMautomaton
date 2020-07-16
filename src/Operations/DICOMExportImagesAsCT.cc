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

#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>

#include "../Imebra_Shim.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorTAR.h"

#include "DICOMExportImagesAsCT.h"


OperationDoc OpArgDocDICOMExportImagesAsCT(){
    OperationDoc out;
    out.name = "DICOMExportImagesAsCT";
    out.desc = "This operation exports the selected Image_Array(s) to DICOM CT-modality files.";

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
    out.args.back().desc = "The filename (or full path name) to which the DICOM files should be written."
                           " The file format is a gzipped-TAR file containing multiple CT-modality files.";
    out.args.back().default_val = "CTs.tgz";
    out.args.back().expected = true;
    out.args.back().examples = { "/tmp/CTs.tgz", "./CTs.tar.gz", "CTs.tgz" };
    out.args.back().mimetype = "application/gzip";

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
                        const OperationArgPkg& OptArgs,
                        const std::map<std::string, std::string>& /*InvocationMetadata*/,
                        const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto FilenameOut = OptArgs.getValueStr("Filename").value();    
    const auto ParanoiaStr = OptArgs.getValueStr("ParanoiaLevel").value();

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

    auto make_sequential_filename = [=]() -> std::string {
        const auto pad_left_zeros = [](std::string in, long int desired_length) -> std::string {
            while(static_cast<long int>(in.length()) < desired_length) in = "0"_s + in;
            return in;
        };
        static long int n = 0; // Internal counter, incremented once per invocation.

        std::string prefix = "CT_";
        std::string middle = std::to_string(n);
        std::string suffix = ".dcm";
        ++n;
        return prefix + pad_left_zeros(middle,6) + suffix;
    };

    const auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );

    if(IAs.empty()){
        throw std::invalid_argument("No image arrays selected. Cannot continue.");
    }

    //-----------------------------------------------------------------------------------------------------------------

    {
        // Prepare an output stream for a tar archive.
        std::ofstream ofs(FilenameOut, std::ios::out | std::ios::trunc | std::ios::binary);
        if(!ofs) throw std::runtime_error("Unable to open file for writing");

        boost::iostreams::filtering_ostream ofsb;
        boost::iostreams::gzip_params gzparams(boost::iostreams::gzip::best_speed);
        ofsb.push(boost::iostreams::gzip_compressor(gzparams));
        ofsb.push(ofs);

        ustar_writer ustar(ofsb);

        for(auto & iap_it : IAs){
 
            // This closure is invoked once per CT file. We simply add each to the TAR file.
            auto file_handler = [&](std::istream &is,
                                    long int filesize) -> void {
                const auto fname = make_sequential_filename();
                const auto fsize = static_cast<long int>(filesize);
                ustar.add_file(is, fname, fsize);
                return;
            };

            try{
                Write_CT_Images(*iap_it, file_handler, p);
            }catch(const std::exception &e){
                FUNCWARN("Unable to export Image_Array as DICOM CT-modality files: '" << e.what() << "'");
            }
        }
        // TAR file finalization, stream flush, and file handle close all done automatically here.
    }

    return DICOM_data;
}
