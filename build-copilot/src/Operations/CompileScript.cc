//CompileScript.cc - A part of DICOMautomaton 2023. Written by hal clark.

#include <deque>
#include <optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Script_Loader.h"
#include "../Operation_Dispatcher.h"
//#include "../Write_File.h"

#include "CompileScript.h"

OperationDoc OpArgDocCompileScript(){
    OperationDoc out;
    out.name = "CompileScript";

    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: scripting");

    out.desc = 
        "This operation can be used to parse, optionally validate, and optionally run a DICOMautomaton script.";


    out.args.emplace_back();
    out.args.back().name = "Filename";
    out.args.back().desc = "The name of a file containing the script.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "script.dscr", "/path/to/script.dscr" };
    out.args.back().mimetype = "text/plain";


    out.args.emplace_back();
    out.args.back().name = "Actions";
    out.args.back().desc = "The actions to perform on  or using the script."
                           " Current options are 'parse', 'validate', and 'run'."
                           "\n\n"
                           "The 'parse' action loads the script and attempts to parse it."
                           " An abstract syntax tree is constructed, but no warnings, errors, or feedback is provided."
                           " The script is not actually executed."
                           " In this mode, the return value indicates whether parsing errors were detected."
                           "\n\n"
                           "The 'validate' action parses the script, then prints out warnings, errors, and notices."
                           " The script is not actually executed."
                           " In this mode, the return value indicates whether validation errors were detected"
                           " (note: warnings are ignored)."
                           " Note that 'lint' and 'compile' are currently synonyms for 'validate'."
                           "\n\n"
                           "The 'run' action parses and validates the script, then immediately executes it."
                           " In this mode, the return value indicates two things: (1) that there were no errors"
                           " detected during the parsing and validation steps, and (2) the return value of the"
                           " operations."
                           " Note that 'execute' is a synonym for 'run'.";
    out.args.back().default_val = "validate";
    out.args.back().expected = true;
    out.args.back().examples = { "parse",
                                 "validate", "lint", "compile",
                                 "run", "execute" };
    out.args.back().samples = OpArgSamples::Exhaustive;
    
    return out;
}

bool CompileScript(Drover &DICOM_data,
                const OperationArgPkg& OptArgs,
                std::map<std::string, std::string>& InvocationMetadata,
                const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    auto Filename = OptArgs.getValueStr("Filename").value();
    auto ActionsStr = OptArgs.getValueStr("Actions").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_parse    = Compile_Regex("^pa?r?s?e?");
    const auto regex_valid    = Compile_Regex("^va?l?i?d?a?t?e?");
    const auto regex_lint     = Compile_Regex("^li?n?t?");
    const auto regex_compile  = Compile_Regex("^co?m?p?i?l?e?");
    const auto regex_run      = Compile_Regex("^ru?n?");
    const auto regex_execute  = Compile_Regex("^ex?e?c?u?t?e?");

    const bool should_parse    = std::regex_match(ActionsStr, regex_parse);
    const bool should_validate = (    std::regex_match(ActionsStr, regex_valid)
                                   || std::regex_match(ActionsStr, regex_lint)
                                   || std::regex_match(ActionsStr, regex_compile) );
    const bool should_execute  = (    std::regex_match(ActionsStr, regex_run)
                                   || std::regex_match(ActionsStr, regex_execute) );

    if(const int count = ( should_parse    ? 1 : 0 )
                       + ( should_validate ? 1 : 0 )
                       + ( should_execute  ? 1 : 0 ) ; count != 1){
        throw std::invalid_argument("Action not understood");
    }

    // Load the file.
    if(Filename.empty()){
        throw std::invalid_argument("No script filename provided");
    }
    std::ifstream fi(Filename, std::ios::in | std::ios::binary);
    if(!fi){
        throw std::invalid_argument("Cannot access script file");
    }

    // Parse the file.
    std::list<script_feedback_t> feedback;
    std::list<OperationArgPkg> ops;
    const bool script_loaded = Load_DCMA_Script(fi, feedback, ops);
    fi.close();

    if(should_parse){
        return script_loaded;
    }

    // Emit feedback generated from parsing and validating the script.
    YLOGINFO("Loaded script with " << ops.size() << " operations");
    Print_Feedback(std::cout, feedback);

    if( should_validate
    ||  !script_loaded ){
        return script_loaded;
    }

    // Execute the script.
    const bool ret = Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, ops);
    return ret;
}
