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
    bool operator==(const char x) const {
        return (this->c == x);
    };
    bool operator==(const char_with_context_t &rhs) const {
        return (this->c == rhs.c);
    };
    void set_missing_lc_lcc(long int l_c, long int l_cc){
        if(this->lc < 0) this->lc = l_c;
        if(this->lcc < 0) this->lcc = l_cc;
        return;
    }
};

std::string to_str(const std::vector<char_with_context_t> &v){
    std::string out;
    for(const auto &c : v) out += c.c;
    return out;
}


// A parsed statement comprising multiple components.
//
// Note: permitted syntax:
//    
//    variable = "something";
//    function1(a = 123,
//              b = 234,
//              c = variable );
//    
//    # This is a comment. It should be ignored, including syntax errors like ;'"(]'\"".
//    # This is another comment.
//    function2(x = "123",
//              # This is another comment.
//              y = "456"){
//    
//        # This is a nested statement. Recursive statements are supported.
//        function3(z = 789);
//    
//    };
struct script_statement_t {
    std::vector<char_with_context_t> var_name; // "variable1".
    std::vector<char_with_context_t> func_name; // "function1".
    std::vector< std::pair< std::vector<char_with_context_t>,
                            std::vector<char_with_context_t> > > arguments; // Function arguments (x = y).
    std::vector<char_with_context_t> payload; // Contents of { } for functions, value for variables.
    std::vector<script_statement_t> child_statements;

    script_statement_t() = default;
    script_statement_t(const script_statement_t &) = default;
    script_statement_t & operator=(const script_statement_t &) = default;

    // Get the first available character. Used to provide a source location for feedback.
    char_with_context_t get_valid_cwct() const {
        if(!this->var_name.empty()) return this->var_name.front();
        if(!this->func_name.empty()) return this->func_name.front();
        for(const auto &ap : this->arguments){
            if(!ap.first.empty()) return ap.first.front();
            if(!ap.second.empty()) return ap.second.front();
        }
        if(!this->payload.empty()) return this->payload.front();
        throw std::logic_error("Statement was completely empty. Unable to provide cwct>");
        return char_with_context_t();
    };

    bool is_completely_empty() const {
        if(!this->var_name.empty()) return false;
        if(!this->func_name.empty()) return false;
        for(const auto &ap : this->arguments){
            if(!ap.first.empty()) return false;
            if(!ap.second.empty()) return false;
        }
        if(!this->payload.empty()) return false;
        return true;
    };

    bool is_var() const {
        return !this->var_name.empty()
            && !this->payload.empty()
            &&  this->func_name.empty();
    };
    bool is_func() const {
        return !this->func_name.empty()
            &&  this->var_name.empty();
    };
};

// Reports a message with accompanying character coordinates for the user.
void report( std::list<script_feedback_t> &feedback,
             script_feedback_severity_t severity,
             const char_with_context_t &c,
             const std::string &msg ){
    if(feedback.size() < 100){
        feedback.emplace_back();
        feedback.back().severity = severity;
        feedback.back().offset = c.cc;
        feedback.back().line = c.lc;
        feedback.back().line_offset = c.lcc;
        feedback.back().message = msg;
    }
    return;
}



