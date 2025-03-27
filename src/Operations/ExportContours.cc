//ExportContours.cc - A part of DICOMautomaton 2022. Written by hal clark.

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

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMathIOOFF.h"
#include "YgorFilesDirs.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../Contour_Collection_File_Loader.h"

#include "ExportContours.h"

OperationDoc OpArgDocExportContours(){
    OperationDoc out;
    out.name = "ExportContours";
    out.tags.emplace_back("category: contour processing");
    out.tags.emplace_back("category: file export");

    out.desc = 
        "This operation writes contour collections to a file in a simplified text format."
        " Contour collections exported this way should round-trip, including metadata.";

    out.notes.emplace_back(
        "This operation does *not* export in DICOM format. Rather, it instead uses a custom plaintext format."
    );

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


    out.args.emplace_back();
    out.args.back().name = "FilenameBase";
    out.args.back().desc = "The base filename that contours will be written to."
                           " The file format is a simplified custom text format."
                           " Metadata is included, but will be base64 encoded if any non-printable"
                           " characters are detected. If no filename is given, the default will be used."
                           " A '_', a sequentially-increasing number, and the '.dat' file suffix are"
                           " appended after the base filename.";
    out.args.back().default_val = "/tmp/dcma_exportcontours";
    out.args.back().expected = true;
    out.args.back().examples = { "contours", 
                                 "../somedir/data", 
                                 "/path/to/some/selected_roi_contours" };
    out.args.back().mimetype = "text/plain";

    return out;
}



bool ExportContours(Drover &DICOM_data,
                         const OperationArgPkg& OptArgs,
                         std::map<std::string, std::string>& /*InvocationMetadata*/,
                         const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();

    const auto FilenameBaseStr = OptArgs.getValueStr("FilenameBase").value();

    //-----------------------------------------------------------------------------------------------------------------

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    // Determine which filename to use.
    const auto FN = Get_Unique_Sequential_Filename(FilenameBaseStr + "_", 6, ".dat");
    std::fstream FO(FN, std::fstream::out | std::ios::binary);

    if(!Write_Contour_Collections(cc_ROIs, FO)){
        throw std::runtime_error("Unable to write contours; emitter routine failed. Cannot continue.");
    }
    FO.flush();
    if(!FO){
        throw std::runtime_error("Unable to write contours; stream left in invalid state. Cannot continue.");
    }
    FO.close();

    YLOGINFO("Contours written to '" << FN << "'");

    return true;
}
