//DeleteContours.cc - A part of DICOMautomaton 2021. Written by hal clark.

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

#include "DeleteContours.h"

OperationDoc OpArgDocDeleteContours(){
    OperationDoc out;
    out.name = "DeleteContours";

    out.tags.emplace_back("category: contour processing");

    out.desc = 
        "This operation deletes the selected contours.";

    out.notes.emplace_back(
        "Contours can be shallow copies that are shared amongst multiple Drover class instances."
        " Deleting contours in one Drover instance will delete them from all linked instances."
        " Typically, contours are deep-copied to avoid this problem, but be aware if using shallow"
        " copies."
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

    return out;
}

bool DeleteContours(Drover &DICOM_data,
                      const OperationArgPkg& OptArgs,
                      std::map<std::string, std::string>& /*InvocationMetadata*/,
                      const std::string& ){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );
    YLOGINFO("Selected " << cc_ROIs.size() << " contours");

    for(auto &cc_refw : cc_ROIs){
        bool found = false; // Stop searching after removing it because the reference becomes invalid.
        DICOM_data.Ensure_Contour_Data_Allocated();
        DICOM_data.contour_data->ccs.remove_if( [&](const contour_collection<double>& cc) -> bool {
            if( !found && (std::addressof(cc_refw.get()) == std::addressof(cc)) ){
                found = true;
                return true;
            }
            return false;
        });

        if(!found){
            throw std::logic_error("Selected contours not found. Cannot proceed.");
        }
    }
    cc_all.clear();
    cc_ROIs.clear();

    return true;
}
