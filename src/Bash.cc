//Bash.cc - A part of DICOMautomaton 2026. Written by OpenAI.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "Bash.h"

namespace {

std::string trim_copy(const std::string &in){
    auto begin = std::find_if_not(in.begin(), in.end(), [](unsigned char c){ return std::isspace(c); });
    auto end = std::find_if_not(in.rbegin(), in.rend(), [](unsigned char c){ return std::isspace(c); }).base();
    if(begin >= end) return "";
    return std::string(begin, end);
}

bool starts_with(const std::string &text, const std::string &prefix){
    return text.rfind(prefix, 0) == 0;
}

std::string join_strings(const std::vector<std::string> &items, const std::string &separator){
    std::ostringstream ss;
    bool first = true;
    for(const auto &item : items){
        if(!first) ss << separator;
        first = false;
        ss << item;
    }
    return ss.str();
}

std::string strip_trailing_newline(std::string text){
    while(!text.empty() && ((text.back() == '\n') || (text.back() == '\r'))){
        text.pop_back();
    }
    return text;
}

std::vector<std::string> split_lines(std::istream &is){
    std::vector<std::string> out;
    std::string line;
    while(std::getline(is, line)){
        out.emplace_back(strip_trailing_newline(line));
    }
    return out;
}

std::vector<std::string> split_lines(const std::string &text){
    std::istringstream is(text);
    return split_lines(is);
}

bool path_is_hidden(const std::filesystem::path &path){
    const auto name = path.filename().string();
    return (!name.empty() && (name.front() == '.'));
}

std::string wildcard_to_regex(const std::string &pattern){
    std::string out = "^";
    for(const auto c : pattern){
        switch(c){
            case '*': out += ".*"; break;
            case '?': out += "."; break;
            case '.':
            case '+':
            case '(':
            case ')':
            case '{':
            case '}':
            case '^':
            case '$':
            case '|':
            case '\\':
                out.push_back('\\');
                out.push_back(c);
                break;
            default:
                out.push_back(c);
                break;
        }
    }
    out += "$";
    return out;
}

std::int64_t to_integer_or_zero(const std::string &text){
    try{
        size_t consumed = 0;
        const auto v = std::stoll(trim_copy(text), &consumed, 10);
        if(consumed != trim_copy(text).size()) return 0;
        return v;
    }catch(...){
        return 0;
    }
}

class ArithmeticParser {
public:
    ArithmeticParser(const std::string &expr,
                     const std::function<std::int64_t(const std::string&)> &lookup)
        : expr_(expr), lookup_(lookup) {}

    std::int64_t parse(){
        pos_ = 0;
        const auto value = parse_expression();
        skip_ws();
        if(pos_ != expr_.size()){
            throw std::runtime_error("Unable to parse arithmetic expression");
        }
        return value;
    }

private:
    const std::string &expr_;
    std::function<std::int64_t(const std::string&)> lookup_;
    size_t pos_ = 0;

    void skip_ws(){
        while((pos_ < expr_.size()) && std::isspace(static_cast<unsigned char>(expr_[pos_]))) ++pos_;
    }

    bool consume(char c){
        skip_ws();
        if((pos_ < expr_.size()) && (expr_[pos_] == c)){
            ++pos_;
            return true;
        }
        return false;
    }

    bool consume(const std::string &text){
        skip_ws();
        if(expr_.compare(pos_, text.size(), text) == 0){
            pos_ += text.size();
            return true;
        }
        return false;
    }

    std::int64_t parse_expression(){
        auto value = parse_term();
        while(true){
            if(consume('+')){
                value += parse_term();
            }else if(consume('-')){
                value -= parse_term();
            }else{
                break;
            }
        }
        return value;
    }

    std::int64_t parse_term(){
        auto value = parse_factor();
        while(true){
            if(consume('*')){
                value *= parse_factor();
            }else if(consume('/')){
                const auto rhs = parse_factor();
                value = (rhs == 0) ? 0 : (value / rhs);
            }else if(consume('%')){
                const auto rhs = parse_factor();
                value = (rhs == 0) ? 0 : (value % rhs);
            }else{
                break;
            }
        }
        return value;
    }

    std::int64_t parse_factor(){
        skip_ws();
        if(consume('+')) return parse_factor();
        if(consume('-')) return -parse_factor();
        if(consume('(')){
            const auto value = parse_expression();
            if(!consume(')')){
                throw std::runtime_error("Expected ')' in arithmetic expression");
            }
            return value;
        }

        skip_ws();
        if(pos_ >= expr_.size()) throw std::runtime_error("Unexpected end of arithmetic expression");

        if(std::isdigit(static_cast<unsigned char>(expr_[pos_]))){
            const auto start = pos_;
            while((pos_ < expr_.size()) && std::isdigit(static_cast<unsigned char>(expr_[pos_]))) ++pos_;
            return std::stoll(expr_.substr(start, pos_ - start));
        }

        if(std::isalpha(static_cast<unsigned char>(expr_[pos_])) || (expr_[pos_] == '_')){
            const auto start = pos_;
            while((pos_ < expr_.size())
               && (std::isalnum(static_cast<unsigned char>(expr_[pos_])) || (expr_[pos_] == '_'))){
                ++pos_;
            }
            return lookup_(expr_.substr(start, pos_ - start));
        }

        throw std::runtime_error("Unexpected token in arithmetic expression");
    }
};

} // namespace

Bash::Bash()
    : Bash(std::filesystem::current_path(), {}){}

Bash::Bash(std::filesystem::path cwd, Environment env)
    : environment_(std::move(env)),
      cwd_(std::move(cwd)){
    initialize_environment();
    register_builtins();
    update_pwd();
}

void Bash::initialize_environment(){
    const std::vector<std::string> names = {
        "HOME", "PATH", "TMPDIR", "TEMP", "TMP", "USER", "USERNAME", "SHELL"
    };

    for(const auto &name : names){
        if(environment_.count(name) != 0U) continue;
        if(const auto *value = std::getenv(name.c_str()); value != nullptr){
            environment_[name] = value;
        }
    }

    if(cwd_.empty()){
        cwd_ = std::filesystem::current_path();
    }
    cwd_ = cwd_.lexically_normal();
    environment_["PWD"] = cwd_.string();
    if(environment_.count("OLDPWD") == 0U) environment_["OLDPWD"] = cwd_.string();
}

