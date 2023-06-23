//Script_Loader.h.

#pragma once

#include <istream>
#include <ostream>
#include <list>
#include <set>
#include <filesystem>
#include <cstdint>

#include "Structs.h"

enum class script_feedback_severity_t {
    debug,
    info,
    warn,
    err,
};

struct script_feedback_t {
    script_feedback_severity_t severity = script_feedback_severity_t::info;
    int64_t offset = -1;      // Offset character count from beginning of stream.
    int64_t line = -1;        // Line number.
    int64_t line_offset = -1; // Offset character count from beginning of line.
    std::string message;

    bool operator<(const script_feedback_t &) const;
    bool operator==(const script_feedback_t &) const;
};

// Load a single script.
bool Load_DCMA_Script(std::istream &is,
                      std::list<script_feedback_t> &feedback,
                      std::list<OperationArgPkg> &op_list);

// Attempt to identify and load scripts from a collection of files.
bool Load_From_Script_Files( std::list<OperationArgPkg> &Operations,
                             std::list<std::filesystem::path> &Filenames );


void Print_Feedback(std::ostream &os,
                    const std::list<script_feedback_t> &feedback);


