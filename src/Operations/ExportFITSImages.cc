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

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "ExportFITSImages.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImagesIO.h"


OperationDoc OpArgDocExportFITSImages(void){
    OperationDoc out;
    out.name = "ExportFITSImages";

    out.desc = 
        "This operation writes image arrays to FITS-formatted image files.";

    out.notes.emplace_back(
        "Only pixel information and basic image positioning metadata are exported."
        " In particular, contours and arbitrary metadata are **not** exported by this routine."
        " (If a rendering of the image with contours drawn is needed, consult the PresentationImage operation.)"
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
    out.args.back().mimetype = "application/object-file-format"; // TODO: find correct MIME type.

    return out;
}


Drover ExportFITSImages(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto FilenameBaseStr = OptArgs.getValueStr("FilenameBase").value();

    //-----------------------------------------------------------------------------------------------------------------

    const auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        long int count = 0;
        for(auto &pimg : (*iap_it)->imagecoll.images){
            const auto pixel_dump_filename_out = Get_Unique_Sequential_Filename(FilenameBaseStr + "_", 6, ".fits");
            if(WriteToFITS(pimg, pixel_dump_filename_out)){
                FUNCINFO("Exported image " << count << " to file '" << pixel_dump_filename_out << "'");
            }else{
                FUNCWARN("Unable to export image to file '" << pixel_dump_filename_out << "'");
            }
            ++count;
        }
    }

    return DICOM_data;
}