void Bash::register_builtins(){
    builtins_["echo"] = [this](const auto &args, const auto &input){ return builtin_echo(args, input); };
    builtins_["pwd"] = [this](const auto &args, const auto &input){ return builtin_pwd(args, input); };
    builtins_["cd"] = [this](const auto &args, const auto &input){ return builtin_cd(args, input); };
    builtins_["ls"] = [this](const auto &args, const auto &input){ return builtin_ls(args, input); };
    builtins_["export"] = [this](const auto &args, const auto &input){ return builtin_export(args, input); };
    builtins_["setenv"] = [this](const auto &args, const auto &input){ return builtin_setenv(args, input); };
    builtins_["unset"] = [this](const auto &args, const auto &input){ return builtin_unset(args, input); };
    builtins_["env"] = [this](const auto &args, const auto &input){ return builtin_env(args, input); };
    builtins_["set"] = [this](const auto &args, const auto &input){ return builtin_set(args, input); };
    builtins_["true"] = [this](const auto &args, const auto &input){ return builtin_true(args, input); };
    builtins_["false"] = [this](const auto &args, const auto &input){ return builtin_false(args, input); };
    builtins_["test"] = [this](const auto &args, const auto &input){ return builtin_test(args, input); };
    builtins_["["] = [this](const auto &args, const auto &input){ return builtin_test(args, input); };
    builtins_["mkdir"] = [this](const auto &args, const auto &input){ return builtin_mkdir(args, input); };
    builtins_["rm"] = [this](const auto &args, const auto &input){ return builtin_rm(args, input); };
    builtins_["touch"] = [this](const auto &args, const auto &input){ return builtin_touch(args, input); };
    builtins_["cat"] = [this](const auto &args, const auto &input){ return builtin_cat(args, input); };
    builtins_["cp"] = [this](const auto &args, const auto &input){ return builtin_cp(args, input); };
    builtins_["mv"] = [this](const auto &args, const auto &input){ return builtin_mv(args, input); };
    builtins_["grep"] = [this](const auto &args, const auto &input){ return builtin_grep(args, input); };
    builtins_["find"] = [this](const auto &args, const auto &input){ return builtin_find(args, input); };
    builtins_["basename"] = [this](const auto &args, const auto &input){ return builtin_basename(args, input); };
    builtins_["dirname"] = [this](const auto &args, const auto &input){ return builtin_dirname(args, input); };
    builtins_["printf"] = [this](const auto &args, const auto &input){ return builtin_printf(args, input); };
    builtins_["exit"] = [this](const auto &args, const auto &input){ return builtin_exit(args, input); };
}

Bash::Result Bash::process(const std::vector<std::string> &script_lines){
    return process(join_strings(script_lines, "\n"));
}

Bash::Result Bash::process(const std::string &script){
    exit_requested_ = false;
    const auto tokens = tokenize(script);
    auto res = run_tokens(tokens, 0, tokens.size(), {}, true);
    last_status_ = res.return_code;

    Result out;
    out.return_code = res.return_code;
    out.output = std::move(res.output);
    out.environment = environment_;
    return out;
}

const std::filesystem::path& Bash::current_working_directory() const{
    return cwd_;
}

void Bash::set_current_working_directory(const std::filesystem::path &cwd){
    cwd_ = cwd.lexically_normal();
    update_pwd();
}

std::optional<std::string> Bash::get_environment_variable(const std::string &name) const{
    const auto it = environment_.find(name);
    if(it == environment_.end()) return std::nullopt;
    return it->second;
}

void Bash::set_environment_variable(const std::string &name, const std::string &value){
    environment_[name] = value;
    if(name == "PWD"){
        cwd_ = std::filesystem::path(value).lexically_normal();
    }
}

void Bash::unset_environment_variable(const std::string &name){
    environment_.erase(name);
}

std::vector<Bash::Token> Bash::tokenize(const std::string &script) const{
    std::vector<Token> tokens;
    std::string current;
    bool in_single = false;
    bool in_double = false;
    bool escape = false;
    bool at_word_boundary = true;
    int substitution_depth = 0;

    const auto flush = [&](){
        if(!current.empty()){
            tokens.push_back({current});
            current.clear();
        }
    };

    for(size_t i = 0; i < script.size(); ++i){
        const char c = script[i];

        if(escape){
            current.push_back(c);
            escape = false;
            at_word_boundary = false;
            continue;
        }

        if(!in_single && (c == '\\')){
            current.push_back(c);
            escape = true;
            at_word_boundary = false;
            continue;
        }

        if(!in_double && (c == '\'')){
            current.push_back(c);
            in_single = !in_single;
            at_word_boundary = false;
            continue;
        }

        if(!in_single && (c == '"')){
            current.push_back(c);
            in_double = !in_double;
            at_word_boundary = false;
            continue;
        }

        if(!in_single && !in_double && (substitution_depth > 0)){
            current.push_back(c);
            if(c == '(') ++substitution_depth;
            else if(c == ')') --substitution_depth;
            at_word_boundary = false;
            continue;
        }

        if(!in_single && !in_double){
            if((c == '$') && ((i + 1) < script.size()) && (script[i + 1] == '(')){
                current.push_back(c);
                current.push_back(script[++i]);
                ++substitution_depth;
                if(((i + 1) < script.size()) && (script[i + 1] == '(')){
                    current.push_back(script[++i]);
                    ++substitution_depth;
                }
                at_word_boundary = false;
                continue;
            }

            if((c == '#') && at_word_boundary){
                flush();
                while((i < script.size()) && (script[i] != '\n')) ++i;
                if(i < script.size()) tokens.push_back({";"});
                at_word_boundary = true;
                continue;
            }

            if(std::isspace(static_cast<unsigned char>(c))){
                flush();
                if(c == '\n') tokens.push_back({";"});
                at_word_boundary = true;
                continue;
            }

            const auto next = ((i + 1) < script.size()) ? script[i + 1] : '\0';
            if(((c == '&') && (next == '&')) || ((c == '|') && (next == '|')) || ((c == '>') && (next == '>'))){
                flush();
                tokens.push_back({std::string() + c + next});
                ++i;
                at_word_boundary = true;
                continue;
            }
            if((c == ';') || (c == '|') || (c == '<') || (c == '>')){
                flush();
                tokens.push_back({std::string(1, c)});
                at_word_boundary = true;
                continue;
            }
        }

        current.push_back(c);
        at_word_boundary = false;
    }

    flush();
    return tokens;
}

