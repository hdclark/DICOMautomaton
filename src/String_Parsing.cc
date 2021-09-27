//String_Parsing.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <algorithm>
#include <vector>
#include <string>
#include <optional>

#include "YgorString.h"
#include "YgorMath.h"

#include "String_Parsing.h"

parsed_function
parse_function(const std::string &in){
    parsed_function out;

    auto split = SplitStringToVector(in, '(', 'd');
    split = SplitVector(split, ')', 'd');
    split = SplitVector(split, ',', 'd');
    split = SplitVector(split, ' ', 'd');
    if(split.empty()) throw std::invalid_argument("Function could not be parsed");

    // Extract name.
    out.name = split.front();
    split.erase(split.begin());

    // Extract parameters.
    for(const auto &w : split){
       if(w.empty()) continue;
       out.parameters.emplace_back();

       // Try extract the number, ignoring any suffixes.
       try{
           const auto x = std::stod(w);
           out.parameters.back().number = x;
       }catch(const std::exception &){ }

       // If numeric, assess whether the (optional) suffix indicates something important.
       if((w.size()-1) == w.find('x')) out.parameters.back().is_fractional = true;
       if((w.size()-1) == w.find('%')) out.parameters.back().is_percentage = true;

       if( out.parameters.back().is_fractional
       &&  out.parameters.back().is_percentage ){
            throw std::invalid_argument("Parameter cannot be both a fraction and a percentage");
       }
    }
    return out;
}

parsed_function
retain_only_numeric_parameters(parsed_function pf){
    pf.parameters.erase(std::remove_if(pf.parameters.begin(), pf.parameters.end(),
                        [](const function_parameter &fp){
                            return !(fp.number);
                        }),
    pf.parameters.end());
    return pf;
}

