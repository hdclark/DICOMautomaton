//ExportTables.cc - A part of DICOMautomaton 2021. Written by hal clark.

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
#include "../Write_File.h"

#include "ExportTables.h"

OperationDoc OpArgDocExportTables(){
    OperationDoc out;
    out.name = "ExportTables";

    out.tags.emplace_back("category: table processing");
    out.tags.emplace_back("category: file export");

    out.desc = 
        "This operation exports the selected table(s) into a single CSV formatted file.";

    out.args.emplace_back();
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back().name = "Filename";
    out.args.back().desc = "The exported file's name."
                           " The format is CSV. Leave empty to generate a unique temporary file."
                           " If an existing file is present, the contents will be appended."
                           " If multiple tables are selected, they will all be appended to the same"
                           " file.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile", "localfile.csv", "derivative_data.csv" };
    out.args.back().mimetype = "text/csv";


    
    return out;
}

bool ExportTables(Drover &DICOM_data,
                const OperationArgPkg& OptArgs,
                std::map<std::string, std::string>& /*InvocationMetadata*/,
                const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TableSelectionStr = OptArgs.getValueStr("TableSelection").value();

    auto Filename = OptArgs.getValueStr("Filename").value();

    //-----------------------------------------------------------------------------------------------------------------
    auto file_lock = Make_File_Lock("dcma_op_exporttables");

    if(Filename.empty()){
        Filename = Generate_Unique_tmp_Filename("dcma_exporttables_", ".csv").string();
    }
    std::fstream of(Filename, std::ios::out | std::ios::app | std::ios::binary);

    auto STs_all = All_STs( DICOM_data );
    auto STs = Whitelist( STs_all, TableSelectionStr );
    for(auto & stp_it : STs){
        (*stp_it)->table.write_csv(of);
    }
    of.flush();

    return true;
}