Bash::CommandResult Bash::run_tokens(const std::vector<Token> &tokens,
                                     size_t begin,
                                     size_t end,
                                     const std::vector<std::string> &terminators,
                                     bool execute){
    CommandResult final_result;
    size_t pos = skip_separators(tokens, begin, end);

    while(pos < end){
        if(is_terminator(tokens[pos].text, terminators)) break;
        auto [result, next] = run_and_or(tokens, pos, end, terminators, execute);
        if(execute){
            final_result.return_code = result.return_code;
            final_result.output.insert(final_result.output.end(), result.output.begin(), result.output.end());
        }
        pos = skip_separators(tokens, next, end);
        if(exit_requested_) break;
    }

    return final_result;
}

std::pair<Bash::CommandResult, size_t> Bash::run_and_or(const std::vector<Token> &tokens,
                                                        size_t pos,
                                                        size_t end,
                                                        const std::vector<std::string> &terminators,
                                                        bool execute){
    auto [result, next] = run_pipeline(tokens, pos, end, terminators, execute);
    while(next < end){
        if(is_terminator(tokens[next].text, terminators)) break;
        if((tokens[next].text != "&&") && (tokens[next].text != "||")) break;

        const auto op = tokens[next].text;
        const bool rhs_execute = execute && ((op == "&&") ? (result.return_code == 0) : (result.return_code != 0));
        auto [rhs_result, rhs_next] = run_pipeline(tokens, next + 1, end, terminators, rhs_execute);
        if(rhs_execute){
            result.return_code = rhs_result.return_code;
            result.output.insert(result.output.end(), rhs_result.output.begin(), rhs_result.output.end());
        }
        next = rhs_next;
    }
    return {result, next};
}

std::pair<Bash::CommandResult, size_t> Bash::run_pipeline(const std::vector<Token> &tokens,
                                                          size_t pos,
                                                          size_t end,
                                                          const std::vector<std::string> &terminators,
                                                          bool execute){
    std::vector<std::string> pipeline_input;
    CommandResult last_result;
    size_t next = pos;

    while(next < end){
        auto [result, cmd_next] = run_command(tokens, next, end, terminators, execute, pipeline_input);
        if(execute) last_result = result;
        next = cmd_next;
        if((next >= end) || (tokens[next].text != "|")) break;
        pipeline_input = execute ? last_result.output : std::vector<std::string>{};
        ++next;
    }

    return {last_result, next};
}

std::pair<Bash::CommandResult, size_t> Bash::run_command(const std::vector<Token> &tokens,
                                                         size_t pos,
                                                         size_t end,
                                                         const std::vector<std::string> &terminators,
                                                         bool execute,
                                                         const std::vector<std::string> &input){
    if(pos >= end) return {CommandResult{}, pos};
    if(is_terminator(tokens[pos].text, terminators)) return {CommandResult{}, pos};

    if(tokens[pos].text == "if") return run_if_command(tokens, pos, end, execute);
    if(tokens[pos].text == "while") return run_while_command(tokens, pos, end, execute);
    return run_simple_command(tokens, pos, end, execute, input);
}

std::pair<Bash::CommandResult, size_t> Bash::run_if_command(const std::vector<Token> &tokens,
                                                            size_t pos,
                                                            size_t end,
                                                            bool execute){
    const auto start = pos + 1;
    size_t depth = 0;
    size_t then_pos = end;
    size_t else_pos = end;
    size_t fi_pos = end;

    for(size_t i = start; i < end; ++i){
        const auto &text = tokens[i].text;
        if(text == "if") ++depth;
        else if(text == "fi"){
            if(depth == 0){
                fi_pos = i;
                break;
            }
            --depth;
        }else if(text == "then"){
            if(depth == 0 && then_pos == end) then_pos = i;
        }else if(text == "else"){
            if(depth == 0 && then_pos != end && else_pos == end) else_pos = i;
        }else if(text == "while"){
            ++depth;
        }else if(text == "done"){
            if(depth > 0) --depth;
        }
    }

    if((then_pos == end) || (fi_pos == end) || (then_pos < start)){
        return {make_error("Malformed if/then/fi block"), end};
    }

    const auto condition = run_tokens(tokens, start, then_pos, {}, execute);
    const bool take_then = execute && (condition.return_code == 0);
    CommandResult result = condition;

    if(else_pos == end){
        const auto body = run_tokens(tokens, then_pos + 1, fi_pos, {}, take_then);
        if(take_then){
            result.return_code = body.return_code;
            result.output.insert(result.output.end(), body.output.begin(), body.output.end());
        }
    }else{
        const auto then_body = run_tokens(tokens, then_pos + 1, else_pos, {}, take_then);
        const bool take_else = execute && !take_then;
        const auto else_body = run_tokens(tokens, else_pos + 1, fi_pos, {}, take_else);
        if(take_then){
            result.return_code = then_body.return_code;
            result.output.insert(result.output.end(), then_body.output.begin(), then_body.output.end());
        }else if(take_else){
            result.return_code = else_body.return_code;
            result.output.insert(result.output.end(), else_body.output.begin(), else_body.output.end());
        }
    }

    return {result, fi_pos + 1};
}

std::pair<Bash::CommandResult, size_t> Bash::run_while_command(const std::vector<Token> &tokens,
                                                               size_t pos,
                                                               size_t end,
                                                               bool execute){
    const auto start = pos + 1;
    size_t depth = 0;
    size_t do_pos = end;
    size_t done_pos = end;

    for(size_t i = start; i < end; ++i){
        const auto &text = tokens[i].text;
        if((text == "if") || (text == "while")){
            ++depth;
        }else if((text == "fi") || (text == "done")){
            if(depth == 0 && text == "done"){
                done_pos = i;
                break;
            }
            if(depth > 0) --depth;
        }else if((text == "do") && (depth == 0) && (do_pos == end)){
            do_pos = i;
        }
    }

    if((do_pos == end) || (done_pos == end) || (do_pos < start)){
        return {make_error("Malformed while/do/done block"), end};
    }

    CommandResult aggregate;
    size_t guard = 0;
    while(execute && !exit_requested_){
        ++guard;
        if(guard > 10000U){
            return {make_error("Loop iteration guard exceeded"), done_pos + 1};
        }

        const auto condition = run_tokens(tokens, start, do_pos, {}, true);
        aggregate.output.insert(aggregate.output.end(), condition.output.begin(), condition.output.end());
        if(condition.return_code != 0){
            aggregate.return_code = 0;
            break;
        }

        const auto body = run_tokens(tokens, do_pos + 1, done_pos, {}, true);
        aggregate.return_code = body.return_code;
        aggregate.output.insert(aggregate.output.end(), body.output.begin(), body.output.end());
        if(exit_requested_) break;
    }

    if(!execute){
        run_tokens(tokens, start, do_pos, {}, false);
        run_tokens(tokens, do_pos + 1, done_pos, {}, false);
    }

    return {aggregate, done_pos + 1};
}

