//ReportROIData.cc - A part of DICOMautomaton 2023. Written by hal clark.

#include <algorithm>
#include <optional>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <set>
#include <memory>
#include <string>
#include <sstream>
#include <tuple>
#include <utility>            //Needed for std::pair.
#include <vector>
#include <filesystem>
#include <cstdint>

#include "Explicator.h"       //Needed for Explicator class.
#include "YgorMath.h"         //Needed for vec3 class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Metadata.h"

#include "ReportROIData.h"

OperationDoc OpArgDocReportROIData(){
    OperationDoc out;
    out.name = "ReportROIData";
    out.tags.emplace_back("category: table processing");
    out.tags.emplace_back("category: contour processing");

    out.desc = "This operation prints ROI contour information into a table.";


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
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";
    
    out.args.emplace_back();
    out.args.back().name = "TableLabel";
    out.args.back().desc = "A label to attach to table if and only if a new table is created.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "xyz", "sheet A" };

    return out;
}

bool ReportROIData(Drover &DICOM_data,
                   const OperationArgPkg& OptArgs,
                   std::map<std::string, std::string>& /*InvocationMetadata*/,
                   const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TableLabel = OptArgs.getValueStr("TableLabel").value();
    const auto TableSelectionStr = OptArgs.getValueStr("TableSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();
    //-----------------------------------------------------------------------------------------------------------------

    Explicator X(FilenameLex);

    const auto NormalizedTableLabel = X(TableLabel);

    auto STs_all = All_STs( DICOM_data );
    auto STs = Whitelist( STs_all, TableSelectionStr );

    const bool create_new_table = STs.empty() || (*(STs.back()) == nullptr);
    std::shared_ptr<Sparse_Table> st = create_new_table ? std::make_shared<Sparse_Table>()
                                                        : *(STs.back());

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }


    // Emit a header.
    int64_t row = st->table.next_empty_row();
    if(create_new_table){
        ++row;
        int64_t col = 1;
        //st->table.inject(row, col++, "PatientID");
        st->table.inject(row, col++, "NormalizedROILabel");
        st->table.inject(row, col++, "ROILabels");
    }

    std::map<std::string, std::set<std::string>> normroi_to_rois;
    for(const auto &cc_refw : cc_ROIs){
        for(const auto &c : cc_refw.get().contours){
            //const auto PatientID = get_as<std::string>(c.metadata, "PatientID").value_or("unspecified");
            const auto ROILabel  = get_as<std::string>(c.metadata, "ROIName").value_or("unspecified");
            const auto NROILabel = get_as<std::string>(c.metadata, "NormalizedROIName").value_or("unspecified");

            normroi_to_rois[NROILabel].insert(ROILabel);
        }
    }

    // Fill in the table.
    {
        for(const auto& kvp : normroi_to_rois){
            ++row;
            int64_t col = 1;
            st->table.inject(row, col++, kvp.first);

            std::stringstream ss;
            for(const auto& r : kvp.second){
                ss << (ss.str().empty() ? "" : ";") << r;
            }
            st->table.inject(row, col++, ss.str());
        }
    }
    
    // Inject the result into the Drover if not already.
    if(create_new_table){
        auto meta = coalesce_metadata_for_basic_table({}, meta_evolve::iterate);
        st->table.metadata = meta;
        st->table.metadata["TableLabel"] = TableLabel;
        st->table.metadata["NormalizedTableLabel"] = NormalizedTableLabel;
        st->table.metadata["Description"] = "Generated table";

        DICOM_data.table_data.emplace_back( st );
    }

    return true;
}
