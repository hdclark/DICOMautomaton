//Script_Loader.h.

#pragma once

#include <istream>
#include <list>

#include "Structs.h"

enum class script_feedback_severity_t {
    debug,
    info,
    warn,
    err,
};

struct script_feedback_t {
    script_feedback_severity_t severity = script_feedback_severity_t::info;
    long int offset = -1;      // Offset character count from beginning of stream.
    long int line = -1;        // Line number.
    long int line_offset = -1; // Offset character count from beginning of line.
    std::string message;
};

bool Load_DCMA_Script(std::istream &is,
                      std::list<script_feedback_t> &feedback,
                      std::list<OperationArgPkg> &op_list);