std::pair<Bash::CommandResult, size_t> Bash::run_simple_command(const std::vector<Token> &tokens,
                                                                size_t pos,
                                                                size_t end,
                                                                bool execute,
                                                                const std::vector<std::string> &input){
    size_t next = pos;
    while(next < end){
        const auto &text = tokens[next].text;
        if((text == ";") || (text == "&&") || (text == "||") || (text == "|")) break;
        if((text == "then") || (text == "else") || (text == "fi") || (text == "do") || (text == "done")) break;
        ++next;
    }

    std::vector<Assignment> assignments;
    std::vector<std::string> words;
    std::vector<Redirection> redirections;

    for(size_t i = pos; i < next; ++i){
        const auto &text = tokens[i].text;
        if((text == "<") || (text == ">") || (text == ">>")){
            if((i + 1) >= next){
                return {make_error("Missing redirection target"), next};
            }
            Redirection redir{ (text == "<") ? Redirection::Type::input
                             : (text == ">") ? Redirection::Type::output
                                                : Redirection::Type::append,
                             tokens[i + 1].text };
            redirections.push_back(redir);
            ++i;
            continue;
        }

        const auto equal_pos = text.find('=');
        if(words.empty() && (equal_pos != std::string::npos) && (equal_pos > 0)){
            const auto name = text.substr(0, equal_pos);
            if(is_identifier(name)){
                Assignment assignment;
                assignment.name = name;
                const auto value = text.substr(equal_pos + 1);
                if(!value.empty() && (value.front() == '(')){
                    assignment.is_array = true;
                    std::string current_value = value.substr(1);
                    bool closed = false;
                    if(!current_value.empty() && (current_value.back() == ')')){
                        current_value.pop_back();
                        closed = true;
                    }
                    if(!current_value.empty()) assignment.values.push_back(current_value);
                    while(!closed && ((i + 1) < next)){
                        ++i;
                        current_value = tokens[i].text;
                        if(!current_value.empty() && (current_value.back() == ')')){
                            current_value.pop_back();
                            closed = true;
                        }
                        if(!current_value.empty()) assignment.values.push_back(current_value);
                    }
                }else{
                    assignment.values.push_back(value);
                }
                assignments.push_back(std::move(assignment));
                continue;
            }
        }

        words.push_back(text);
    }

    if(!execute) return {CommandResult{}, next};

    for(const auto &assignment : assignments){
        if(assignment.is_array){
            assign_array(assignment.name, expand_words(assignment.values));
        }else{
            assign_scalar(assignment.name, expand_scalar(assignment.values.empty() ? "" : assignment.values.front()));
        }
    }

    std::vector<std::string> stdin_lines = input;
    for(const auto &redirection : redirections){
        if(redirection.type == Redirection::Type::input){
            stdin_lines = read_file_lines(resolve_path(expand_scalar(redirection.target)));
        }
    }

    if(words.empty()) return {CommandResult{}, next};

    auto expanded_words = expand_words(words);
    if(expanded_words.empty()) return {CommandResult{}, next};

    const auto command = expanded_words.front();
    expanded_words.erase(expanded_words.begin());

    CommandResult result;
    if(const auto it = builtins_.find(command); it != builtins_.end()){
        result = it->second(expanded_words, stdin_lines);
    }else{
        result = make_error("Unsupported command '" + command + "'");
    }

    for(const auto &redirection : redirections){
        if((redirection.type == Redirection::Type::output) || (redirection.type == Redirection::Type::append)){
            const auto target = resolve_path(expand_scalar(redirection.target));
            const auto rc = write_file_lines(target, result.output, redirection.type == Redirection::Type::append);
            if(rc != 0) return {make_error("Unable to write redirected output"), next};
            result.output.clear();
        }
    }

    last_status_ = result.return_code;
    return {result, next};
}

Bash::ExpandedWord Bash::expand_word(const std::string &raw){
    ExpandedWord out;
    out.words = {""};

    const auto append_literal = [&](const std::string &text){
        for(auto &word : out.words) word += text;
    };
    const auto append_values = [&](const std::vector<std::string> &values){
        std::vector<std::string> next_words;
        for(const auto &prefix : out.words){
            if(values.empty()){
                next_words.push_back(prefix);
            }else{
                for(const auto &value : values) next_words.push_back(prefix + value);
            }
        }
        out.words = std::move(next_words);
    };

    bool in_single = false;
    bool in_double = false;
    for(size_t i = 0; i < raw.size(); ++i){
        const char c = raw[i];
        if(!in_double && (c == '\'')){
            in_single = !in_single;
            out.quoted = true;
            continue;
        }
        if(!in_single && (c == '"')){
            in_double = !in_double;
            out.quoted = true;
            continue;
        }
        if(!in_single && (c == '\\')){
            if((i + 1) < raw.size()){
                append_literal(std::string(1, raw[++i]));
                continue;
            }
        }
        if(!in_single && (c == '$')){
            if(((i + 2) < raw.size()) && (raw[i + 1] == '(') && (raw[i + 2] == '(')){
                size_t depth = 1;
                size_t j = i + 3;
                for(; j < raw.size(); ++j){
                    if(raw[j] == '(') ++depth;
                    else if((raw[j] == ')') && (--depth == 0)) break;
                }
                if((j + 1) < raw.size() && (raw[j + 1] == ')')){
                    append_literal(evaluate_arithmetic_expression(raw.substr(i + 3, j - (i + 3))));
                    i = j + 1;
                    continue;
                }
            }
            if((i + 1) < raw.size() && (raw[i + 1] == '(')){
                size_t depth = 1;
                bool sub_single = false;
                bool sub_double = false;
                size_t j = i + 2;
                for(; j < raw.size(); ++j){
                    const char sc = raw[j];
                    if(!sub_double && (sc == '\'')) sub_single = !sub_single;
                    else if(!sub_single && (sc == '"')) sub_double = !sub_double;
                    else if(!sub_single && !sub_double){
                        if(sc == '(') ++depth;
                        else if((sc == ')') && (--depth == 0)) break;
                    }
                }
                if(j < raw.size()){
                    append_literal(evaluate_command_substitution(raw.substr(i + 2, j - (i + 2))));
                    i = j;
                    continue;
                }
            }
            if((i + 1) < raw.size() && (raw[i + 1] == '{')){
                size_t depth = 1;
                size_t j = i + 2;
                for(; j < raw.size(); ++j){
                    if(raw[j] == '{') ++depth;
                    else if((raw[j] == '}') && (--depth == 0)) break;
                }
                if(j < raw.size()){
                    const auto expr = raw.substr(i + 2, j - (i + 2));
                    if((expr.size() >= 2) && ((expr.substr(expr.size() - 2) == "[@]") || (expr.substr(expr.size() - 2) == "[*]"))){
                        const auto array_name = expr.substr(0, expr.size() - 3);
                        append_values(lookup_array(array_name).value_or(std::vector<std::string>{}));
                    }else{
                        append_literal(evaluate_parameter_expansion(expr));
                    }
                    i = j;
                    continue;
                }
            }
            if((i + 1) < raw.size() && (raw[i + 1] == '?')){
                append_literal(std::to_string(last_status_));
                ++i;
                continue;
            }
            size_t j = i + 1;
            while((j < raw.size()) && (std::isalnum(static_cast<unsigned char>(raw[j])) || (raw[j] == '_'))) ++j;
            if(j > (i + 1)){
                append_literal(lookup_scalar(raw.substr(i + 1, j - (i + 1))).value_or(""));
                i = j - 1;
                continue;
            }
        }
        append_literal(std::string(1, c));
    }

    if(!out.quoted){
        std::vector<std::string> globbed;
        for(const auto &word : out.words){
            const auto matches = expand_glob(word);
            if(matches.empty()) globbed.push_back(word);
            else globbed.insert(globbed.end(), matches.begin(), matches.end());
        }
        out.words = std::move(globbed);
    }

    return out;
}

