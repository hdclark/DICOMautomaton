// DeleteWarps.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <optional>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <string>    

#include "YgorMath.h"         //Needed for vec3 class.

#include "../Structs.h"
#include "../Regex_Selectors.h"

#include "DeleteWarps.h"


OperationDoc OpArgDocDeleteWarps(){
    OperationDoc out;
    out.name = "DeleteWarps";
    out.aliases.emplace_back("DeleteTransform");

    out.tags.emplace_back("category: spatial transform processing");

    out.desc = 
        "This routine deletes spatial transformations (i.e., warps) from memory."
        " It is most useful when working with positional operations in stages.";

    out.args.emplace_back();
    out.args.back() = T3WhitelistOpArgDoc();
    out.args.back().name = "TransformSelection";
    out.args.back().default_val = "last";

    return out;
}



bool DeleteWarps(Drover &DICOM_data,
                 const OperationArgPkg& OptArgs,
                 std::map<std::string, std::string>& /*InvocationMetadata*/,
                 const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TransformSelectionStr = OptArgs.getValueStr("TransformSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    auto T3s_all = All_T3s( DICOM_data );
    auto T3s = Whitelist( T3s_all, TransformSelectionStr );
    for(auto & t3p_it : T3s){
        DICOM_data.trans_data.erase( t3p_it );
    }

    return true;
}
