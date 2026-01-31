//ModifyLineSampleMetadata.cc - A part of DICOMautomaton 2025. Written by hal clark.

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

#include "ModifyLineSampleMetadata.h"



OperationDoc OpArgDocModifyLineSampleMetadata(){
    OperationDoc out;
    out.name = "ModifyLineSampleMetadata";

    out.tags.emplace_back("category: line sample processing");
    out.tags.emplace_back("category: metadata");

    out.desc = 
        "This operation injects metadata into line samples.";


    out.args.emplace_back();
    out.args.back() = LSWhitelistOpArgDoc();
    out.args.back().name = "LineSampleSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back() = MetadataInjectionOpArgDoc();
    out.args.back().name = "KeyValues";
    out.args.back().default_val = "";

    return out;
}



bool ModifyLineSampleMetadata(Drover &DICOM_data,
                              const OperationArgPkg& OptArgs,
                              std::map<std::string, std::string>& /*InvocationMetadata*/,
                              const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto LineSampleSelectionStr = OptArgs.getValueStr("LineSampleSelection").value();

    const auto KeyValuesOpt = OptArgs.getValueStr("KeyValues");

    //-----------------------------------------------------------------------------------------------------------------

    // Parse user-provided metadata, if any has been provided.
    const auto key_values = parse_key_values(KeyValuesOpt.value_or(""));

    auto LSs_all = All_LSs( DICOM_data );
    auto LSs = Whitelist( LSs_all, LineSampleSelectionStr );
    for(auto & lsp_it : LSs){
        // Insert a copy of the user-provided key-values, but pre-process to replace macros and evaluate known
        // functions.
        auto l_key_values = key_values;
        inject_metadata( (*lsp_it)->metadata, std::move(l_key_values) );
    }

    return true;
}
