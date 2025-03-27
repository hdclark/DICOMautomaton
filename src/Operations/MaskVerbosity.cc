//MaskVerbosity.cc - A part of DICOMautomaton 2024. Written by hal clark.

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

#include "MaskVerbosity.h"


OperationDoc OpArgDocMaskVerbosity() {
    OperationDoc out;
    out.name = "MaskVerbosity";
    out.tags.emplace_back("category: meta");

    out.aliases.emplace_back("MaskWarnings");
    out.aliases.emplace_back("MaskLogs");
    out.aliases.emplace_back("MaskNotifications");
    out.aliases.emplace_back("SilenceWarnings");

    out.desc = "This operation is a meta-operation that temporarily alters the global log verbosity level."
               " Child operations are executed with the adjusted verbosity level, which affects what log"
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
    out.args.back().name = "Permanent";
    out.args.back().desc = "Controls whether the original verbosity levels are reset after invoking children"
                           " operations. If false, the effect is temporary and applied only to children operations."
                           " If true, the effect is permanent and applies to all subsequent operations.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}

bool MaskVerbosity(Drover &DICOM_data,
                   const OperationArgPkg& OptArgs,
                   std::map<std::string, std::string>& InvocationMetadata,
                   const std::string& FilenameLex){
    //-----------------------------------------------------------------------------------------------------------------
    const auto VerbosityStr = OptArgs.getValueStr("Verbosity").value();
    const auto PermanentStr = OptArgs.getValueStr("Permanent").value();
    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = Compile_Regex("^tr?u?e?$");

    const auto regex_inc = Compile_Regex("^in?c?r?e?a?s?e?");
    const auto regex_dec = Compile_Regex("^de?c?r?e?a?s?e?");

    const bool should_inc = std::regex_match(VerbosityStr, regex_inc);
    const bool should_dec = std::regex_match(VerbosityStr, regex_dec);

    const bool make_permanent = std::regex_match(PermanentStr, regex_true);

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
    if(!make_permanent){
        ygor::g_logger.set_callback_min_level(log_lvl_callback);
        ygor::g_logger.set_terminal_min_level(log_lvl_terminal);
    }

    return res;
}

