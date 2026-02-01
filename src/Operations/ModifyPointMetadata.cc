// ModifyPointMetadata.cc - A part of DICOMautomaton 2026. Written by hal clark.

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

#include "ModifyPointMetadata.h"



OperationDoc OpArgDocModifyPointMetadata(){
    OperationDoc out;
    out.name = "ModifyPointMetadata";

    out.tags.emplace_back("category: point cloud processing");
    out.tags.emplace_back("category: metadata");

    out.desc = 
        "This operation injects metadata into point clouds.";


    out.args.emplace_back();
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "PointSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back() = MetadataInjectionOpArgDoc();
    out.args.back().name = "KeyValues";
    out.args.back().default_val = "";

    return out;
}

bool ModifyPointMetadata(Drover &DICOM_data,
                         const OperationArgPkg& OptArgs,
                         std::map<std::string, std::string>& /*InvocationMetadata*/,
                         const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto PointSelectionStr = OptArgs.getValueStr("PointSelection").value();

    const auto KeyValuesOpt = OptArgs.getValueStr("KeyValues");

    //-----------------------------------------------------------------------------------------------------------------

    // Parse user-provided metadata, if any has been provided.
    const auto key_values = parse_key_values(KeyValuesOpt.value_or(""));

    auto PCs_all = All_PCs( DICOM_data );
    auto PCs = Whitelist( PCs_all, PointSelectionStr );
    for(auto & pcp_it : PCs){
        // Insert a copy of the user-provided key-values, but pre-process to replace macros and evaluate known
        // functions.
        auto l_key_values = key_values;
        inject_metadata( (*pcp_it)->pset.metadata, std::move(l_key_values) );
    }

    return true;
}
