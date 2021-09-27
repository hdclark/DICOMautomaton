//String_Parsing.h.

#pragma once

#include <vector>
#include <string>
#include <optional>

#include "YgorString.h"
#include "YgorMath.h"

// Used to parse functions like 'func(1.0, 2.0,3.0, -1.23, 1.0x, 123%, ...)'.
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
};

parsed_function parse_function(const std::string &in);

parsed_function retain_only_numeric_parameters(parsed_function pf);

