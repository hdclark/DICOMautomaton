//Session.h.

#pragma once

#include <filesystem>
#include <istream>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "Script_Loader.h"
#include "Structs.h"

namespace dcma {

class Session {
    public:
        using metadata_t = std::map<std::string, std::string>;
        using operation_list_t = std::list<OperationArgPkg>;
        using path_list_t = std::list<std::filesystem::path>;
        using feedback_list_t = std::list<script_feedback_t>;

        Session() = default;
        Session(const Session &) = delete;
        Session & operator=(const Session &) = delete;

        Drover & data();
        const Drover & data() const;

        metadata_t & metadata();
        const metadata_t & metadata() const;

        std::string & lexicon_filename();
        const std::string & lexicon_filename() const;
        void prepare_lexicon();

        operation_list_t & pending_operations();
        const operation_list_t & pending_operations() const;
        bool has_pending_operations() const;

        // Operations discovered while loading are staged ahead of existing operations.
        bool load(path_list_t &paths);

        bool run();
        bool run(OperationArgPkg operation);
        bool run(operation_list_t operations);

        bool run_script(std::istream &is, feedback_list_t &feedback);
        bool run_script(const std::string &script, feedback_list_t &feedback);

        std::vector<OperationDoc> operation_docs() const;
        const std::string & last_error() const;

    private:
        Drover data_;
        metadata_t metadata_;
        std::string lexicon_filename_;
        operation_list_t loaded_operations_;
        operation_list_t pending_operations_;
        std::string last_error_;
};

} // namespace dcma
