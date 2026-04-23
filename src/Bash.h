//Bash.h - A part of DICOMautomaton 2026. Written by OpenAI.

#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

class Bash {
public:
    struct Result {
        int return_code = 0;
        std::vector<std::string> output;
        std::map<std::string, std::string> environment;
    };

    using Environment = std::map<std::string, std::string>;

    Bash();
    explicit Bash(std::filesystem::path cwd, Environment env = {});

    Result process(const std::string &script);
    Result process(const std::vector<std::string> &script_lines);

    const std::filesystem::path& current_working_directory() const;
    void set_current_working_directory(const std::filesystem::path &cwd);

    std::optional<std::string> get_environment_variable(const std::string &name) const;
    void set_environment_variable(const std::string &name, const std::string &value);
    void unset_environment_variable(const std::string &name);

private:
    struct CommandResult {
        int return_code = 0;
        std::vector<std::string> output;
    };

    using Builtin = std::function<CommandResult(const std::vector<std::string>&,
                                                const std::vector<std::string>&)>;

    struct Token {
        std::string text;
    };

    struct Assignment {
        std::string name;
        bool is_array = false;
        std::vector<std::string> values;
    };

    struct Redirection {
        enum class Type {
            input,
            output,
            append,
        };

        Type type;
        std::string target;
    };

    struct ExpandedWord {
        std::vector<std::string> words;
        bool quoted = false;
    };

    Environment environment_;
    std::map<std::string, std::string> variables_;
    std::map<std::string, std::vector<std::string>> arrays_;
    std::filesystem::path cwd_;
    std::map<std::string, Builtin> builtins_;
    int last_status_ = 0;
    bool exit_requested_ = false;

    void initialize_environment();
    void register_builtins();

    std::vector<Token> tokenize(const std::string &script) const;
    CommandResult run_tokens(const std::vector<Token> &tokens,
                             size_t begin,
                             size_t end,
                             const std::vector<std::string> &terminators,
                             bool execute);
    std::pair<CommandResult, size_t> run_and_or(const std::vector<Token> &tokens,
                                                size_t pos,
                                                size_t end,
                                                const std::vector<std::string> &terminators,
                                                bool execute);
    std::pair<CommandResult, size_t> run_pipeline(const std::vector<Token> &tokens,
                                                  size_t pos,
                                                  size_t end,
                                                  const std::vector<std::string> &terminators,
                                                  bool execute);
    std::pair<CommandResult, size_t> run_command(const std::vector<Token> &tokens,
                                                 size_t pos,
                                                 size_t end,
                                                 const std::vector<std::string> &terminators,
                                                 bool execute,
                                                 const std::vector<std::string> &input);
    std::pair<CommandResult, size_t> run_if_command(const std::vector<Token> &tokens,
                                                    size_t pos,
                                                    size_t end,
                                                    bool execute);
    std::pair<CommandResult, size_t> run_while_command(const std::vector<Token> &tokens,
                                                       size_t pos,
                                                       size_t end,
                                                       bool execute);
    std::pair<CommandResult, size_t> run_simple_command(const std::vector<Token> &tokens,
                                                        size_t pos,
                                                        size_t end,
                                                        bool execute,
                                                        const std::vector<std::string> &input);

    ExpandedWord expand_word(const std::string &raw);
    std::vector<std::string> expand_words(const std::vector<std::string> &raw_words);
    std::string expand_scalar(const std::string &raw);
    std::string evaluate_parameter_expansion(const std::string &expr);
    std::string evaluate_command_substitution(const std::string &expr);
    std::string evaluate_arithmetic_expression(const std::string &expr);
    std::vector<std::string> expand_glob(const std::string &pattern) const;

    std::optional<std::string> lookup_scalar(const std::string &name) const;
    std::optional<std::vector<std::string>> lookup_array(const std::string &name) const;
    void assign_scalar(const std::string &name, const std::string &value);
    void assign_array(const std::string &name, const std::vector<std::string> &values);
    void update_pwd();
    void update_oldpwd(const std::filesystem::path &old_cwd);
    std::filesystem::path resolve_path(const std::string &path) const;
    bool is_identifier(const std::string &name) const;

    std::vector<std::string> read_file_lines(const std::filesystem::path &path) const;
    int write_file_lines(const std::filesystem::path &path,
                         const std::vector<std::string> &lines,
                         bool append) const;

    static bool is_control_operator(const std::string &token);
    static bool is_terminator(const std::string &token,
                              const std::vector<std::string> &terminators);
    static size_t skip_separators(const std::vector<Token> &tokens, size_t pos, size_t end);

    CommandResult make_error(const std::string &message) const;

    CommandResult builtin_echo(const std::vector<std::string> &args,
                               const std::vector<std::string> &input);
    CommandResult builtin_pwd(const std::vector<std::string> &args,
                              const std::vector<std::string> &input);
    CommandResult builtin_cd(const std::vector<std::string> &args,
                             const std::vector<std::string> &input);
    CommandResult builtin_ls(const std::vector<std::string> &args,
                             const std::vector<std::string> &input);
    CommandResult builtin_export(const std::vector<std::string> &args,
                                 const std::vector<std::string> &input);
    CommandResult builtin_setenv(const std::vector<std::string> &args,
                                 const std::vector<std::string> &input);
    CommandResult builtin_unset(const std::vector<std::string> &args,
                                const std::vector<std::string> &input);
    CommandResult builtin_env(const std::vector<std::string> &args,
                              const std::vector<std::string> &input);
    CommandResult builtin_set(const std::vector<std::string> &args,
                              const std::vector<std::string> &input);
    CommandResult builtin_true(const std::vector<std::string> &args,
                               const std::vector<std::string> &input);
    CommandResult builtin_false(const std::vector<std::string> &args,
                                const std::vector<std::string> &input);
    CommandResult builtin_test(const std::vector<std::string> &args,
                               const std::vector<std::string> &input);
    CommandResult builtin_mkdir(const std::vector<std::string> &args,
                                const std::vector<std::string> &input);
    CommandResult builtin_rm(const std::vector<std::string> &args,
                             const std::vector<std::string> &input);
    CommandResult builtin_touch(const std::vector<std::string> &args,
                                const std::vector<std::string> &input);
    CommandResult builtin_cat(const std::vector<std::string> &args,
                              const std::vector<std::string> &input);
    CommandResult builtin_cp(const std::vector<std::string> &args,
                             const std::vector<std::string> &input);
    CommandResult builtin_mv(const std::vector<std::string> &args,
                             const std::vector<std::string> &input);
    CommandResult builtin_grep(const std::vector<std::string> &args,
                               const std::vector<std::string> &input);
    CommandResult builtin_find(const std::vector<std::string> &args,
                               const std::vector<std::string> &input);
    CommandResult builtin_basename(const std::vector<std::string> &args,
                                   const std::vector<std::string> &input);
    CommandResult builtin_dirname(const std::vector<std::string> &args,
                                  const std::vector<std::string> &input);
    CommandResult builtin_printf(const std::vector<std::string> &args,
                                 const std::vector<std::string> &input);
    CommandResult builtin_exit(const std::vector<std::string> &args,
                               const std::vector<std::string> &input);
};
