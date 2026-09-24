//Python.cc.

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "YgorString.h"

#include "../Python_Embed.h"
#include "../Structs.h"
#include "Python.h"

OperationDoc OpArgDocPython(){
    OperationDoc out;
    out.name = "Python";
    out.aliases.emplace_back("PythonScript");
    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: scripting");
    out.desc = "Executes a Python file against the active DICOMautomaton data.";
    out.notes.emplace_back(
        "This operation executes arbitrary code. Python and native mutations are non-transactional.");
    out.notes.emplace_back(
        "The callback receives an operation-scoped session whose data adapters become invalid when the callback returns.");

    out.args.emplace_back();
    out.args.back().name = "Filename";
    out.args.back().desc = "The Python source file to execute.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().mimetype = "text/x-python";

    out.args.emplace_back();
    out.args.back().name = "Function";
    out.args.back().desc = "The callable to invoke with the active operation session.";
    out.args.back().default_val = "process";
    out.args.back().expected = true;
    out.args.back().examples = { "process", "main" };

    out.args.emplace_back();
    out.args.back().name = "ModuleSearchPaths";
    out.args.back().desc = "Optional semicolon-separated module search directories. Paths are applied only during this callback.";
    out.args.back().default_val = "";
    out.args.back().expected = false;

    return out;
}

bool Python(Drover &DICOM_data,
            const OperationArgPkg& OptArgs,
            std::map<std::string, std::string>& InvocationMetadata,
            const std::string& FilenameLex){
    const auto filename = OptArgs.getValueStr("Filename").value();
    const auto function = OptArgs.getValueStr("Function").value();
    const auto search_paths = OptArgs.getValueStr("ModuleSearchPaths").value_or("");

    if(filename.empty()){
        throw std::invalid_argument("Python script filename is empty");
    }
    if(!std::filesystem::is_regular_file(filename)){
        throw std::invalid_argument("Python script is not a regular file: '" + filename + "'");
    }
    if(function.empty()){
        throw std::invalid_argument("Python callback function name is empty");
    }

    std::vector<std::string> module_search_paths;
    if(!search_paths.empty()){
        module_search_paths = SplitStringToVector(search_paths, ';', 'd');
        for(const auto &path : module_search_paths){
            if(path.empty() || !std::filesystem::is_directory(path)){
                throw std::invalid_argument("Python module search path is not a directory: '" + path + "'");
            }
        }
    }

    dcma::python::ExecuteScriptFile(DICOM_data, InvocationMetadata, FilenameLex,
                                    filename, function, module_search_paths);
    return true;
}
