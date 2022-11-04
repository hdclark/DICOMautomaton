//ExportSNCImages.cc - A part of DICOMautomaton 2022. Written by hal clark.

#include <asio.hpp>
#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImagesIO.h"
#include "YgorFilesDirs.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../SNC_File_Loader.h"

#include "ExportSNCImages.h"


OperationDoc OpArgDocExportSNCImages(){
    OperationDoc out;
    out.name = "ExportSNCImages";

    out.desc = 
        "This operation writes image arrays to ASCII 'SNC' formatted files.";

    out.notes.emplace_back(
        "Support for this format is ad-hoc. Metadata export is not supported."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
   

    out.args.emplace_back();
    out.args.back().name = "FilenameBase";
    out.args.back().desc = "The base filename that images will be written to."
                           " A sequentially-increasing number and file suffix are appended after the base filename."
                           " Note that the file type is ASCII SNC.";
    out.args.back().default_val = "/tmp/dcma_exportfitsimages";
    out.args.back().expected = true;
    out.args.back().examples = { "../somedir/out", 
                                 "/path/to/some/dir/file_prefix" };
    out.args.back().mimetype = "text/plain";

    return out;
}


bool ExportSNCImages(Drover &DICOM_data,
                     const OperationArgPkg& OptArgs,
                     std::map<std::string, std::string>& /*InvocationMetadata*/,
                     const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto FilenameBaseStr = OptArgs.getValueStr("FilenameBase").value();

    //-----------------------------------------------------------------------------------------------------------------

    const auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(const auto& iap_it : IAs){

        for(const auto& img : (*iap_it)->imagecoll.images){
            const auto fname = Get_Unique_Sequential_Filename(FilenameBaseStr + "_", 6, ".snc");
            FUNCINFO("Exporting image to file '" << fname << "' now..");

            std::ofstream os(fname, std::ios::out | std::ios::binary);
            if( !os.good()
            ||  !write_snc_file(os, img) ){
                FUNCWARN("Unable to write to file");
                return false;
            }
        }
    }

    return true;
}
