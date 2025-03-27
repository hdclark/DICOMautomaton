//CopyLineSamples.cc - A part of DICOMautomaton 2022. Written by hal clark.

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
#include "CopyLineSamples.h"

OperationDoc OpArgDocCopyLineSamples(){
    OperationDoc out;
    out.name = "CopyLineSamples";
    out.tags.emplace_back("category: line sample processing");

    out.desc = 
        "This operation deep-copies the selected line samples.";

    out.args.emplace_back();
    out.args.back() = LSWhitelistOpArgDoc();
    out.args.back().name = "LineSelection";
    out.args.back().default_val = "last";
    
    return out;
}

bool CopyLineSamples(Drover &DICOM_data,
                     const OperationArgPkg& OptArgs,
                     std::map<std::string, std::string>& /*InvocationMetadata*/,
                     const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto LineSelectionStr = OptArgs.getValueStr("LineSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    // Gather a list of objects to work on.
    std::deque<std::shared_ptr<Line_Sample>> lss_to_copy;

    auto LSs_all = All_LSs( DICOM_data );
    const auto LSs = Whitelist( LSs_all, LineSelectionStr );
    for(auto & lsp_it : LSs){
        lss_to_copy.push_back(*lsp_it);
    }

    // Perform the copy.
    for(auto & ls : lss_to_copy){
        DICOM_data.lsamp_data.emplace_back( std::make_shared<Line_Sample>( *ls ) );
    }

    return true;
}