// Split into statements, respecting quotations, parentheses, and escaping.
bool
Split_into_Statements( std::vector<char_with_context_t> &contents,
                       std::vector<script_statement_t> &statements,
                       const std::vector<script_statement_t> &variables,
                       std::list<script_feedback_t> &feedback ){

    std::vector<script_statement_t> l_statements;
    bool compilation_successful = true;

    const auto is_whitespace = [](const char_with_context_t &c){
        return std::isspace(static_cast<unsigned char>(c.c));
    };

    const auto trim_all_space = [is_whitespace](std::vector<char_with_context_t> &vc) -> void {
        vc.erase( std::remove_if( std::begin(vc), std::end(vc), is_whitespace ), std::end(vc) );
        return;
    };

    const auto trim_outer_space = [is_whitespace](std::vector<char_with_context_t> &vc) -> void {
        while( !vc.empty() && is_whitespace(vc.back()) ){
            vc.pop_back();
        }
        std::reverse( std::begin(vc), std::end(vc) );
        while( !vc.empty() && is_whitespace(vc.back()) ){
            vc.pop_back();
        }
        std::reverse( std::begin(vc), std::end(vc) );
        return;
    };

    const auto unquote = [](std::vector<char_with_context_t> &vc) -> void {
        if( (1 < vc.size())
        &&  (vc.front() == vc.back())
        &&  ((vc.front() == '"') || (vc.front() == '\'')) ){
            vc.pop_back();
            std::reverse( std::begin(vc), std::end(vc) );
            vc.pop_back();
            std::reverse( std::begin(vc), std::end(vc) );
        }
        return;
    };

    const auto contains_valid_identifier = [](const std::vector<char_with_context_t> &vc) -> bool {
        for(const auto &c : vc){
            const bool is_valid = std::isalnum(static_cast<unsigned char>(c.c))
                                  || (c.c == '.')
                                  || (c.c == '-')
                                  || (c.c == '_');
            if( !is_valid ) return false;
        }
        return true;
    };


    // Split into statements, respecting quotations and escaping.
    if(!contents.empty()){
        long int lcc = 0; // Line character count.
        long int lc = 0;  // Line count.

        l_statements.emplace_back();
        std::vector<char_with_context_t> shtl;
        std::vector<char_with_context_t> quote_stack; // Accounts for quotation.
        std::vector<char_with_context_t> curve_stack; // Accounts for ()s
        std::vector<char_with_context_t> bumpy_stack; // Accounts for {}s
        bool prev_escape = false;
        bool inside_comment = false;
        for(auto &c : contents){
            bool this_caused_escape = false;
            bool skip_character = false;

            // Fill-in missing character metadata.
            c.set_missing_lc_lcc(lc, lcc);

            // Comments.
            if(false){
            }else if( !prev_escape
                  &&  !inside_comment
                  &&  quote_stack.empty()
                  &&  (c == '#') ){
                skip_character = true;
                inside_comment = true;
//FUNCINFO("Opened comment");

            // Quotations.
            }else if( !prev_escape
                  &&  !inside_comment
                  &&  bumpy_stack.empty()
                  &&  ((c == '\"') || (c == '\'')) ){
                // Only permit a single quotation type at a time. Nesting not supported for quotes.
                if( !quote_stack.empty() ){
                    if(quote_stack.back() == c){
                        quote_stack.pop_back();
//FUNCINFO("Closed quotation");
                    }
                }else{
                    quote_stack.push_back(c);
//FUNCINFO("Opened quotation");
                }

            // Variable assignment.
            }else if( !prev_escape
                  &&  !inside_comment
                  &&  quote_stack.empty()
                  &&  curve_stack.empty()
                  &&  bumpy_stack.empty()
                  &&  (c == '=') ){
//FUNCINFO("Pushing back variable name '" << to_str(shtl) << "'");
                if(!l_statements.back().var_name.empty()){
                    report(feedback, script_feedback_severity_t::err, c, "Prior variable name provided");
                    compilation_successful = false;
                }
                l_statements.back().var_name = shtl;
                shtl.clear();
                skip_character = true;

            // Function argument assignment.
            }else if( !prev_escape
                  &&  !inside_comment
                  &&  quote_stack.empty()
                  &&  (curve_stack.size() == 1)
                  &&  (curve_stack.back() == '(')
                  &&  bumpy_stack.empty()
                  &&  (c == '=') ){
//FUNCINFO("Pushing back argument key '" << to_str(shtl) << "'");
                l_statements.back().arguments.emplace_back();
                l_statements.back().arguments.back().first = shtl;
                shtl.clear();
                skip_character = true;

            // Function parameters.
            }else if( !prev_escape
                  &&  !inside_comment
                  &&  quote_stack.empty()
                  &&  (curve_stack.size() == 1)
                  &&  (curve_stack.back() == '(')
                  &&  bumpy_stack.empty()
                  &&  (c == ',') ){
                if( !l_statements.back().arguments.empty()
                &&  !shtl.empty() ){
                    l_statements.back().arguments.back().second = shtl;
                    skip_character = true;
//FUNCINFO("Pushing back argument value '" << to_str(shtl) << "'");
                }else{
                    report(feedback, script_feedback_severity_t::err, c, "Ambiguous ','");
                    compilation_successful = false;
                }
                shtl.clear();


            // Parentheses opening.
            }else if( !prev_escape
                  &&  !inside_comment
                  &&  quote_stack.empty()
                  &&  bumpy_stack.empty()
                  &&  (c == '(') ){
//FUNCINFO("Pushing back function name '" << to_str(shtl) << "'");
                if(curve_stack.empty()){
                    l_statements.back().func_name = shtl;
                    curve_stack.push_back(c);
                    skip_character = true;
                }else{
                    report(feedback, script_feedback_severity_t::err, c, "Nested '('");
                }
                shtl.clear();

            }else if( !prev_escape
                  &&  !inside_comment
                  &&  quote_stack.empty()
                  &&  curve_stack.empty()
                  &&  (c == '{') ){

                if(bumpy_stack.empty()){
                    shtl.clear();
                    skip_character = true;
                }
                bumpy_stack.push_back(c);

            // Parentheses closing.
            }else if( !prev_escape
                  &&  !inside_comment
                  &&  quote_stack.empty()
                  &&  bumpy_stack.empty()
                  &&  (c == ')') ){

                if( (curve_stack.size() == 1)
                &&  (curve_stack.back() == '(')
                &&  !l_statements.back().arguments.empty() ){
//FUNCINFO("Pushing back argument value '" << to_str(shtl) << "'");
                    curve_stack.pop_back();
                    l_statements.back().arguments.back().second = shtl;
                    skip_character = true;
                }else{
                    report(feedback, script_feedback_severity_t::err, c, "Unmatched ')'"_s);
                    compilation_successful = false;
                }
                shtl.clear();

            }else if( !prev_escape
                  &&  !inside_comment
                  &&  quote_stack.empty()
                  &&  curve_stack.empty()
                  &&  !bumpy_stack.empty()
                  &&  (bumpy_stack.back() == '{')
                  &&  (c == '}') ){
                //if( !bumpy_stack.empty()
                //&&  (bumpy_stack.back() == '{') ){
                bumpy_stack.pop_back();
                l_statements.back().payload = shtl;
                if(bumpy_stack.empty()){
                    skip_character = true;
                    shtl.clear();
                }

            // Line endings.
            }else if( !prev_escape && (c == '\r') ){
                skip_character = true;
            }else if( !prev_escape && (c == '\n') ){
                lcc = 0;
                ++lc;
//if(inside_comment) FUNCINFO("Closed comment");
                inside_comment = false;
                skip_character = true;

            // Statement terminator.
            }else if( !prev_escape 
                  &&  !inside_comment
                  &&  quote_stack.empty()
                  &&  curve_stack.empty()
                  &&  bumpy_stack.empty()
                  &&  (c == ';') ){

                if( l_statements.back().var_name.empty()
                &&  l_statements.back().func_name.empty() ){
                    l_statements.back().var_name = shtl;

                }else if(!l_statements.back().var_name.empty()){
//FUNCINFO("Pushing back variable equals '" << to_str(shtl) << "'");
                    l_statements.back().payload = shtl;
                }

                l_statements.emplace_back();
                shtl.clear();
                skip_character = true;
//FUNCINFO("Created statement");

            // 'Noise' characters.
            }else if( !prev_escape
                  &&  !inside_comment
                  &&  (c == '\0') ){
                skip_character = true;

            // Escapes.
            }else if( !prev_escape
                  &&  !inside_comment
                  &&  quote_stack.empty()
                  &&  (c == '\\') ){
                prev_escape = true;
                this_caused_escape = true;
                skip_character = true;
            }
            
            // Handle the input, ignoring noise characters.
            if( !skip_character 
            &&  !inside_comment ) shtl.push_back(c);
            ++lcc;

            // Disable escape, if needed.
            if(!this_caused_escape) prev_escape = false;
        }

        if( !shtl.empty()
        &&  !std::all_of(std::begin(shtl), std::end(shtl), is_whitespace) ){
//FUNCINFO("Trailing input has shtl = '" << to_str(shtl) << "'");
            report(feedback, script_feedback_severity_t::err, contents.back(),
                   "Trailing input. (Are you missing a semicolon?)");
            compilation_successful = false;
        }

        // Check that there are no open quotes / parentheses.
        const auto report_unmatched = [&]( const std::vector<char_with_context_t> &v ){
            for(const auto &c : v){
                report(feedback, script_feedback_severity_t::err, c, "Unmatched '"_s + c.c + "'");
                compilation_successful = false;
            }
            return;
        };
        report_unmatched(quote_stack);
        report_unmatched(curve_stack);
        report_unmatched(bumpy_stack);
    }

    // Trim outermost quotes and whitespace.
    for(auto &s : l_statements){
        trim_outer_space(s.var_name);
        trim_outer_space(s.payload);
        unquote(s.payload);

        trim_outer_space(s.func_name);
        for(auto &a : s.arguments){
            trim_outer_space(a.first);
            trim_outer_space(a.second);
            unquote(a.second);
        }

        trim_outer_space(s.payload);
        unquote(s.payload);
    }

    // Remove any completely empty statements.
    if(!l_statements.empty()){
        l_statements.erase( std::remove_if( std::begin(l_statements), std::end(l_statements),
            [](const script_statement_t &s){ return s.is_completely_empty(); }),
             std::end(l_statements) );
    }

    ////////////////////////////////////////////////////////////////////
    // Validate statements are reasonable.
    ////////////////////////////////////////////////////////////////////

    for(auto &s : l_statements){
        // Every statement is either a variable assignment, or a function.
        if( s.is_var() == s.is_func() ){
            report(feedback, script_feedback_severity_t::err, s.get_valid_cwct(),
                   "Statement is neither a variable assignment, nor a function.");
            compilation_successful = false;
        }

        // Names contain only valid characters.
        if( s.is_var() && !contains_valid_identifier(s.var_name) ){
            report(feedback, script_feedback_severity_t::err, s.var_name.front(),
                   "Variable contains forbidden identifier characters.");
            compilation_successful = false;
        }
        if( s.is_func() && !contains_valid_identifier(s.func_name) ){
            report(feedback, script_feedback_severity_t::err, s.func_name.front(),
                   "Operation contains forbidden identifier characters.");
            compilation_successful = false;
        }
        if( s.is_func() ){
            for(const auto &ap : s.arguments){
                if(!contains_valid_identifier(ap.first) ){
                    report(feedback, script_feedback_severity_t::err, s.get_valid_cwct(),
                           "Operation argument name contains forbidden identifier characters.");
                    compilation_successful = false;
                }
            }

        }
    }

    // All function arguments are unique and non-empty.
    for(auto &s : l_statements){
        const auto end_it = std::end(s.arguments);
        for(auto a_it = std::begin(s.arguments); a_it != end_it; ++a_it){
            if(a_it->first.empty()){
                report(feedback, script_feedback_severity_t::err, s.get_valid_cwct(),
                       "Argument is unnamed.");
                compilation_successful = false;

            }else{
                for(auto b_it = std::next(a_it); b_it != end_it; ++b_it){
                    if(a_it->first == b_it->first){
                        report(feedback, script_feedback_severity_t::warn, b_it->first.front(),
                               "Duplicate function argument specified (duplicated from line "_s
                               + std::to_string(a_it->first.front().lc) + ").");
                    }
                }
            }
        }
    }

    // All variables (defined in the current scope) are unique and non-empty.
    for(auto s1_it = std::begin(l_statements); s1_it != std::end(l_statements); ++s1_it){
        for(auto s2_it = std::next(s1_it); s2_it != std::end(l_statements); ++s2_it){
            if( s1_it->is_var()
            &&  s2_it->is_var()
            &&  (to_str(s1_it->var_name) == to_str(s2_it->var_name)) ){
                report(feedback, script_feedback_severity_t::warn, s2_it->get_valid_cwct(),
                       "Duplicate variable assignment (previously assigned on line "_s
                       + std::to_string(s1_it->get_valid_cwct().lc) + ").");
            }
        }
    }

    // Warn when variables supercede prior variable definitions.
    std::vector<script_statement_t> l_variables;
    for(const auto &v : variables){
        if( !v.is_var() ){
            report(feedback, script_feedback_severity_t::err, v.get_valid_cwct(),
                   "Unable to handle as a variable.");
            compilation_successful = false;
        }

        bool is_superceded = false;
        for(const auto &s : l_statements){
            if( s.is_var()
            &&  v.is_var()
            &&  (to_str(s.var_name) == to_str(v.var_name)) ){
                report(feedback, script_feedback_severity_t::warn, s.get_valid_cwct(),
                       "Variable declaration supercedes earlier definition (on line "_s
                       + std::to_string(v.get_valid_cwct().lc) + ").");
                is_superceded = true;
            }
        }

        if(!is_superceded) l_variables.emplace_back(v);
    }
    // Add local variables in reverse order to later variable assignments supercede earlier assignments.
    for(auto s_it = std::rbegin(l_statements); s_it != std::rend(l_statements); ++s_it){
        if(s_it->is_var()) l_variables.emplace_back(*s_it);
    }

    // Perform variable replacements.
    //
    // Note: at the moment, only exact matches are supported. Arithmetic expressions, for example, are not currently
    // supported.
    for(auto &s : l_statements){
        long int iter = 0;
        while(true){
            bool replacement_made = false;
            for(const auto &v : l_variables){
                const auto var_name = to_str(v.var_name);
                if(s.is_var()){
                    if(var_name == to_str(s.payload)){
                        s.payload = v.payload;
                        replacement_made = true;
                    }
                }
                if(s.is_func()){
                    if(var_name == to_str(s.func_name)){
                        s.func_name = v.payload;
                        replacement_made = true;
                    }
                    for(auto &a : s.arguments){
                        if(var_name == to_str(a.second)){
                            a.second = v.payload;
                            replacement_made = true;
                        }
                    }
                }
            }
            if( 100L < ++iter ){
                report(feedback, script_feedback_severity_t::err, s.get_valid_cwct(),
                       "Variable replacement exceeded 100 replacement iterations.");
                compilation_successful = false;
                break;
            }
            if(!replacement_made) break;
        }
    }
    // Remove variables from the statements.
    l_statements.erase( std::remove_if( std::begin(l_statements), std::end(l_statements),
                                        [](const script_statement_t &s){ return s.is_var(); } ),
                        std::end(l_statements) );

    // Recurse for operations that have payloads, extracting nested statements.
    for(auto &s : l_statements){
        if(s.is_func() && !s.payload.empty()){
            const bool res = Split_into_Statements(s.payload,
                                                   s.child_statements,
                                                   l_variables,
                                                   feedback);
            if(res){
                s.payload.clear();
            }else{
                compilation_successful = false;
            }
        }
    }

    if(compilation_successful){
        statements.insert( std::end(statements), std::begin(l_statements), std::end(l_statements) );
    }
    return compilation_successful;
}

