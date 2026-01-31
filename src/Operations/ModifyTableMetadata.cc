//ModifyTableMetadata.cc - A part of DICOMautomaton 2025. Written by hal clark.

#include <optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Metadata.h"

#include "ModifyTableMetadata.h"



OperationDoc OpArgDocModifyTableMetadata(){
    OperationDoc out;
    out.name = "ModifyTableMetadata";

    out.tags.emplace_back("category: table processing");
    out.tags.emplace_back("category: metadata");

    out.desc = 
        "This operation injects metadata into tables.";


    out.args.emplace_back();
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back() = MetadataInjectionOpArgDoc();
    out.args.back().name = "KeyValues";
    out.args.back().default_val = "";

    return out;
}



bool ModifyTableMetadata(Drover &DICOM_data,
                         const OperationArgPkg& OptArgs,
                         std::map<std::string, std::string>& /*InvocationMetadata*/,
                         const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TableSelectionStr = OptArgs.getValueStr("TableSelection").value();

    const auto KeyValuesOpt = OptArgs.getValueStr("KeyValues");

    //-----------------------------------------------------------------------------------------------------------------

    // Parse user-provided metadata, if any has been provided.
    const auto key_values = parse_key_values(KeyValuesOpt.value_or(""));

    auto STs_all = All_STs( DICOM_data );
    auto STs = Whitelist( STs_all, TableSelectionStr );
    for(auto & stp_it : STs){
        // Insert a copy of the user-provided key-values, but pre-process to replace macros and evaluate known
        // functions.
        auto l_key_values = key_values;
        inject_metadata( (*stp_it)->metadata, std::move(l_key_values) );
    }

    return true;
}
