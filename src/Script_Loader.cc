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

// A parsed character from a stream that is imbued with additional metadata from the stream.
struct char_with_context_t {
    char c = '\0';
    long int cc  = -1; // Total character count (offset from beginning of stream/file).
    long int lc  = -1; // Line count (which line the character is on).
    long int lcc = -1; // Line character count (offset from beginning of line).

    char_with_context_t() = default;
    char_with_context_t(const char_with_context_t &) = default;
    char_with_context_t & operator=(const char_with_context_t &) = default;
    bool operator==(const char x){
        return (this->c == x);
    };
    bool operator==(const char_with_context_t &rhs){
        return (this->c == rhs.c);
    };
    void set_missing_lc_lcc(long int l_c, long int l_cc){
        if(this->lc < 0) this->lc = l_c;
        if(this->lcc < 0) this->lcc = l_cc;
        return;
    }
};

// Reports a message with accompanying character coordinates for the user.

void report( std::list<script_feedback_t> &feedback,
             script_feedback_severity_t severity,
             const char_with_context_t &c,
             const std::string &msg ){
    feedback.emplace_back();
    feedback.back().severity = severity;
    feedback.back().offset = c.cc;
    feedback.back().line = c.lc;
    feedback.back().line_offset = c.lcc;
    feedback.back().message = msg;
    return;
};


// Split into statements, respecting quotations, parentheses, and escaping.
bool
Split_into_Statements( std::vector<char_with_context_t> &contents,
                       std::vector<std::vector<char_with_context_t>> &statements,
                       std::list<script_feedback_t> &feedback ){

    // Split into statements, respecting quotations and escaping.
    if(!contents.empty()){
        long int lcc = 0; // Line character count.
        long int lc = 0;  // Line count.

        std::vector<char_with_context_t> shtl;
        std::vector<char_with_context_t> quote_stack; // Accounts for quotation.
        std::vector<char_with_context_t> level_stack; // Accounts for parentheses nesting.
        bool prev_escape = false;
        bool inside_comment = false;
        for(auto &c : contents){
            bool this_caused_escape = false;
            bool skip_character = false;

            // Fill-in missing character metadata.
            c.set_missing_lc_lcc(lc, lcc);

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
                    report(feedback, script_feedback_severity_t::err, c,
                                    "Unmatched ')'"_s
                                    + " (conflicting '"_s + level_stack.back().c 
                                    + "' on line "_s + std::to_string(level_stack.back().lc) + "?)");
                    return false;
                }

            }else if( !prev_escape && !inside_comment && (c == '}') ){
                if( !level_stack.empty() && (level_stack.back() == '{') ){
                    level_stack.pop_back();
                }else{
                    report(feedback, script_feedback_severity_t::err, c,
                                    "Unmatched '}'"_s
                                    + " (conflicting '"_s + level_stack.back().c 
                                    + "' on line "_s + std::to_string(level_stack.back().lc) + "?)");
                    return false;
                }

            }else if( !prev_escape && !inside_comment && (c == ']') ){
                if( !level_stack.empty() && (level_stack.back() == '[') ){
                    level_stack.pop_back();
                }else{
                    report(feedback, script_feedback_severity_t::err, c,
                                    "Unmatched ']'"_s
                                    + " (conflicting '"_s + level_stack.back().c 
                                    + "' on line "_s + std::to_string(level_stack.back().lc) + "?)");
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
                  &&  quote_stack.empty()
                  &&  level_stack.empty() ){

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

            // Disable escape, if needed.
            if(!this_caused_escape) prev_escape = false;
        }

        if( !shtl.empty()
        &&  !std::all_of(std::begin(shtl), std::end(shtl),
                         [](const char_with_context_t &c){ return std::isspace(static_cast<char>(c.c)); }) ){
            report(feedback, script_feedback_severity_t::err, contents.back(),
                   "Trailing input. (Are you missing a semicolon?)");
            return false;
        }

        // Check that there are no open quotes / parentheses.
        if( !quote_stack.empty() ){
            report(feedback, script_feedback_severity_t::err, contents.back(),
                   "Unmatched quotation ("_s + quote_stack.back().c + " from line "_s + std::to_string(quote_stack.back().lc) + ")");
            return false;
        }
        if( !level_stack.empty() ){
            report(feedback, script_feedback_severity_t::err, contents.back(),
                   "Unmatched parenthesis ("_s + level_stack.back().c + " from line "_s + std::to_string(quote_stack.back().lc) + ")");
            return false;
        }
    }
    return true;
}

bool Load_DCMA_Script(std::istream &is,
                      std::list<script_feedback_t> &feedback,
                      std::list<OperationArgPkg> &op_list){

    // Treat the file like a linear string of characters, including whitespace.
    std::vector<char_with_context_t> contents;
    {
        const auto end_it = std::istreambuf_iterator<char>();
        long int i = 0;
        for(auto c_it = std::istreambuf_iterator<char>(is); c_it != end_it; ++c_it, ++i){
            contents.emplace_back();
            contents.back().c  = *c_it;
            contents.back().cc = i;
        }
    }

    std::list<OperationArgPkg> out;
    std::vector<std::vector<char_with_context_t>> statements;
    if(!Split_into_Statements(contents, statements, feedback)){
        return false;
    }

    report(feedback, script_feedback_severity_t::info, contents.back(), "OK");
    
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