std::vector<std::string> Bash::expand_words(const std::vector<std::string> &raw_words){
    std::vector<std::string> out;
    for(const auto &raw : raw_words){
        auto expanded = expand_word(raw);
        out.insert(out.end(), expanded.words.begin(), expanded.words.end());
    }
    return out;
}

std::string Bash::expand_scalar(const std::string &raw){
    const auto expanded = expand_word(raw);
    return join_strings(expanded.words, " ");
}

std::string Bash::evaluate_parameter_expansion(const std::string &expr){
    if(expr == "?") return std::to_string(last_status_);
    if(expr == "PWD") return cwd_.string();

    if(starts_with(expr, "#")){
        const auto key = expr.substr(1);
        if((key.size() >= 3) && ((key.substr(key.size() - 3) == "[@]") || (key.substr(key.size() - 3) == "[*]"))){
            return std::to_string(lookup_array(key.substr(0, key.size() - 3)).value_or(std::vector<std::string>{}).size());
        }
        return std::to_string(lookup_scalar(key).value_or("").size());
    }

    const auto lb = expr.find('[');
    const auto rb = expr.find(']');
    if((lb != std::string::npos) && (rb != std::string::npos) && (lb < rb)){
        const auto name = expr.substr(0, lb);
        const auto index_text = trim_copy(expr.substr(lb + 1, rb - lb - 1));
        const auto array = lookup_array(name).value_or(std::vector<std::string>{});
        if((index_text == "@") || (index_text == "*")) return join_strings(array, " ");
        const auto index = static_cast<size_t>(std::max<std::int64_t>(0, to_integer_or_zero(index_text)));
        if(index < array.size()) return array[index];
        return "";
    }

    return lookup_scalar(expr).value_or("");
}

std::string Bash::evaluate_command_substitution(const std::string &expr){
    const auto result = process(expr);
    last_status_ = result.return_code;
    return join_strings(result.output, "\n");
}

std::string Bash::evaluate_arithmetic_expression(const std::string &expr){
    try{
        ArithmeticParser parser(expr, [this](const std::string &name){
            return to_integer_or_zero(lookup_scalar(name).value_or("0"));
        });
        return std::to_string(parser.parse());
    }catch(...){
        return "0";
    }
}

std::vector<std::string> Bash::expand_glob(const std::string &pattern) const{
    if((pattern.find('*') == std::string::npos) && (pattern.find('?') == std::string::npos)) return {};

    const auto resolved = resolve_path(pattern);
    const auto parent = resolved.has_parent_path() ? resolved.parent_path() : cwd_;
    const auto filename_pattern = resolved.filename().string();
    std::vector<std::string> matches;
    std::error_code ec;
    if(!std::filesystem::exists(parent, ec)) return {};

    const std::regex re(wildcard_to_regex(filename_pattern));
    for(const auto &entry : std::filesystem::directory_iterator(parent, ec)){
        if(ec) break;
        if(std::regex_match(entry.path().filename().string(), re)){
            if(pattern.find('/') != std::string::npos) matches.push_back(entry.path().lexically_normal().string());
            else matches.push_back(entry.path().filename().string());
        }
    }
    std::sort(matches.begin(), matches.end());
    return matches;
}

std::optional<std::string> Bash::lookup_scalar(const std::string &name) const{
    if(name == "PWD") return cwd_.string();
    if(name == "OLDPWD"){
        const auto it = environment_.find("OLDPWD");
        if(it != environment_.end()) return it->second;
    }
    if(const auto it = variables_.find(name); it != variables_.end()) return it->second;
    if(const auto it = environment_.find(name); it != environment_.end()) return it->second;
    if(const auto it = arrays_.find(name); it != arrays_.end()) return join_strings(it->second, " ");
    return std::nullopt;
}

std::optional<std::vector<std::string>> Bash::lookup_array(const std::string &name) const{
    if(const auto it = arrays_.find(name); it != arrays_.end()) return it->second;
    return std::nullopt;
}

void Bash::assign_scalar(const std::string &name, const std::string &value){
    variables_[name] = value;
    if(name == "PWD"){
        cwd_ = std::filesystem::path(value).lexically_normal();
        update_pwd();
    }
}

void Bash::assign_array(const std::string &name, const std::vector<std::string> &values){
    arrays_[name] = values;
}

void Bash::update_pwd(){
    environment_["PWD"] = cwd_.string();
}

void Bash::update_oldpwd(const std::filesystem::path &old_cwd){
    environment_["OLDPWD"] = old_cwd.string();
}

std::filesystem::path Bash::resolve_path(const std::string &path) const{
    const auto expanded = std::filesystem::path(path);
    if(expanded.is_absolute()) return expanded.lexically_normal();
    return (cwd_ / expanded).lexically_normal();
}

bool Bash::is_identifier(const std::string &name) const{
    if(name.empty()) return false;
    if(!(std::isalpha(static_cast<unsigned char>(name.front())) || (name.front() == '_'))) return false;
    return std::all_of(std::next(name.begin()), name.end(), [](unsigned char c){
        return std::isalnum(c) || (c == '_');
    });
}

