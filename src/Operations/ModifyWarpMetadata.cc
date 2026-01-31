//ModifyWarpMetadata.cc - A part of DICOMautomaton 2025. Written by hal clark.

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

#include "ModifyWarpMetadata.h"



OperationDoc OpArgDocModifyWarpMetadata(){
    OperationDoc out;
    out.name = "ModifyWarpMetadata";
    out.aliases.emplace_back("ModifyTransformMetadata");

    out.tags.emplace_back("category: spatial transform processing");
    out.tags.emplace_back("category: metadata");

    out.desc = 
        "This operation injects metadata into spatial transformations (i.e., warps).";


    out.args.emplace_back();
    out.args.back() = T3WhitelistOpArgDoc();
    out.args.back().name = "TransformSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back() = MetadataInjectionOpArgDoc();
    out.args.back().name = "KeyValues";
    out.args.back().default_val = "";

    return out;
}



bool ModifyWarpMetadata(Drover &DICOM_data,
                        const OperationArgPkg& OptArgs,
                        std::map<std::string, std::string>& /*InvocationMetadata*/,
                        const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TransformSelectionStr = OptArgs.getValueStr("TransformSelection").value();

    const auto KeyValuesOpt = OptArgs.getValueStr("KeyValues");

    //-----------------------------------------------------------------------------------------------------------------

    // Parse user-provided metadata, if any has been provided.
    const auto key_values = parse_key_values(KeyValuesOpt.value_or(""));

    auto T3s_all = All_T3s( DICOM_data );
    auto T3s = Whitelist( T3s_all, TransformSelectionStr );
    for(auto & t3p_it : T3s){
        // Insert a copy of the user-provided key-values, but pre-process to replace macros and evaluate known
        // functions.
        auto l_key_values = key_values;
        inject_metadata( (*t3p_it)->metadata, std::move(l_key_values) );
    }

    return true;
}
