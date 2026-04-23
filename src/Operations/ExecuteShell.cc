//ExecuteShell.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <deque>
#include <optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <sstream>
#include <string>    


#include "../Bash.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "ExecuteShell.h"

OperationDoc OpArgDocExecuteShell(){
    OperationDoc out;
    out.name = "ExecuteShell";

    out.tags.emplace_back("category: meta");

    out.desc = 
        "This operation executes the given command in DICOMautomaton's internal portable bash-like shell.";

    out.args.emplace_back();
    out.args.back().name = "Command";
    out.args.back().desc = "The command(s) to execute using the system shell.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "echo example",
                                 "echo 'another example'", 
                                 R"***(for i in 1 2 3 ; do echo "loop iteration $i" ; done)***",
                                 R"***(for %i in (1 2 3) do echo "loop iteration %i")***" };


    out.args.emplace_back();
    out.args.back().name = "Result";
    out.args.back().desc = "The name of the variable in which to store the shell's stdout."
                           " The result will be stored in the global parameter table;"
                           " the variable name corresponds to the 'key' and the stdout will be stored as the 'value.'"
                           "\n\n"
                           "If no variable name is provided, the stdout will be ignored.";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "result", "stdout", "x" };


    out.args.emplace_back();
    out.args.back().name = "Return";
    out.args.back().desc = "The name of the variable in which to store the shell's return value (if available)."
                           " The result will be stored in the global parameter table;"
                           " the variable name corresponds to the 'key' and the return value will be stored as the 'value.'"
                           "\n\n"
                           "If no variable name is provided, the return value will not be recorded."
                           " However, this operation will still evaluate to 'true' only when the shell reports that the"
                           " command succeeds.";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "ret", "return_value" };

    
    return out;
}

bool ExecuteShell(Drover &DICOM_data,
                  const OperationArgPkg& OptArgs,
                  std::map<std::string, std::string>& InvocationMetadata,
                  const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto CommandStr = OptArgs.getValueStr("Command").value();
    const auto ResultOpt = OptArgs.getValueStr("Result");
    const auto ReturnOpt = OptArgs.getValueStr("Return");

    //-----------------------------------------------------------------------------------------------------------------

    Bash bash;
    const auto bash_result = bash.process(CommandStr);

    std::ostringstream stdout_ss;
    for(size_t i = 0; i < bash_result.output.size(); ++i){
        stdout_ss << bash_result.output[i];
        if((i + 1) < bash_result.output.size()) stdout_ss << '\n';
    }
    const auto out = stdout_ss.str();
    const int res = bash_result.return_code;

    if(ResultOpt) InvocationMetadata[ResultOpt.value()] = out;
    if(ReturnOpt) InvocationMetadata[ReturnOpt.value()] = Xtostring<int>(res);

    const bool op_ret = (res == EXIT_SUCCESS);
    return op_ret;
}
