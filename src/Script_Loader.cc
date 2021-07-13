// Script_Loader.cc - A part of DICOMautomaton 2021. Written by hal clark.
//
// This program loads DICOMautomaton scripts from ASCII text files.
//

#include <istream>
#include <sstream>
#include <list>

#include <cmath>
#include <cctype>
#include <algorithm>
#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    

#include <cstdlib>            //Needed for exit() calls.

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathIOSTL.h"
#include "YgorString.h"       //Needed for SplitStringToVector, Canonicalize_String2, SplitVector functions.

#include "Script_Loader.h"
#include "Structs.h"

bool Load_DCMA_Script(std::istream &is,
                      std::list<script_feedback_t> &feedback,
                      std::list<OperationArgPkg> &op_list){
    std::list<OperationArgPkg> out;


    // Treat the file like a linear string of characters, ignoring whitespace.
    std::string contents;
    std::copy(std::istreambuf_iterator<char>(is),
              std::istreambuf_iterator<char>(),
              std::back_inserter(contents));
//FUNCINFO("The script file contents are: '" << contents << "'");

    // Split into statements, respecting quotations and escaping.

    std::vector<std::string> statements;
    {
        long int cc = 0;  // Character count.
        long int lcc = 0; // Line character count.
        long int lc = 0;  // Line count.

        // Reports a message with accompanying character coordinates for the user.
        const auto report_info = [&](const std::string &msg){
            feedback.emplace_back();
            feedback.back().severity = script_feedback_severity_t::info;
            feedback.back().offset = cc;
            feedback.back().line = lc;
            feedback.back().line_offset = lcc;
            feedback.back().message = msg;
            return;
        };
        const auto report_error = [&](const std::string &msg){
            feedback.emplace_back();
            feedback.back().severity = script_feedback_severity_t::err;
            feedback.back().offset = cc;
            feedback.back().line = lc;
            feedback.back().line_offset = lcc;
            feedback.back().message = msg;
            return;
        };

// TODO: convert to a custom character type with char count attributes? (Much easier diagnostics...)

        std::string shtl;
        std::string quote_stack; // Accounts for quotation.
        std::string level_stack; // Accounts for parentheses nesting.
        bool prev_escape = false;
        bool inside_comment = false;
        for(const auto &c : contents){
            bool this_caused_escape = false;
            bool skip_character = false;

            // Comments.
            if(false){
            }else if( !prev_escape && (c == '#') ){
                skip_character = true;
                inside_comment = true;

            // Quotations.
            }else if( !prev_escape && !inside_comment && ((c == '\"') || (c == '\'')) ){
                // Only permit a single quotation type at a time. Nesting not supported for quotes.
                if( !quote_stack.empty() ){
                    if(quote_stack.back() == c) quote_stack.pop_back();
                }else{
                    quote_stack.push_back(c);
                }

            // Parentheses nesting.
            }else if( !prev_escape && !inside_comment && ((c == '(') || (c == '{') || (c == '[')) ){
                level_stack.push_back(c);

            }else if( !prev_escape && !inside_comment && (c == ')') ){
                if( !level_stack.empty() && (level_stack.back() == '(') ){
                    level_stack.pop_back();
                }else{
                    report_error("Unmatched ')'");
                    return false;
                }

            }else if( !prev_escape && !inside_comment && (c == '}') ){
                if( !level_stack.empty() && (level_stack.back() == '{') ){
                    level_stack.pop_back();
                }else{
                    report_error("Unmatched '}'");
                    return false;
                }

            }else if( !prev_escape && !inside_comment && (c == ']') ){
                if( !level_stack.empty() && (level_stack.back() == '[') ){
                    level_stack.pop_back();
                }else{
                    report_error("Unmatched ']'");
                    return false;
                }

            // Line endings.
            }else if( !prev_escape && (c == '\r') ){
                skip_character = true;
            }else if( !prev_escape && (c == '\n') ){
                lcc = 0;
                ++lc;
                inside_comment = false;
                skip_character = true;

            // Statement terminator.
            }else if( !prev_escape 
                  &&  !inside_comment
                  &&  (c == ';')
                  &&  quote_stack.empty() ){

                if( !level_stack.empty() ){
                    report_error("Unmatched parenthesis ("_s + level_stack.front() + ")");
                    return false;
                }
                statements.emplace_back(shtl);
                shtl.clear();
                skip_character = true;

            // 'Noise' characters.
            }else if( !prev_escape && !inside_comment && (c == '\0') ){
                skip_character = true;

            // Escapes.
            }else if( !prev_escape && !inside_comment && (c == '\\')){
                prev_escape = true;
                this_caused_escape = true;
            }
            
            // Handle the input, ignoring noise characters.
            if( !skip_character 
            &&  !inside_comment ) shtl.push_back(c);
            ++lcc;
            ++cc;

            // Disable escape, if needed.
            if(!this_caused_escape) prev_escape = false;
        }

        if( !shtl.empty()
        &&  !std::all_of(std::begin(shtl), std::end(shtl),
                         [](unsigned char c){ return std::isspace(c); }) ){
            --lcc;
            report_error("Trailing input. (Are you missing a semicolon?)");
            return false;
        }

        // Check that there are no open quotes / parentheses.
        if( !quote_stack.empty() ){
            report_error("Unmatched quotation ("_s + quote_stack.front() + ")");
            return false;
        }
        if( !level_stack.empty() ){
            report_error("Unmatched parenthesis ("_s + level_stack.front() + ")");
            return false;
        }
        report_info("OK");
    }

    
// for(size_t i = 0; i < statements.size(); ++i){
// FUNCINFO("Statement " << i << " = '" << Quote_Static_for_Bash( statements[i] ));
// }



/* 
    const auto regex_comment = Compile_Regex(R"***(^[ \t]*#.*|^[ \t]*\/\/.*)***");

    size_t i = 0; // Current character.

    std::string contents;
    std::string line;
    while(std::getline(is, line)){
        if(std::regex_match(line, regex_comment)) continue;
        contents += line + " ";
    }

    // Split on semicolons.

    // ... TODO ...

    // Separate operation names, parameters, and nested children.

    // ... TODO ...
*/

    //op_list.merge(out);
    return true;
}

