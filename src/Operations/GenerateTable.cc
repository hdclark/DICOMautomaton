//GenerateTable.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Structs.h"
#include "../Tables.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../String_Parsing.h"
#include "../Metadata.h"
#include "../CSG_SDF.h"

#include "GenerateTable.h"


OperationDoc OpArgDocGenerateTable(){
    OperationDoc out;
    out.name = "GenerateTable";
    out.tags.emplace_back("category: table processing");
    out.tags.emplace_back("category: generator");

    out.desc = 
        "This operation creates an empty table.";

    out.args.emplace_back();
    out.args.back().name = "TableLabel";
    out.args.back().desc = "A label to attach to the new table.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "xyz", "sheet A" };

    return out;
}

bool GenerateTable(Drover &DICOM_data,
                   const OperationArgPkg& OptArgs,
                   std::map<std::string, std::string>& /*InvocationMetadata*/,
                   const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TableLabel = OptArgs.getValueStr("TableLabel").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto NormalizedTableLabel = X(TableLabel);

    // Inject the result into the Drover.
    auto meta = coalesce_metadata_for_basic_table({}, meta_evolve::iterate);
    DICOM_data.table_data.emplace_back( std::make_unique<Sparse_Table>() );
    DICOM_data.table_data.back()->table.metadata = meta;
    DICOM_data.table_data.back()->table.metadata["TableLabel"] = TableLabel;
    DICOM_data.table_data.back()->table.metadata["NormalizedTableLabel"] = NormalizedTableLabel;
    DICOM_data.table_data.back()->table.metadata["Description"] = "Generated table";

    //// Insert user-specified metadata.
    ////
    //// Note: This must occur last so it overwrites incumbent metadata entries.
    //for(const auto &kvp : Metadata){
    //    DICOM_data.table_data.back()->metadata[kvp.first] = kvp.second;
    //}
    //
    // ... TODO ...

    return true;
}
