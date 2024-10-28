//AdjustVerbosity.cc - A part of DICOMautomaton 2024. Written by hal clark.

#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include <filesystem>

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../Operation_Dispatcher.h"

#include "AdjustVerbosity.h"


OperationDoc OpArgDocAdjustVerbosity() {
    OperationDoc out;
    out.name = "AdjustVerbosity";
    out.aliases.emplace_back("SuppressWarnings");
    out.aliases.emplace_back("AdjustLogs");
    out.aliases.emplace_back("AdjustNotifications");

    out.desc = "This operation is a meta-operation that temporarily adjusts the global log verbosity level."
               " Child operations are executed with the adjust verbosity level, which affects what log"
               " messages, and thus notifications, are suppressed.";

    out.notes.emplace_back(
        "The log is a global object, accessible by all threads in the process. Adjusting the log verbosity"
        " in one thread will also impact all other threads, so it is best to avoid multiple concurrent calls"
        " where logs may be written. (Note: recursively calling this operation, i.e., calling this operation as a"
        " child of itself, is ok.)"
    );

    out.args.emplace_back();
    out.args.back().name = "Verbosity";
    out.args.back().desc = "Controls whether to 'increase' or 'decrease' verbosity.";
    out.args.back().default_val = "decrease";
    out.args.back().expected = true;
    out.args.back().examples = { "decrease", "increase" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "ResetAfterward";
    out.args.back().desc = "Controls whether the original verbosity levels are reset after executing children"
                           " operations.";
    out.args.back().default_val = "true";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}

bool AdjustVerbosity(Drover &DICOM_data,
                     const OperationArgPkg& OptArgs,
                     std::map<std::string, std::string>& InvocationMetadata,
                     const std::string& FilenameLex){
    //-----------------------------------------------------------------------------------------------------------------
    const auto VerbosityStr = OptArgs.getValueStr("Verbosity").value();
    const auto ResetAfterwardStr = OptArgs.getValueStr("ResetAfterward").value();
    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = Compile_Regex("^tr?u?e?$");

    const auto regex_inc = Compile_Regex("^in?c?r?e?a?s?e?");
    const auto regex_dec = Compile_Regex("^de?c?r?e?a?s?e?");

    const bool should_inc = std::regex_match(VerbosityStr, regex_inc);
    const bool should_dec = std::regex_match(VerbosityStr, regex_dec);

    const bool should_reset_afterward = std::regex_match(ResetAfterwardStr, regex_true);

    if( (!should_inc && !should_dec)
    ||  (should_inc != !should_dec) ){
        throw std::invalid_argument("Verbosity parameter argument not understood");
    }

    // Store the existing log threholds.
    const auto log_lvl_callback = ygor::g_logger.get_callback_min_level();
    const auto log_lvl_terminal = ygor::g_logger.get_terminal_min_level();

    // NOTE: TOCTOU problem exists here, but there isn't much we can do about it.

    if(should_dec){
        //ygor::g_logger.decrease_verbosity();
        ygor::g_logger.decrease_terminal_verbosity();
        ygor::g_logger.decrease_callback_verbosity();
    }else if(should_inc){
        //ygor::g_logger.increase_verbosity();
        ygor::g_logger.increase_terminal_verbosity();
        ygor::g_logger.increase_callback_verbosity();
    }

    auto children = OptArgs.getChildren();
    const bool res = Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, children);

    // Reset the log thresholds.
    if(should_reset_afterward){
        ygor::g_logger.set_callback_min_level(log_lvl_callback);
        ygor::g_logger.set_terminal_min_level(log_lvl_terminal);
    }

    return res;
}

