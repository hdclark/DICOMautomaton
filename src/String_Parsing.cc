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
                char func_sep_char,
                long int parse_depth){

    std::vector<parsed_function> out;
    const bool debug = false;

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
    // Note that nested functions *are* supported. They use syntax like:
    //
    //    `parent(x, y, z){ child1(a, b, c); child2(d, e, f) }`
    //
    // Also note that quotations can be used to avoid nested function parse issues,
    // which will convert
    //
    //    `f1(x, "f2(a,b,c)")`
    //
    // into:
    //
    //  Function:
    //    Function name: 'f1'
    //    Parameter 1:   'x'
    //    Parameter 2:   'f2(a,b,c)'
    //

    {
        const auto clean_string = [](const std::string &in){
            return Canonicalize_String2(in, CANONICALIZE::TRIM_ENDS);
        };
        const auto clean_function_name = [](const std::string &in){
            std::string out = in;
            out.erase( std::remove_if( std::begin(out), std::end(out),
                                       [](unsigned char c){
                                           return !(std::isalnum(c) || (c == '_')); 
                                       }),
                       std::end(out));
            return out;
        };
        
        parsed_function pshtl;
        std::string shtl;
        std::vector<char> close_quote; // e.g., '"' or '\''.
        std::vector<char> close_paren; // e.g., ')' or ']'.
        
        const auto end = std::end(in);
        for(auto c_it = std::begin(in); c_it != end; ++c_it){

            // Behaviour inside a curly parenthesis (i.e., nested children functions).
            //
            // Note that nested functions are parsed recursively.
            if( !close_paren.empty()
            &&  (close_paren.front() == '}') ){

                // Behaviour inside a quote.
                if( !close_quote.empty() ){

                    // Handle escapes by immediately advancing to the next char.
                    if( *c_it == escape_char ){
                        shtl.push_back(*c_it);
                        ++c_it;
                        if(c_it == end){
                            throw std::invalid_argument("Escape character present, but nothing to escape");
                        }
                        shtl.push_back(*c_it);
                        continue;

                    // Close the quote, discarding the quote symbol.
                    }else if( *c_it == close_quote.back() ){
                        shtl.push_back(*c_it);
                        close_quote.pop_back();
                        continue;

                    }else{
                        shtl.push_back(*c_it);
                        continue;
                    }

                }else{

                    // Open a quote.
                    if( (*c_it == '\'') || (*c_it == '"') ){
                        close_quote.push_back(*c_it);
                        shtl.push_back(*c_it);
                        continue;

                    // Increase the nesting depth.
                    }else if( *c_it == '{' ){
                        close_paren.push_back('}');
                        shtl.push_back(*c_it);
                        continue;

                    // Close a parenthesis.
                    }else if( *c_it == close_paren.back() ){
                        close_paren.pop_back();

                        // Parse the children.
                        if(close_paren.empty()){
                            if(out.empty()){
                                throw std::invalid_argument("No parent function available to append child to");
                            }
                            if(!out.back().children.empty()){
                                throw std::invalid_argument("Function already contains one or more nested functions");
                            }
                            shtl = clean_string(shtl);
                            if(!shtl.empty()){
                                out.back().children = parse_functions(shtl, escape_char, func_sep_char, parse_depth + 1);
                            }
                            shtl.clear();

                        // Note: we only drop the top-level parenthesis. Pass all others through for recursive parsing.
                        }else{
                            shtl.push_back(*c_it);
                        }
                        continue;

                    }else{
                        shtl.push_back(*c_it);
                        continue;
                    }
                }

            // Behaviour inside a quote.
            }else if( !close_quote.empty() ){

                // Handle escapes by immediately advancing to the next char.
                if( *c_it == escape_char ){
                    ++c_it;
                    if(c_it == end){
                        throw std::invalid_argument("Escape character present, but nothing to escape");
                    }
                    shtl.push_back(*c_it);
                    continue;

                // Close the quote, discarding the quote symbol.
                }else if( *c_it == close_quote.back() ){
                    close_quote.pop_back();
                    continue;

                }else{
                    shtl.push_back(*c_it);
                    continue;
                }

            // Behaviour inside a parenthesis (i.e., the 'parameters' part of a function).
            }else if( !close_paren.empty()
                  &&  ( (close_paren.back() == ')') || (close_paren.back() == ']') ) ){

                // Open a quote and discard the quoting character.
                if( (*c_it == '\'') || (*c_it == '"') ){
                    close_quote.push_back(*c_it);
                    continue;

                // Close a parenthesis, which should also complete the function.
                }else if( *c_it == close_paren.back() ){
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

                    close_paren.pop_back();
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
                if( (*c_it == '(') || (*c_it == '[') ){
                    if(*c_it == '('){
                        close_paren.push_back(')');
                    }else if(*c_it == '['){
                        close_paren.push_back(']');
                    }

                    // Assign function name.
                    shtl = clean_string(shtl);
                    shtl = clean_function_name(shtl);
                    if(shtl.empty()){
                        throw std::invalid_argument("Function names cannot be empty");
                    }
                    if(!pshtl.name.empty()){
                        throw std::invalid_argument("Refusing to overwrite existing function name");
                    }
                    pshtl.name = shtl;
                    shtl.clear();
                    continue;

                // Parse nested children.
                //
                // Note: we drop the top-level parenthesis here.
                }else if( *c_it == '{' ){
                    close_paren.push_back('}');
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

        if(!close_quote.empty()){
            throw std::invalid_argument("Imbalanced quote");
        }
        if(!close_paren.empty()){
            throw std::invalid_argument("Imbalanced parentheses");
        }

    }

    // Print the AST for debugging.
    if( debug
    &&  (parse_depth == 0) ){
        std::function<void(const std::vector<parsed_function> &, std::string)> print_ast;
        print_ast = [&print_ast](const std::vector<parsed_function> &pfv, std::string depth){
            for(auto &pf : pfv){
               FUNCINFO(depth << "name = '" << pf.name << "'");
               for(auto &p : pf.parameters){
                   FUNCINFO(depth << "  parameter: '" << p.raw << "'");
               }
               FUNCINFO(depth << "  children: " << pf.children.size());
               if(!pf.children.empty()){
                   print_ast(pf.children, depth + "    ");
               }
            }
        };
        print_ast(out, "");
    }

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

        if(!pf.children.empty()){
            pf.children = retain_only_numeric_parameters(pf.children);
        }
    }
    return pfs;
}


// Parser for number lists.
std::vector<double>
parse_numbers(const std::string &split_chars, const std::string &in){
    std::vector<std::string> split;
    split.emplace_back(in);
    for(const auto& c : split_chars){
        split = SplitVector(split, c, 'd');
    }

    std::vector<double> numbers;
    for(const auto &w : split){
       try{
           const auto x = std::stod(w);
           numbers.emplace_back(x);
       }catch(const std::exception &){ }
    }
    return numbers;
}

