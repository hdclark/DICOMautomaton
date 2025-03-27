//ConvertParametersToTable.cc - A part of DICOMautomaton 2023. Written by hal clark.

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

#include <filesystem>

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Metadata.h"
#include "../Thread_Pool.h"
#include "../Write_File.h"
#include "../String_Parsing.h"

#include "ConvertParametersToTable.h"


OperationDoc OpArgDocConvertParametersToTable(){
    OperationDoc out;
    out.name = "ConvertParametersToTable";
    out.tags.emplace_back("category: parameter table");
    out.tags.emplace_back("category: table processing");

    out.desc = 
        "Convert one or more key-value parameters from the global parameter table into a table."
        " If no table is selected, a new table will be created."
        " If an existing table is selected, row(s) will be appended to the bottom.";

    out.args.emplace_back();
    out.args.back().name = "KeySelection";
    out.args.back().desc = "A regular expression that will select key-values to include in the table."
                           "\n\n"
                           "This regular expression will be applied only to keys."
                           " Note that multiple keys can be selected; whether they are emitted on one"
                           " or multiple rows is controlled by the 'Shape' parameter."
                           "";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", "a_specific_key", "^a_specific_prefix.*" };


    out.args.emplace_back();
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back().name = "TableLabel";
    out.args.back().desc = "A label to attach to table if and only if a new table is created.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "xyz", "sheet A" };


    out.args.emplace_back();
    out.args.back().name = "EmitHeader";
    out.args.back().desc = "Controls whether a header (consisting of the key names) is emitted."
                           " If 'false' no header is emitted."
                           " If 'true', two rows are emitted regardless of whether there is a pre-existing header."
                           " If 'empty', a header is only emitted when the table is empty (i.e., no content in any"
                           " cells). Consistency of the emitted row with the existing table content and"
                           " structure is not verified and is therefore left to the user."
                           "";
    out.args.back().default_val = "empty";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false", "empty" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back().name = "Shape";
    out.args.back().desc = "Controls whether the table is written in 'wide' or 'tall' format."
                           "\n\n"
                           "The 'wide' shape places all output from a single invocation onto a single row"
                           " (or two if a header is also emitted)."
                           " This format is most useful for analysis since the relationship between metadata"
                           " elements can be analyzed more easily (e.g., regression or classification)."
                           "\n\n"
                           "The 'tall' shape places every metadata key-value pair on a separate row."
                           " This format is most useful for simplistic data extraction or simple analysis where"
                           " the relationship between metadata elements is not important (e.g., grepping for a"
                           " specific key-value, checking if one-of-any tags are present)."
                           "";
    out.args.back().default_val = "wide";
    out.args.back().expected = true;
    out.args.back().examples = { "wide", "tall" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}

bool ConvertParametersToTable(Drover& DICOM_data,
                              const OperationArgPkg& OptArgs,
                              std::map<std::string, std::string>& InvocationMetadata,
                              const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto KeySelectionStr = OptArgs.getValueStr("KeySelection").value();

    const auto TableLabel = OptArgs.getValueStr("TableLabel").value();
    const auto TableSelectionStr = OptArgs.getValueStr("TableSelection").value();
    const auto EmitHeaderStr = OptArgs.getValueStr("EmitHeader").value();
    const auto ShapeStr = OptArgs.getValueStr("Shape").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto NormalizedTableLabel = X(TableLabel);

    const auto regex_key = Compile_Regex(KeySelectionStr);

    const auto regex_true = Compile_Regex("^tr?u?e?$");
    const auto regex_false = Compile_Regex("^fa?l?s?e?$");
    const auto regex_empty = Compile_Regex("^em?p?t?y?$");

    const auto regex_wide = Compile_Regex("^wi?d?e?$");
    const auto regex_tall = Compile_Regex("^ta?l?l?$");
    
    const bool emit_header_true  = std::regex_match(EmitHeaderStr, regex_true);
    const bool emit_header_false = std::regex_match(EmitHeaderStr, regex_false);
    const bool emit_header_empty = std::regex_match(EmitHeaderStr, regex_empty);

    const bool shape_wide = std::regex_match(ShapeStr, regex_wide);
    const bool shape_tall = std::regex_match(ShapeStr, regex_tall);

    // Select or create a table.
    auto STs_all = All_STs( DICOM_data );
    auto STs = Whitelist( STs_all, TableSelectionStr );

    const bool create_new_table = STs.empty() || (*(STs.back()) == nullptr);
    std::shared_ptr<Sparse_Table> st = create_new_table ? std::make_shared<Sparse_Table>()
                                                        : *(STs.back());


    // Search all parameters.
    int64_t row_a = st->table.next_empty_row();
    int64_t row_b = row_a + 1;
    tables::table2 placeholder_table;
    const bool is_empty = create_new_table || (row_a == placeholder_table.next_empty_row());

    int64_t col = 0;
    int64_t row = row_a;
    for(const auto& kvp : InvocationMetadata){
        if(std::regex_match(kvp.first, regex_key)){

            if(false){
            }else if(shape_wide){
                std::optional<int64_t> row_hdr;
                std::optional<int64_t> row_val;
                if(false){
                }else if( emit_header_true
                      ||  (emit_header_empty && is_empty) ){
                    row_hdr = row_a;
                    row_val = row_b;
                }else if( emit_header_false
                      ||  (emit_header_empty && !is_empty) ){
                    row_hdr = {};
                    row_val = row_a;
                }else{
                    throw std::runtime_error("EmitHeader argument not understood");
                }

                if(row_hdr){
                    st->table.inject(row_hdr.value(), col, kvp.first);
                }
                if(row_val){
                    st->table.inject(row_val.value(), col, kvp.second);
                }
                ++col;

            }else if(shape_tall){
                std::optional<int64_t> col_hdr;
                std::optional<int64_t> col_val;

                if(false){
                }else if( emit_header_true
                      ||  (emit_header_empty && is_empty) ){
                    col_hdr = 0;
                    col_val = 1;
                }else if( emit_header_false
                      ||  (emit_header_empty && !is_empty) ){
                    col_hdr = {};
                    col_val = 0;
                }else{
                    throw std::runtime_error("EmitHeader argument not understood");
                }

                if(col_hdr){
                    st->table.inject(row, col_hdr.value(), kvp.first);
                }
                if(col_val){
                    st->table.inject(row, col_val.value(), kvp.second);
                }
                ++row;

            }else{
                throw std::runtime_error("Shape argument not understood");
            }
        }
    }

    // Inject the result into the Drover if not already.
    if(create_new_table){
        auto meta = coalesce_metadata_for_basic_table({}, meta_evolve::iterate);
        st->table.metadata = meta;
        st->table.metadata["TableLabel"] = TableLabel;
        st->table.metadata["NormalizedTableLabel"] = NormalizedTableLabel;
        st->table.metadata["Description"] = "Generated table";

        DICOM_data.table_data.emplace_back( st );
    }

    return true;
}