std::vector<std::string> Bash::read_file_lines(const std::filesystem::path &path) const{
    std::ifstream is(path);
    if(!is) throw std::runtime_error("Unable to open file '" + path.string() + "'");
    return split_lines(is);
}

int Bash::write_file_lines(const std::filesystem::path &path,
                           const std::vector<std::string> &lines,
                           bool append) const{
    std::error_code ec;
    if(path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream os(path, append ? (std::ios::out | std::ios::app) : std::ios::out);
    if(!os) return 1;
    for(size_t i = 0; i < lines.size(); ++i){
        os << lines[i];
        if((i + 1) < lines.size()) os << '\n';
    }
    return os ? 0 : 1;
}

bool Bash::is_control_operator(const std::string &token){
    return (token == ";") || (token == "&&") || (token == "||") || (token == "|")
        || (token == "then") || (token == "else") || (token == "fi")
        || (token == "do") || (token == "done");
}

bool Bash::is_terminator(const std::string &token,
                         const std::vector<std::string> &terminators){
    return std::find(terminators.begin(), terminators.end(), token) != terminators.end();
}

size_t Bash::skip_separators(const std::vector<Token> &tokens, size_t pos, size_t end){
    while((pos < end) && (tokens[pos].text == ";")) ++pos;
    return pos;
}

Bash::CommandResult Bash::make_error(const std::string &message) const{
    return CommandResult{1, {message}};
}

Bash::CommandResult Bash::builtin_echo(const std::vector<std::string> &args,
                                       const std::vector<std::string> &){
    bool omit_newline = false;
    size_t start = 0;
    if(!args.empty() && (args.front() == "-n")){
        omit_newline = true;
        start = 1;
    }
    if(omit_newline && (start >= args.size())) return {0, {}};
    std::vector<std::string> out;
    out.emplace_back(join_strings(std::vector<std::string>(args.begin() + static_cast<long>(start), args.end()), " "));
    return {0, out};
}

Bash::CommandResult Bash::builtin_pwd(const std::vector<std::string> &,
                                      const std::vector<std::string> &){
    return {0, {cwd_.string()}};
}

Bash::CommandResult Bash::builtin_cd(const std::vector<std::string> &args,
                                     const std::vector<std::string> &){
    std::string target;
    if(args.empty()) target = environment_.count("HOME") ? environment_.at("HOME") : cwd_.string();
    else if(args.front() == "-") target = environment_.count("OLDPWD") ? environment_.at("OLDPWD") : cwd_.string();
    else target = args.front();

    const auto new_cwd = resolve_path(target);
    std::error_code ec;
    if(!std::filesystem::exists(new_cwd, ec) || !std::filesystem::is_directory(new_cwd, ec)){
        return make_error("cd: no such directory '" + target + "'");
    }

    const auto old_cwd = cwd_;
    cwd_ = std::filesystem::weakly_canonical(new_cwd, ec);
    if(ec) cwd_ = new_cwd.lexically_normal();
    update_oldpwd(old_cwd);
    update_pwd();
    return {0, {}};
}

Bash::CommandResult Bash::builtin_ls(const std::vector<std::string> &args,
                                     const std::vector<std::string> &){
    bool show_all = false;
    bool show_dir = false;
    std::vector<std::string> paths;
    for(const auto &arg : args){
        if(arg == "-a") show_all = true;
        else if(arg == "-d") show_dir = true;
        else if(arg == "-1"){}
        else paths.push_back(arg);
    }
    if(paths.empty()) paths.emplace_back(".");

    std::vector<std::string> out;
    for(const auto &raw_path : paths){
        const auto path = resolve_path(raw_path);
        std::error_code ec;
        if(show_dir || !std::filesystem::is_directory(path, ec)){
            const auto raw = std::filesystem::path(raw_path);
            out.emplace_back((raw_path == ".") ? path.filename().string()
                                               : (raw.has_parent_path() ? path.lexically_normal().string()
                                                                        : path.filename().string()));
            continue;
        }
        std::vector<std::string> names;
        for(const auto &entry : std::filesystem::directory_iterator(path, ec)){
            if(ec) return make_error("ls: unable to enumerate '" + raw_path + "'");
            if(!show_all && path_is_hidden(entry.path())) continue;
            names.emplace_back(entry.path().filename().string());
        }
        std::sort(names.begin(), names.end());
        out.insert(out.end(), names.begin(), names.end());
    }
    return {0, out};
}

Bash::CommandResult Bash::builtin_export(const std::vector<std::string> &args,
                                         const std::vector<std::string> &){
    if(args.empty()) return builtin_env(args, {});
    for(const auto &arg : args){
        const auto eq = arg.find('=');
        if((eq != std::string::npos) && (eq > 0)){
            const auto name = arg.substr(0, eq);
            const auto value = arg.substr(eq + 1);
            if(!is_identifier(name)) return make_error("export: invalid identifier '" + name + "'");
            assign_scalar(name, value);
            environment_[name] = value;
        }else if(is_identifier(arg)){
            environment_[arg] = lookup_scalar(arg).value_or("");
        }else{
            return make_error("export: invalid identifier '" + arg + "'");
        }
    }
    return {0, {}};
}

Bash::CommandResult Bash::builtin_setenv(const std::vector<std::string> &args,
                                         const std::vector<std::string> &){
    if(args.size() == 2U){
        if(!is_identifier(args[0])) return make_error("setenv: invalid identifier");
        environment_[args[0]] = args[1];
        return {0, {}};
    }
    if((args.size() == 1U) && (args.front().find('=') != std::string::npos)) return builtin_export(args, {});
    return make_error("setenv: expected NAME VALUE");
}

Bash::CommandResult Bash::builtin_unset(const std::vector<std::string> &args,
                                        const std::vector<std::string> &){
    for(const auto &arg : args){
        variables_.erase(arg);
        arrays_.erase(arg);
        environment_.erase(arg);
    }
    update_pwd();
    return {0, {}};
}

Bash::CommandResult Bash::builtin_env(const std::vector<std::string> &,
                                      const std::vector<std::string> &){
    std::vector<std::string> out;
    for(const auto &[name, value] : environment_) out.emplace_back(name + "=" + value);
    return {0, out};
}

Bash::CommandResult Bash::builtin_set(const std::vector<std::string> &,
                                      const std::vector<std::string> &){
    std::vector<std::string> out;
    for(const auto &[name, value] : variables_) out.emplace_back(name + "=" + value);
    for(const auto &[name, values] : arrays_) out.emplace_back(name + "=(" + join_strings(values, " ") + ")");
    return {0, out};
}

Bash::CommandResult Bash::builtin_true(const std::vector<std::string> &,
                                       const std::vector<std::string> &){
    return {0, {}};
}

Bash::CommandResult Bash::builtin_false(const std::vector<std::string> &,
                                        const std::vector<std::string> &){
    return {1, {}};
}

Bash::CommandResult Bash::builtin_test(const std::vector<std::string> &args,
                                       const std::vector<std::string> &){
    auto tokens = args;
    if(!tokens.empty() && (tokens.front() == "[")) tokens.erase(tokens.begin());
    if(!tokens.empty() && (tokens.back() == "]")) tokens.pop_back();

    if(tokens.empty()) return {1, {}};
    if(tokens.size() == 1U) return {tokens.front().empty() ? 1 : 0, {}};
    if(tokens.size() == 2U){
        const auto &op = tokens[0];
        const auto &rhs = tokens[1];
        if(op == "-e") return {std::filesystem::exists(resolve_path(rhs)) ? 0 : 1, {}};
        if(op == "-f") return {std::filesystem::is_regular_file(resolve_path(rhs)) ? 0 : 1, {}};
        if(op == "-d") return {std::filesystem::is_directory(resolve_path(rhs)) ? 0 : 1, {}};
        if(op == "-n") return {!rhs.empty() ? 0 : 1, {}};
        if(op == "-z") return {rhs.empty() ? 0 : 1, {}};
    }
    if(tokens.size() == 3U){
        const auto &lhs = tokens[0];
        const auto &op = tokens[1];
        const auto &rhs = tokens[2];
        if((op == "=") || (op == "==")) return {(lhs == rhs) ? 0 : 1, {}};
        if(op == "!=") return {(lhs != rhs) ? 0 : 1, {}};
        const auto lhs_i = to_integer_or_zero(lhs);
        const auto rhs_i = to_integer_or_zero(rhs);
        if(op == "-eq") return {(lhs_i == rhs_i) ? 0 : 1, {}};
        if(op == "-ne") return {(lhs_i != rhs_i) ? 0 : 1, {}};
        if(op == "-gt") return {(lhs_i > rhs_i) ? 0 : 1, {}};
        if(op == "-ge") return {(lhs_i >= rhs_i) ? 0 : 1, {}};
        if(op == "-lt") return {(lhs_i < rhs_i) ? 0 : 1, {}};
        if(op == "-le") return {(lhs_i <= rhs_i) ? 0 : 1, {}};
    }
    return make_error("test: unsupported expression");
}

Bash::CommandResult Bash::builtin_mkdir(const std::vector<std::string> &args,
                                        const std::vector<std::string> &){
    bool parents = false;
    std::vector<std::string> paths;
    for(const auto &arg : args){
        if(arg == "-p") parents = true;
        else paths.push_back(arg);
    }
    if(paths.empty()) return make_error("mkdir: missing operand");
    std::error_code ec;
    for(const auto &raw_path : paths){
        const auto path = resolve_path(raw_path);
        if(parents) std::filesystem::create_directories(path, ec);
        else std::filesystem::create_directory(path, ec);
        if(ec) return make_error("mkdir: unable to create '" + raw_path + "'");
    }
    return {0, {}};
}

Bash::CommandResult Bash::builtin_rm(const std::vector<std::string> &args,
                                     const std::vector<std::string> &){
    bool recursive = false;
    bool force = false;
    std::vector<std::string> paths;
    for(const auto &arg : args){
        if((arg == "-r") || (arg == "-R")) recursive = true;
        else if(arg == "-f") force = true;
        else if((arg == "-rf") || (arg == "-fr")){
            recursive = true;
            force = true;
        }else paths.push_back(arg);
    }
    if(paths.empty()) return make_error("rm: missing operand");
    std::error_code ec;
    for(const auto &raw_path : paths){
        const auto path = resolve_path(raw_path);
        if(!std::filesystem::exists(path, ec)){
            if(force) continue;
            return make_error("rm: cannot remove '" + raw_path + "'");
        }
        if(std::filesystem::is_directory(path, ec) && !recursive) return make_error("rm: is a directory '" + raw_path + "'");
        if(recursive) std::filesystem::remove_all(path, ec);
        else std::filesystem::remove(path, ec);
        if(ec && !force) return make_error("rm: unable to remove '" + raw_path + "'");
    }
    return {0, {}};
}

Bash::CommandResult Bash::builtin_touch(const std::vector<std::string> &args,
                                        const std::vector<std::string> &){
    if(args.empty()) return make_error("touch: missing operand");
    for(const auto &raw_path : args){
        const auto path = resolve_path(raw_path);
        std::error_code ec;
        if(path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
        if(!std::filesystem::exists(path, ec)){
            std::ofstream os(path);
            if(!os) return make_error("touch: unable to create '" + raw_path + "'");
        }
        const auto now = std::filesystem::file_time_type::clock::now();
        std::filesystem::last_write_time(path, now, ec);
    }
    return {0, {}};
}

Bash::CommandResult Bash::builtin_cat(const std::vector<std::string> &args,
                                      const std::vector<std::string> &input){
    if(args.empty()) return {0, input};
    std::vector<std::string> out;
    try{
        for(const auto &raw_path : args){
            const auto lines = read_file_lines(resolve_path(raw_path));
            out.insert(out.end(), lines.begin(), lines.end());
        }
    }catch(const std::exception &e){
        return make_error(e.what());
    }
    return {0, out};
}

Bash::CommandResult Bash::builtin_cp(const std::vector<std::string> &args,
                                     const std::vector<std::string> &){
    bool recursive = false;
    std::vector<std::string> paths;
    for(const auto &arg : args){
        if((arg == "-r") || (arg == "-R")) recursive = true;
        else paths.push_back(arg);
    }
    if(paths.size() < 2U) return make_error("cp: expected SOURCE DEST");

    const auto dest = resolve_path(paths.back());
    std::error_code ec;
    const bool dest_is_dir = std::filesystem::exists(dest, ec) && std::filesystem::is_directory(dest, ec);
    for(size_t i = 0; i + 1 < paths.size(); ++i){
        const auto source = resolve_path(paths[i]);
        auto target = dest;
        if(dest_is_dir) target /= source.filename();
        if(std::filesystem::is_directory(source, ec)){
            if(!recursive) return make_error("cp: omitting directory '" + paths[i] + "'");
            std::filesystem::copy(source,
                                  target,
                                  std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
                                  ec);
        }else{
            if(target.has_parent_path()) std::filesystem::create_directories(target.parent_path(), ec);
            std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, ec);
        }
        if(ec) return make_error("cp: unable to copy '" + paths[i] + "'");
    }
    return {0, {}};
}

Bash::CommandResult Bash::builtin_mv(const std::vector<std::string> &args,
                                     const std::vector<std::string> &){
    if(args.size() != 2U) return make_error("mv: expected SOURCE DEST");
    std::error_code ec;
    std::filesystem::rename(resolve_path(args[0]), resolve_path(args[1]), ec);
    if(ec) return make_error("mv: unable to move '" + args[0] + "'");
    return {0, {}};
}

Bash::CommandResult Bash::builtin_grep(const std::vector<std::string> &args,
                                       const std::vector<std::string> &input){
    bool ignore_case = false;
    bool invert = false;
    bool quiet = false;
    bool number = false;
    std::vector<std::string> rest;
    for(const auto &arg : args){
        if(arg == "-i") ignore_case = true;
        else if(arg == "-v") invert = true;
        else if(arg == "-q") quiet = true;
        else if(arg == "-n") number = true;
        else rest.push_back(arg);
    }
    if(rest.empty()) return make_error("grep: missing pattern");
    const auto pattern = rest.front();
    rest.erase(rest.begin());

    std::regex_constants::syntax_option_type options = std::regex::ECMAScript;
    if(ignore_case) options |= std::regex::icase;

    std::regex re;
    try{
        re = std::regex(pattern, options);
    }catch(...){
        return make_error("grep: invalid pattern");
    }

    std::vector<std::string> haystack = input;
    if(!rest.empty()){
        haystack.clear();
        try{
            for(const auto &raw_path : rest){
                const auto lines = read_file_lines(resolve_path(raw_path));
                haystack.insert(haystack.end(), lines.begin(), lines.end());
            }
        }catch(const std::exception &e){
            return make_error(e.what());
        }
    }

    std::vector<std::string> out;
    for(size_t i = 0; i < haystack.size(); ++i){
        const bool matched = std::regex_search(haystack[i], re);
        if(matched != invert){
            if(quiet) return {0, {}};
            out.push_back(number ? (std::to_string(i + 1) + ":" + haystack[i]) : haystack[i]);
        }
    }
    return {out.empty() ? 1 : 0, out};
}

Bash::CommandResult Bash::builtin_find(const std::vector<std::string> &args,
                                       const std::vector<std::string> &){
    std::vector<std::string> roots;
    std::optional<std::regex> name_re;
    char type_filter = '\0';
    int max_depth = -1;
    int min_depth = 0;

    for(size_t i = 0; i < args.size(); ++i){
        if(args[i] == "-name"){
            if((i + 1) >= args.size()) return make_error("find: missing pattern after -name");
            name_re = std::regex(wildcard_to_regex(args[++i]));
        }else if(args[i] == "-type"){
            if((i + 1) >= args.size()) return make_error("find: missing type after -type");
            type_filter = args[++i].empty() ? '\0' : args[i].front();
        }else if(args[i] == "-maxdepth"){
            if((i + 1) >= args.size()) return make_error("find: missing depth after -maxdepth");
            max_depth = static_cast<int>(to_integer_or_zero(args[++i]));
        }else if(args[i] == "-mindepth"){
            if((i + 1) >= args.size()) return make_error("find: missing depth after -mindepth");
            min_depth = static_cast<int>(to_integer_or_zero(args[++i]));
        }else{
            roots.push_back(args[i]);
        }
    }
    if(roots.empty()) roots.emplace_back(".");

    std::vector<std::string> out;
    std::error_code ec;
    for(const auto &root_arg : roots){
        const auto root = resolve_path(root_arg);
        if(!std::filesystem::exists(root, ec)) return make_error("find: unable to access '" + root_arg + "'");
        out.push_back(root.lexically_normal().string());
        std::filesystem::recursive_directory_iterator it(root, ec), end_it;
        for(; !ec && (it != end_it); it.increment(ec)){
            const auto depth = static_cast<int>(it.depth()) + 1;
            if((max_depth >= 0) && (depth > max_depth)){
                it.disable_recursion_pending();
                continue;
            }
            if(depth < min_depth) continue;
            const auto &path = it->path();
            if(name_re && !std::regex_match(path.filename().string(), *name_re)) continue;
            if((type_filter == 'f') && !std::filesystem::is_regular_file(path, ec)) continue;
            if((type_filter == 'd') && !std::filesystem::is_directory(path, ec)) continue;
            out.push_back(path.lexically_normal().string());
        }
        if(ec) return make_error("find: unable to enumerate '" + root_arg + "'");
    }
    return {0, out};
}

Bash::CommandResult Bash::builtin_basename(const std::vector<std::string> &args,
                                           const std::vector<std::string> &){
    if(args.empty()) return make_error("basename: missing operand");
    return {0, {std::filesystem::path(args.front()).filename().string()}};
}

Bash::CommandResult Bash::builtin_dirname(const std::vector<std::string> &args,
                                          const std::vector<std::string> &){
    if(args.empty()) return make_error("dirname: missing operand");
    const auto path = std::filesystem::path(args.front());
    return {0, {path.has_parent_path() ? path.parent_path().string() : std::string(".")}};
}

Bash::CommandResult Bash::builtin_printf(const std::vector<std::string> &args,
                                         const std::vector<std::string> &){
    if(args.empty()) return {0, {}};
    std::string format = args.front();
    std::vector<std::string> values(args.begin() + 1, args.end());
    std::ostringstream ss;
    size_t value_index = 0;
    for(size_t i = 0; i < format.size(); ++i){
        if((format[i] == '%') && ((i + 1) < format.size()) && (format[i + 1] == 's')){
            ss << ((value_index < values.size()) ? values[value_index++] : std::string());
            ++i;
        }else if((format[i] == '%') && ((i + 1) < format.size()) && (format[i + 1] == 'd')){
            ss << to_integer_or_zero((value_index < values.size()) ? values[value_index++] : std::string("0"));
            ++i;
        }else if((format[i] == '\\') && ((i + 1) < format.size())){
            ++i;
            if(format[i] == 'n') ss << '\n';
            else if(format[i] == 't') ss << '\t';
            else ss << format[i];
        }else{
            ss << format[i];
        }
    }
    return {0, split_lines(ss.str())};
}

Bash::CommandResult Bash::builtin_exit(const std::vector<std::string> &args,
                                       const std::vector<std::string> &){
    exit_requested_ = true;
    const int code = args.empty() ? last_status_ : static_cast<int>(to_integer_or_zero(args.front()));
    return {code, {}};
}
