//ExecuteShell.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <deque>
#include <optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <cstdlib>  //Needed for popen, pclose.


#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "ExecuteShell.h"

OperationDoc OpArgDocExecuteShell(){
    OperationDoc out;
    out.name = "ExecuteShell";
    out.tags.emplace_back("category: meta");

    out.desc = 
        "This operation executes the given command in a system shell.";

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

    std::string out;
    auto pipe = popen(CommandStr.c_str(), "r");
    if(pipe == nullptr){
        throw std::runtime_error("Unable to create pipe");
    }

    ssize_t nbytes;
    std::array<char, 5000> buff;
    buff.fill('\0');

#ifdef EAGAIN
    while( ((nbytes = read(fileno(pipe), buff.data(), buff.size()-1)) != -1)  || (errno == EAGAIN) ){
#else
    while( ((nbytes = read(fileno(pipe), buff.data(), buff.size()-1)) != -1) ){
#endif
        //Check if we have reached the end of the file (ie. "data has run out.")
        if( nbytes == 0 ) break;
        out += std::string(buff.data(), nbytes);
    }
    const int res = pclose(pipe);

    if(ResultOpt) InvocationMetadata[ResultOpt.value()] = out;
    if(ReturnOpt) InvocationMetadata[ReturnOpt.value()] = Xtostring<int>(res);

    const bool op_ret = (res == EXIT_SUCCESS);
    return op_ret;
}

