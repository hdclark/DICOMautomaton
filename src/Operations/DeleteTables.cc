//DeleteTables.cc - A part of DICOMautomaton 2021. Written by hal clark.

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
#include "DeleteTables.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocDeleteTables(){
    OperationDoc out;
    out.name = "DeleteTables";

    out.tags.emplace_back("category: table processing");

    out.desc = 
        "This routine deletes tables.";

    out.args.emplace_back();
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";

    return out;
}

bool DeleteTables(Drover &DICOM_data,
                  const OperationArgPkg& OptArgs,
                  std::map<std::string, std::string>& /*InvocationMetadata*/,
                  const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TableSelectionStr = OptArgs.getValueStr("TableSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    auto STs_all = All_STs( DICOM_data );
    auto STs = Whitelist( STs_all, TableSelectionStr );
    for(auto & stp_it : STs){
        DICOM_data.table_data.erase( stp_it );
    }

    return true;
}
