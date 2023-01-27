//ExportFITSImages.cc - A part of DICOMautomaton 2019. Written by hal clark.

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
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImagesIO.h"
#include "YgorFilesDirs.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "ExportFITSImages.h"


OperationDoc OpArgDocExportFITSImages(){
    OperationDoc out;
    out.name = "ExportFITSImages";

    out.desc = 
        "This operation writes image arrays to FITS-formatted image files.";

    out.notes.emplace_back(
        "FITS images support lossless metadata export, but the metadata is embedded in a non-standard (but compliant)"
        " manner. Altering images using other software may result in invalidated metadata or (partial) removal of"
        " metadata."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
   

    out.args.emplace_back();
    out.args.back().name = "FilenameBase";
    out.args.back().desc = "The base filename that images will be written to."
                           " A sequentially-increasing number and file suffix are appended after the base filename."
                           " Note that the file type is FITS.";
    out.args.back().default_val = "/tmp/dcma_exportfitsimages";
    out.args.back().expected = true;
    out.args.back().examples = { "../somedir/out", 
                                 "/path/to/some/dir/file_prefix" };
    out.args.back().mimetype = "application/fits"; // According to FITSv4.0 specification.

    return out;
}


bool ExportFITSImages(Drover &DICOM_data,
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
        const auto fname = Get_Unique_Sequential_Filename(FilenameBaseStr + "_", 6, ".fits");

        YLOGINFO("Exporting " << (*iap_it)->imagecoll.images.size() << " images to file '" << fname << "' now..");
        if(WriteToFITS((*iap_it)->imagecoll, fname)){
            YLOGINFO("Exported image array to file '" << fname << "'");
        }else{
            YLOGWARN("Unable to export image array to file '" << fname << "'");
        }
    }

    return true;
}
