// ValidateTPlan.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"
#include "../String_Parsing.h"
#include "../Metadata.h"
#include "../Tables.h"

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

bool dispatch_checks( common_context_t& c );

table_placement_t get_table_placement(common_context_t& c);

std::vector<check_t> get_checks();


OperationDoc OpArgDocValidateTPlan();

bool ValidateTPlan(Drover &DICOM_data,
                   const OperationArgPkg& /*OptArgs*/,
                   std::map<std::string, std::string>& /*InvocationMetadata*/,
                   const std::string& /*FilenameLex*/);
