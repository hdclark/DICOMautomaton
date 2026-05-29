// DeleteRTPlans.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <optional>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"

#include "DeleteRTPlans.h"



OperationDoc OpArgDocDeleteRTPlans(){
    OperationDoc out;
    out.name = "DeleteRTPlans";

    out.tags.emplace_back("category: rtplan processing");

    out.desc = 
        "This routine deletes treatment plans from memory.";

    out.args.emplace_back();
    out.args.back() = TPWhitelistOpArgDoc();
    out.args.back().name = "RTPlanSelection";
    out.args.back().default_val = "last";

    return out;
}



bool DeleteRTPlans(Drover &DICOM_data,
                   const OperationArgPkg& OptArgs,
                   std::map<std::string, std::string>& /*InvocationMetadata*/,
                   const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto RTPlanSelectionStr = OptArgs.getValueStr("RTPlanSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    auto TPs_all = All_TPs( DICOM_data );
    auto TPs = Whitelist( TPs_all, RTPlanSelectionStr );
    for(auto & tp_it : TPs){
        DICOM_data.rtplan_data.erase( tp_it );
    }

    return true;
}
