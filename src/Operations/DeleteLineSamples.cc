//DeleteLineSamples.cc - A part of DICOMautomaton 2022. Written by hal clark.

#include <deque>
#include <optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "DeleteLineSamples.h"

OperationDoc OpArgDocDeleteLineSamples(){
    OperationDoc out;
    out.name = "DeleteLineSamples";
    out.tags.emplace_back("category: line sample processing");

    out.desc = 
        "This operation deletes the selected line samples.";

    out.args.emplace_back();
    out.args.back() = LSWhitelistOpArgDoc();
    out.args.back().name = "LineSelection";
    out.args.back().default_val = "last";
    
    return out;
}

bool DeleteLineSamples(Drover &DICOM_data,
                     const OperationArgPkg& OptArgs,
                     std::map<std::string, std::string>& /*InvocationMetadata*/,
                     const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto LineSelectionStr = OptArgs.getValueStr("LineSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    auto LSs_all = All_LSs( DICOM_data );
    const auto LSs = Whitelist( LSs_all, LineSelectionStr );
    for(auto & lsp_it : LSs){
        DICOM_data.lsamp_data.erase( lsp_it );
    }

    return true;
}
