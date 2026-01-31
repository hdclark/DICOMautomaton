//ModifyRTPlanMetadata.cc - A part of DICOMautomaton 2025. Written by hal clark.

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

#include "ModifyRTPlanMetadata.h"



OperationDoc OpArgDocModifyRTPlanMetadata(){
    OperationDoc out;
    out.name = "ModifyRTPlanMetadata";

    out.tags.emplace_back("category: rtplan processing");
    out.tags.emplace_back("category: metadata");

    out.desc = 
        "This operation injects metadata into treatment plans.";


    out.args.emplace_back();
    out.args.back() = TPWhitelistOpArgDoc();
    out.args.back().name = "RTPlanSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back() = MetadataInjectionOpArgDoc();
    out.args.back().name = "KeyValues";
    out.args.back().default_val = "";

    return out;
}



bool ModifyRTPlanMetadata(Drover &DICOM_data,
                          const OperationArgPkg& OptArgs,
                          std::map<std::string, std::string>& /*InvocationMetadata*/,
                          const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto RTPlanSelectionStr = OptArgs.getValueStr("RTPlanSelection").value();

    const auto KeyValuesOpt = OptArgs.getValueStr("KeyValues");

    //-----------------------------------------------------------------------------------------------------------------

    // Parse user-provided metadata, if any has been provided.
    const auto key_values = parse_key_values(KeyValuesOpt.value_or(""));

    auto TPs_all = All_TPs( DICOM_data );
    auto TPs = Whitelist( TPs_all, RTPlanSelectionStr );
    for(auto & tp_it : TPs){
        // Insert a copy of the user-provided key-values, but pre-process to replace macros and evaluate known
        // functions.
        auto l_key_values = key_values;
        inject_metadata( (*tp_it)->metadata, std::move(l_key_values) );
    }

    return true;
}
