//CopyContours.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <deque>
#include <optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "Explicator.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"

#include "CopyContours.h"

OperationDoc OpArgDocCopyContours(){
    OperationDoc out;
    out.name = "CopyContours";

    out.tags.emplace_back("category: contour processing");

    out.desc = 
        "This operation deep-copies the selected contours.";

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
    out.args.back().name = "ROILabel";
    out.args.back().desc = "A label to attach to the copied ROI contours.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "copy", "duplicate", "bone", "roi_xyz" };

    return out;
}

bool CopyContours(Drover &DICOM_data,
                    const OperationArgPkg& OptArgs,
                    std::map<std::string, std::string>& /*InvocationMetadata*/,
                    const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();

    const auto ROILabel = OptArgs.getValueStr("ROILabel").value();

    //-----------------------------------------------------------------------------------------------------------------
    Explicator X(FilenameLex);
    const auto NormalizedROILabel = X(ROILabel);

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    // Copy to a separate buffer, just in case any part of the main Drover would be invalidated copying into it.
    std::shared_ptr<Contour_Data> contour_storage = std::make_shared<Contour_Data>();
    for(const auto &cc_refw : cc_ROIs){
        contour_storage->ccs.emplace_back( cc_refw );
    }

    for(auto& cc : contour_storage->ccs){
        cc.Insert_Metadata("ROIName", ROILabel);
        cc.Insert_Metadata("NormalizedROIName", NormalizedROILabel);
    }
    DICOM_data.Consume(contour_storage);

    return true;
}
