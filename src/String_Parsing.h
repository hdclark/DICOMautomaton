//String_Parsing.h.

#pragma once

#include <vector>
#include <string>
#include <optional>

#include "YgorString.h"
#include "YgorMath.h"

// String <--> fixed array conversion routines.
void array_to_string(std::string &s, const std::array<char, 2048> &a);

std::string array_to_string(const std::array<char, 2048> &a);

void string_to_array(std::array<char, 2048> &a, const std::string &s);

std::array<char, 2048> string_to_array(const std::string &s);

// Remove characters so that the argument can be inserted like '...' on command line.
std::string escape_for_quotes(std::string s);


// Parser for functions like 'func(1.0, 2.0,3.0, -1.23, 1.0x, 123%, "some text", ...)'.
//
// Note: Supports nested functions.
//
// Note: Supports optional suffixes that denote how the parameter should be interpretted.
//       Currently supported: '1.23x' for fractional and '12.3%' for percentage.
//       What exactly these are relative to is domain-specific and must be handled by the caller.

struct function_parameter {
    std::string raw;
    std::optional<double> number;
    bool is_fractional = false;
    bool is_percentage = false;
};

struct parsed_function {
    std::string name;
    std::vector<function_parameter> parameters;

    std::vector<parsed_function> children;
};

std::vector<parsed_function>
parse_functions(const std::string &,
                char escape_char = '\\',
                char func_sep_char = ';',
                long int parse_depth = 0);

std::vector<parsed_function>
retain_only_numeric_parameters(std::vector<parsed_function>);

// Parser for number lists.
std::vector<double> parse_numbers(const std::string &split_chars,
                                  const std::string &in);

