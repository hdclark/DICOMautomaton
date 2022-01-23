//ValidateTPlan.cc - A part of DICOMautomaton 2022. Written by hal clark.

#include <asio.hpp>
#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <functional>
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
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../String_Parsing.h"
#include "../Metadata.h"
#include "../Tables.h"

#include "ValidateTPlan.h"



struct common_context_t {
    std::reference_wrapper< const parsed_function > pf;
    std::reference_wrapper< TPlan_Config > plan;
    std::reference_wrapper< tables::table2 > table;

    std::reference_wrapper< Drover > DICOM_data;
    std::reference_wrapper< const OperationArgPkg > OptArgs;
    std::reference_wrapper< std::map<std::string, std::string> > InvocationMetadata;
    std::reference_wrapper< const std::string > FilenameLex;

    int64_t depth;
    int64_t report_row;
};



bool dispatch_checks( common_context_t& c );


// I'm toying with the idea of making classes into an object and factory function.
// Upsides:
//     - Easier to inspect (documentation).
//     - Easier to implement check dispatch.
//     - Better separation of tabular output from checks.
//     - Easier to split up implementation of many checks (**** important upside for managing complexity *****)
//     - Better re-use of checks. (Maybe? Not sure if would ever be needed separately??)
//
// Downsides:
//     - Have to write out the function parameters a million times. (Use a macro???)
//     - More complicated to handle tabular outputs. (Make an 'insert shrinkwrapped table B into table A at x,y cell' routine?)

struct check_t {
    std::string name;
    std::string desc;
    std::string name_regex;

    using check_impl_t = std::function<bool(common_context_t& c)>;
    check_impl_t check_impl;
};

struct table_placement_t {
    int64_t empty_row;
    int64_t pass_fail_col;
    int64_t title_col;
    int64_t expl_col;
};

table_placement_t get_table_placement(common_context_t& c);

table_placement_t get_table_placement(common_context_t& c){
    table_placement_t out;
    out.empty_row     = c.table.get().next_empty_row();
    out.pass_fail_col = c.depth + 0;
    out.title_col     = c.depth + 1;
    out.expl_col      = c.depth + 2;
    return out;
}

std::vector<check_t> get_checks();

std::vector<check_t> get_checks(){
    std::vector<check_t> out;

    // Logical statements.
    out.emplace_back();
    out.back().name = "pass";
    out.back().desc = "This check always passes.";
    out.back().name_regex = "^pass$|^true$";
    out.back().check_impl = [](common_context_t&) -> bool {
        return true;
    };

    out.emplace_back();
    out.back().name = "fail";
    out.back().desc = "This check never passes.";
    out.back().name_regex = "^fail$|^false$";
    out.back().check_impl = [](common_context_t&) -> bool {
        return false;
    };

    // Logical checks.
    out.emplace_back();
    out.back().name = "all of";
    out.back().desc = "All children checks must pass for this check to pass.";
    out.back().name_regex = "^requ?i?r?e?s?$|^all[-_ ]?of$";
    out.back().check_impl = [](common_context_t& c) -> bool {
        const auto check_child = [&c](const parsed_function& pf) -> bool {
            common_context_t l_c(c);
            l_c.pf = std::cref(pf);
            l_c.depth++;
            return dispatch_checks(l_c);
        };
        const bool passed = std::all_of( std::begin(c.pf.get().children), std::end(c.pf.get().children), check_child );
        return passed;
    };

    out.emplace_back();
    out.back().name = "one or more of";
    out.back().desc = "At least one of the children checks must pass for this check to pass.";
    out.back().name_regex = "^any[-_ ]?of$";
    out.back().check_impl = [](common_context_t& c) -> bool {
        const auto check_child = [&c](const parsed_function& pf) -> bool {
            common_context_t l_c(c);
            l_c.pf = std::cref(pf);
            l_c.depth++;
            return dispatch_checks(l_c);
        };
        const bool passed = std::any_of( std::begin(c.pf.get().children), std::end(c.pf.get().children), check_child );
        return passed;
    };

    out.emplace_back();
    out.back().name = "none of";
    out.back().desc = "All children checks must fail for this check to pass.";
    out.back().name_regex = "^none[-_ ]?of$";
    out.back().check_impl = [](common_context_t& c) -> bool {
        const auto check_child = [&c](const parsed_function& pf) -> bool {
            common_context_t l_c(c);
            l_c.pf = std::cref(pf);
            l_c.depth++;
            return dispatch_checks(l_c);
        };
        const bool passed = std::none_of( std::begin(c.pf.get().children), std::end(c.pf.get().children), check_child );
        return passed;
    };

    // Specific checks.
    out.emplace_back();
    out.back().name = "plan name has no spaces";
    out.back().desc = "Ensure the plan name does not contain any spaces.";
    out.back().name_regex = "^plan[-_ ]?name[-_ ]has[-_ ]no[-_ ]spaces$";
    out.back().check_impl = [](common_context_t& c) -> bool {
        const auto tp = get_table_placement(c);

        const auto RTPlanLabelOpt = c.plan.get().GetMetadataValueAs<std::string>("RTPlanLabel"); 
        const auto RTPlanLabel = RTPlanLabelOpt.value_or("unknown"); 
        c.table.get().inject(c.report_row, tp.expl_col, RTPlanLabel);
        const bool has_space = (RTPlanLabel.find(" ") != std::string::npos);
        const bool passed = !has_space;
        return passed;
    };

    // ...

    return out;
}

