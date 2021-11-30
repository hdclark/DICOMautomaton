//String_Parsing.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <algorithm>
#include <vector>
#include <string>
#include <optional>

#include "YgorString.h"
#include "YgorMath.h"
#include "YgorMisc.h"

#include "String_Parsing.h"

std::vector<parsed_function>
parse_functions(const std::string &in, 
                char escape_char,
                char func_sep_char ){
    std::vector<parsed_function> out;

    // Parse function statements respecting quotation and escapes, converting
    //
    //   `f1(x, "arg, text\, or \"escaped\" sequence", 1.23); f2("tex\t", 2.\34)`
    //
    // into the following parsed function name and parameter tokens:
    //
    //  Function:
    //    Function name: 'f1'
    //    Parameter 1:   'x'
    //    Parameter 2:   'arg, text, or "escaped" sequence'
    //    Parameter 3:   '1.23'
    //  Function:
    //    Function name: 'f2'
    //    Parameter 1:   'text'  <--- n.b. the 't' is normal.
    //    Parameter 2:   '2.\34' <--- n.b. escaping only works inside a quotation
    //
    // Note that nested functions are NOT supported.

    {
        const auto clean_string = [](const std::string &in){
            return Canonicalize_String2(in, CANONICALIZE::TRIM_ENDS);
        };
        
        parsed_function pshtl;
        std::string shtl;
        char close_quote = '\0'; // e.g., '"' or '\''.
        char close_paren = '\0'; // e.g., ')' or ']'.
        //const char escape_char = '\\';
        //const char func_sep_char = ';';
        const auto end = std::end(in);
        for(auto c_it = std::begin(in); c_it != end; ++c_it){

            // Behaviour inside a quote.
            if( close_quote != '\0' ){

                // Handle escapes by immediately advancing to the next char.
                if( *c_it == escape_char ){
                    ++c_it;
                    if(c_it == end){
                        throw std::invalid_argument("Escape character present, but nothing to escape");
                    }
                    shtl.push_back(*c_it);
                    continue;

                // Close the quote, discarding the quote symbol.
                }else if( *c_it == close_quote ){
                    close_quote = '\0';
                    continue;

                }else{
                    shtl.push_back(*c_it);
                    continue;
                }

            // Behaviour inside a parenthesis.
            }else if(close_paren != '\0'){

                // Open a quote and discard the quoting character.
                if( (*c_it == '\'') || (*c_it == '"') ){
                    close_quote = *c_it;
                    continue;

                // Close a parenthesis, which should also complete the function.
                }else if( *c_it == close_paren ){
                    // Complete the final argument.
                    //
                    // Note that to support disregarding a trailing comma, we disallow empty arguments.
                    shtl = clean_string(shtl);
                    if(!shtl.empty()){
                        pshtl.parameters.emplace_back();
                        pshtl.parameters.back().raw = shtl;
                    }
                    shtl.clear();

                    // Reset the shtl function.
                    out.emplace_back(pshtl);
                    pshtl = parsed_function();

                    close_paren = '\0';
                    continue;

                // Handle arg -> arg transition, discarding the comma character.
                }else if(*c_it == ','){
                    // Note that to support disregarding a trailing comma, we disallow empty arguments.
                    shtl = clean_string(shtl);
                    if(!shtl.empty()){
                        pshtl.parameters.emplace_back();
                        pshtl.parameters.back().raw = shtl;
                    }
                    shtl.clear();
                    continue;

                }else{
                    shtl.push_back(*c_it);
                    continue;
                }

            // Behaviour outside of a quote or parenthesis.
            }else{

                // Handle function name -> args transition.
                // Open a parentheses and discard the character.
                if( (*c_it == '(') || (*c_it == '{') || (*c_it == '[') ){
                    if(*c_it == '('){
                        close_paren = ')';
                    }else if(*c_it == '{'){
                        close_paren = '}';
                    }else if(*c_it == '['){
                        close_paren = ']';
                    }

                    // Assign function name.
                    shtl = clean_string(shtl);
                    if(shtl.empty()){
                        throw std::invalid_argument("Function names cannot be empty");
                    }
                    if(!pshtl.name.empty()){
                        throw std::invalid_argument("Refusing to overwrite existing function name");
                    }
                    pshtl.name = shtl;
                    shtl.clear();
                    continue;


                // Handle function -> function transitions.
                }else if(*c_it == func_sep_char){
                    shtl = clean_string(shtl);
                    if(!shtl.empty()){
                        throw std::invalid_argument("Disregarding characters between functions");
                    }
                    shtl.clear();

                }else{
                    shtl.push_back(*c_it);
                    continue;
                }
            }

        }

        if( !pshtl.name.empty()
        ||  !pshtl.parameters.empty() ){
            throw std::invalid_argument("Incomplete function statement: terminate function by opening/closing scope");
        }
    }

    //// Print the AST for debugging.
    //for(auto &pf : out){
    //   FUNCINFO("pf.name = '" << pf.name << "'");
    //   for(auto &p : pf.parameters){
    //       FUNCINFO("    parameter: '" << p.raw << "'");
    //   }
    //}

    if(out.empty()) throw std::invalid_argument("Unable to parse function from input");

    // Post-process parameters.
    for(auto &pf : out){
       for(auto &p : pf.parameters){
           if(p.raw.empty()) continue;

           // Try extract the number, ignoring any suffixes.
           try{
               const auto x = std::stod(p.raw);
               p.number = x;
           }catch(const std::exception &){ }

           // If numeric, assess whether the (optional) suffix indicates something important.
           if((p.raw.size()-1) == p.raw.find('x')) p.is_fractional = true;
           if((p.raw.size()-1) == p.raw.find('%')) p.is_percentage = true;

           if( p.is_fractional
           &&  p.is_percentage ){
                throw std::invalid_argument("Parameter cannot be both a fraction and a percentage");
           }
       }
    }
    
    return out;
}

std::vector<parsed_function>
retain_only_numeric_parameters(std::vector<parsed_function> pfs){
    for(auto &pf : pfs){
        pf.parameters.erase(std::remove_if(pf.parameters.begin(), pf.parameters.end(),
                            [](const function_parameter &fp){
                                return !(fp.number);
                            }),
        pf.parameters.end());
    }
    return pfs;
}

