//ConvertParametersToTable.cc - A part of DICOMautomaton 2023. Written by hal clark.

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

#include <filesystem>

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Metadata.h"
#include "../Thread_Pool.h"
#include "../Write_File.h"
#include "../String_Parsing.h"

#include "ConvertParametersToTable.h"


OperationDoc OpArgDocConvertParametersToTable(){
    OperationDoc out;
    out.name = "ConvertParametersToTable";

    out.desc = 
        "Convert one or more key-value parameters from the global parameter table into a table.";

    out.args.emplace_back();
    out.args.back().name = "KeySelection";
    out.args.back().desc = "A regular expression that will select key-values to include in the table."
                           "\n\n"
                           "This regular expression will be applied only to keys."
                           "";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", "a_specific_key", "^a_specific_prefix.*" };


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

bool ConvertParametersToTable(Drover& DICOM_data,
                      const OperationArgPkg& OptArgs,
                      std::map<std::string, std::string>& InvocationMetadata,
                      const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto KeySelectionStr = OptArgs.getValueStr("KeySelection").value();

    const auto TableLabel = OptArgs.getValueStr("TableLabel").value();
    const auto TableSelectionStr = OptArgs.getValueStr("TableSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto NormalizedTableLabel = X(TableLabel);

    const auto regex_key = Compile_Regex(KeySelectionStr);

    auto STs_all = All_STs( DICOM_data );
    auto STs = Whitelist( STs_all, TableSelectionStr );

    const bool create_new_table = STs.empty() || (*(STs.back()) == nullptr);
    std::shared_ptr<Sparse_Table> st = create_new_table ? std::make_shared<Sparse_Table>()
                                                        : *(STs.back());

    // Emit a header.
    int64_t row = st->table.next_empty_row();
    if(create_new_table){
        ++row;
        int64_t col = 1;
        st->table.inject(row, col++, "Key");
        st->table.inject(row, col++, "Value");
    }

    // Fill in the table.
    {
        for(const auto& kvp : InvocationMetadata){
            if(std::regex_match(kvp.first, regex_key)){
                ++row;
                int64_t col = 1;
                st->table.inject(row, col++, kvp.first);
                st->table.inject(row, col++, kvp.second);
            }
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