bool dispatch_checks( common_context_t& c ){
    const auto tp = get_table_placement(c);
    c.report_row = tp.empty_row;
    bool check_found = false;
    bool passed = false;

    for(const auto& check : get_checks()){
        if(std::regex_match(c.pf.get().name, Compile_Regex(check.name_regex))){
            check_found = true;
            c.table.get().inject(c.report_row, tp.title_col, check.name);
            passed = check.check_impl(c);
            break;
        }
    }
    if(!check_found){
        throw std::invalid_argument("Unable to find check matching '"_s + c.pf.get().name + "'");
    }

    c.table.get().inject(c.report_row, tp.pass_fail_col, (passed ? "pass" : "fail"));
    return passed;
}


OperationDoc OpArgDocValidateTPlan(){
    OperationDoc out;
    out.name = "ValidateTPlan";

    out.desc = 
        "This operation evaluates a radiotherapy treatment plan against user-specified criteria.";

    out.args.emplace_back();
    out.args.back() = TPWhitelistOpArgDoc();
    out.args.back().name = "TPlanSelection";
    out.args.back().default_val = "last";


    std::string checks_list;
    for(const auto& c : get_checks()) checks_list += "\n'" + c.name + "' -- " + c.desc + "\n";
    out.args.emplace_back();
    out.args.back().name = "Checks";
    out.args.back().desc = "The specific checks to perform when evaluating the plan."
                           " This parameter will often contain a script with multiple checks."
                           " List of supported checks:"
                           "\n" + checks_list + "\n";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "require(){ check_A(); check_B(); ... }",
                                 "all_of(){ check_A(); check_B(); ... }",
                                 "any_of(){ check_A(); check_B(); ... }" };

    out.args.emplace_back();
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";

    return out;
}

bool ValidateTPlan(Drover &DICOM_data,
                    const OperationArgPkg& OptArgs,
                    std::map<std::string, std::string>& InvocationMetadata,
                    const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TPlanSelectionStr = OptArgs.getValueStr("TPlanSelection").value();

    const auto TableSelectionStr = OptArgs.getValueStr("TableSelection").value();
    const auto ChecksStr = OptArgs.getValueStr("Checks").value();

    //-----------------------------------------------------------------------------------------------------------------

    const auto pfs = parse_functions(ChecksStr);
    if(pfs.empty()){
        throw std::invalid_argument("No checks specified, nothing to check");
    }

    auto TPs_all = All_TPs( DICOM_data );
    auto TPs = Whitelist( TPs_all, TPlanSelectionStr );
    if(TPs.empty()){
        throw std::invalid_argument("No plans specified, nothing to check");
    }

    // Locate or create a table for the results.
    auto STs_all = All_STs( DICOM_data );
    auto STs = Whitelist( STs_all, TableSelectionStr );
    if(STs.empty()){
        DICOM_data.table_data.emplace_back( std::make_shared<Sparse_Table>() );
        auto* tab_ptr = &( DICOM_data.table_data.back()->table );
        auto l_meta = coalesce_metadata_for_basic_table(tab_ptr->metadata);
        l_meta.merge(tab_ptr->metadata);
        tab_ptr->metadata = l_meta;

        STs_all = All_STs( DICOM_data );
        STs = Whitelist( STs_all, TableSelectionStr );
    }
    if(STs.size() != 1u){
        throw std::invalid_argument("Multiple tables selected");
    }

    for(auto & tp_it : TPs){
        // Process each treatment plan separately.
        for(const auto& pf : pfs){
            common_context_t c {
                std::cref(pf),
                std::ref(*(*tp_it)),
                std::ref((*(STs.front()))->table),
                std::ref(DICOM_data),
                std::cref(OptArgs),
                std::ref(InvocationMetadata),
                std::cref(FilenameLex),
                0,
                0 };

            dispatch_checks(c);
        }
    }

    return true;
}

