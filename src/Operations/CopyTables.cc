//CopyTables.cc - A part of DICOMautomaton 2021. Written by hal clark.

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
#include "CopyTables.h"

OperationDoc OpArgDocCopyTables(){
    OperationDoc out;
    out.name = "CopyTables";

    out.desc = 
        "This operation deep-copies the selected tables.";

    out.args.emplace_back();
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";
    
    return out;
}

bool CopyTables(Drover &DICOM_data,
                const OperationArgPkg& OptArgs,
                std::map<std::string, std::string>& /*InvocationMetadata*/,
                const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TableSelectionStr = OptArgs.getValueStr("TableSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    //Gather a list of tables to copy.
    std::deque<std::shared_ptr<Sparse_Table>> tables_to_copy;

    auto STs_all = All_STs( DICOM_data );
    auto STs = Whitelist( STs_all, TableSelectionStr );
    for(auto & stp_it : STs){
        tables_to_copy.push_back(*stp_it);
    }

    //Copy the meshes.
    for(auto & stp : tables_to_copy){
        DICOM_data.table_data.emplace_back( std::make_shared<Sparse_Table>( *stp ) );
    }

    return true;
}