bool Load_DCMA_Script(std::istream &is,
                      std::list<script_feedback_t> &feedback,
                      std::list<OperationArgPkg> &op_list){
    std::list<OperationArgPkg> out;

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

    std::vector<script_statement_t> statements;
    std::vector<script_statement_t> variables;
    if(!Split_into_Statements(contents, statements, variables, feedback)){
        return false;
    }


    std::function<void(const std::vector<script_statement_t> &, std::string)> recursively_print_statements;
    recursively_print_statements = [&](const std::vector<script_statement_t> &statements,
                                       std::string spacing) -> void {
        std::cout << spacing << "==============================================================================================" << std::endl;
        for(const auto &s : statements){
            std::cout << spacing << "Var name   : '";
            for(const auto &c : s.var_name) std::cout << c.c;
            std::cout << "'" << std::endl;

            std::cout << spacing << "Func name  : '";
            for(const auto &c : s.func_name) std::cout << c.c;
            std::cout << "'" << std::endl;

            std::cout << spacing << "Arguments  : " << std::endl;
            for(const auto &a : s.arguments){
                std::cout << spacing << "  Name       : '";
                for(const auto &c : a.first) std::cout << c.c;
                std::cout << "'" << std::endl;

                std::cout << spacing << "  Value      : '";
                for(const auto &c : a.second) std::cout << c.c;
                std::cout << "'" << std::endl;
            }

            std::cout << spacing << "Payload    : '";
            for(const auto &c : s.payload) std::cout << c.c;
            std::cout << "'" << std::endl;

            std::cout << spacing << "Children   : " << std::endl;;
            if(!s.child_statements.empty()) recursively_print_statements(s.child_statements, spacing + "    ");
            std::cout << spacing << "==============================================================================================" << std::endl;
        }
        return;
    };
    recursively_print_statements(statements, "    ");

    report(feedback, script_feedback_severity_t::info, contents.back(), "OK");
    
    op_list.splice( std::end(op_list), out);
    return true;
}

