//WidenTable.cc - A part of DICOMautomaton 2024. Written by hal clark.

#include <deque>
#include <optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "YgorMath.h"
#include "YgorString.h"
#include "YgorLog.h"

#include "../Tables.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"


#include "WidenTable.h"

OperationDoc OpArgDocWidenTable(){
    OperationDoc out;
    out.name = "WidenTable";
    out.tags.emplace_back("category: table processing");

    out.desc = 
        "This operation reshapes tables, changing from 'long' to 'wide' by computing a self-intersection.";

    out.args.emplace_back();
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";
    

    out.args.emplace_back();
    out.args.back().name = "KeyColumns";
    out.args.back().desc = "A list of the columns to collectively use as a key. All rows with matching cell contents"
                           " in these columns forms a group, and the group is reshaped such that the first row is"
                           " unaltered except subsequent rows are appended to the right."
                           " After this operation, the distinct combinations of keys appearing in the specified"
                           " columns will appear on only one row."
                           "\n\n"
                           "Multiple columns can be specified as a comma-separated list. Specifiers are intepretted"
                           " as either column numbers (note: zero-based), or regular expressions. Regular expressions"
                           " will be applied to the entire table, and the column number of any cell whose value matches"
                           " will be added to the list."
                           "\n\n"
                           "Note that the relative order of rows is preserved, except instead of subsequent rows"
                           " appearing *below* earlier rows, they will now appear to the *right*.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "0,1", "0,1,5,6", "5,6,7", "ROILabel,StudyDate,.*Date.*" };


    out.args.emplace_back();
    out.args.back().name = "IgnoreRows";
    out.args.back().desc = "A list of rows to ignore. For example: rows containing headers, or cells that should not be"
                           " appended."
                           "\n\n"
                           "Multiple rows can be specified as a comma-separated list. Specifiers are intepretted"
                           " as either row numbers (note: zero-based), or regular expressions. Regular expressions"
                           " will be applied to the entire table, and the row number of any cell whose value matches"
                           " will be added to the list."
                           "\n\n"
                           "Note that while ignored rows will not participate in the reshaping process, their row"
                           " numbers may be altered due to reshaped rows being removed."
                           " The relative order will not be altered.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "0,1", "0,1,5,6", "5,6,7", "ROILabel,StudyDate,.*Date.*" };

    return out;
}

bool WidenTable(Drover &DICOM_data,
                const OperationArgPkg& OptArgs,
                std::map<std::string, std::string>& /*InvocationMetadata*/,
                const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TableSelectionStr = OptArgs.getValueStr("TableSelection").value();
    const auto KeyColumnsStr = OptArgs.getValueStr("KeyColumns").value();
    const auto IgnoreRowsStr = OptArgs.getValueStr("IgnoreRows").value();

    //-----------------------------------------------------------------------------------------------------------------
    auto STs_all = All_STs( DICOM_data );
    auto STs = Whitelist( STs_all, TableSelectionStr );
    for(auto & stp_it : STs){
        tables::table2& t = (*stp_it)->table;
        const auto mmr = t.min_max_row();
        const auto mmc = t.min_max_col();


        // Split specifiers into lists.
        const auto KeyColumnsStrs = SplitStringToVector(KeyColumnsStr, ',', 'd');
        const auto IgnoreRowsStrs = SplitStringToVector(IgnoreRowsStr, ',', 'd');

        // Check if a number was provided, otherwise it is a regex.
        std::set<int64_t> key_columns;
        std::list<std::regex> key_columns_regexes;
        for(const auto &s : KeyColumnsStrs){
            
            const bool is_numeric = Is_String_An_X<int64_t>(s);
            if(is_numeric){
                key_columns.insert( stringtoX<int64_t>(s) );
            }else{
                key_columns_regexes.emplace_back(Compile_Regex(s));
            }
        }

        std::set<int64_t> ignore_rows;
        std::list<std::regex> ignore_rows_regexes;
        for(const auto &s : IgnoreRowsStrs){
            
            const bool is_numeric = Is_String_An_X<int64_t>(s);
            if(is_numeric){
                ignore_rows.insert( stringtoX<int64_t>(s) );
            }else{
                ignore_rows_regexes.emplace_back(Compile_Regex(s));
            }
        }

        // Find the cells matching the given regexes.
        const auto key_columns_matches = t.find_cells( key_columns_regexes, mmr, mmc );
        const auto ignore_rows_matches = t.find_cells( ignore_rows_regexes, mmr, mmc );

        // Convert the cells to specifiers, and merge with the rows/columns explicitly specified by the user.
        for(const auto &c : t.get_specifiers(key_columns_matches).second) key_columns.insert(c);
        for(const auto &r : t.get_specifiers(ignore_rows_matches).first) ignore_rows.insert(r);


        const auto print_specifiers = [](const tables::specifiers_t &s){
            std::stringstream ss;
            bool first = true;
            for(const auto &x : s){
                ss << (first ? "" : ", ") << x;
                first = false;
            }
            return ss.str();
        };
        YLOGINFO("Proceeding with KeyColumns = " << print_specifiers(key_columns)
              << " and IgnoreRows = " << print_specifiers(ignore_rows));

        t.reshape_widen( key_columns, ignore_rows, mmr, mmc );
    }

    return true;
}
