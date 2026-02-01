// CopyRTPlans.cc - A part of DICOMautomaton 2026. Written by hal clark.

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

#include "CopyRTPlans.h"

OperationDoc OpArgDocCopyRTPlans(){
    OperationDoc out;
    out.name = "CopyRTPlans";

    out.tags.emplace_back("category: rtplan processing");

    out.desc = 
        "This operation deep-copies the selected treatment plans.";

    out.args.emplace_back();
    out.args.back() = TPWhitelistOpArgDoc();
    out.args.back().name = "RTPlanSelection";
    out.args.back().default_val = "last";
    
    return out;
}

bool CopyRTPlans(Drover &DICOM_data,
                 const OperationArgPkg& OptArgs,
                 std::map<std::string, std::string>& /*InvocationMetadata*/,
                 const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto RTPlanSelectionStr = OptArgs.getValueStr("RTPlanSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    //Gather a list of rtplans to copy.
    std::deque<std::shared_ptr<RTPlan>> rtplans_to_copy;

    auto TPs_all = All_TPs( DICOM_data );
    auto TPs = Whitelist( TPs_all, RTPlanSelectionStr );
    for(auto & tp_it : TPs){
        rtplans_to_copy.push_back(*tp_it);
    }

    //Copy the rtplans.
    for(auto & tpp : rtplans_to_copy){
        DICOM_data.rtplan_data.emplace_back( std::make_shared<RTPlan>( *tpp ) );
    }

    return true;
}
