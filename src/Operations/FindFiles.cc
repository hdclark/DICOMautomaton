//FindFiles.cc - A part of DICOMautomaton 2024. Written by hal clark.

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
#include "../Metadata.h"
#include "../Thread_Pool.h"
#include "../Write_File.h"
#include "../String_Parsing.h"
#include "../Operation_Dispatcher.h"

#include "FindFiles.h"


OperationDoc OpArgDocFindFiles(){
    OperationDoc out;
    out.name = "FindFiles";

    out.desc = 
        "Search a directory for files and/or subdirectories without loading them."
        " Children operations will be invoked once for each file or directory.";

    out.notes.emplace_back(
        "The search is halted when a child operation returns false."
    );
    out.notes.emplace_back(
        "The return value is 'false' if a child operation fails or returns false, otherwise the return value is true."
    );

    out.args.emplace_back();
    out.args.back().name = "Key";
    out.args.back().desc = "Children operations will be invoked once per located file or subdirectory. The path will"
                           " be saved temporarily in the global parameter table using this argument as the key."
                           "\n\n"
                           "Note that any existing key will be reset after this operation runs.";
    out.args.back().default_val = "path";
    out.args.back().expected = true;
    out.args.back().examples = { "path", "file", "dir", "x" };


    out.args.emplace_back();
    out.args.back().name = "RootDir";
    out.args.back().desc = "The root directory to search. Note that backslashes might be interpretted as esccape"
                           " characters.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "/tmp/", "$HOME", "/path/to/root/dir" };


    out.args.emplace_back();
    out.args.back().name = "Recurse";
    out.args.back().desc = "Controls whether the search should recurse into directories. If false, only the"
                           " root directory is searched.";
    out.args.back().default_val = "true";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back().name = "Target";
    out.args.back().desc = "Controls whether files, directories, or both should be located.";
    out.args.back().default_val = "files";
    out.args.back().expected = true;
    out.args.back().examples = { "files", "directories", "files+directories" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}

bool FindFiles(Drover& DICOM_data,
               const OperationArgPkg& OptArgs,
               std::map<std::string, std::string>& InvocationMetadata,
               const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto Key     = OptArgs.getValueStr("Key").value();
    const auto RootDir = OptArgs.getValueStr("RootDir").value();
    const auto Recurse = OptArgs.getValueStr("Recurse").value();
    const auto Target  = OptArgs.getValueStr("Target").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = Compile_Regex("^tr?u?e?$");

    const auto regex_files = Compile_Regex(".*fi?l?e?s?.*");
    const auto regex_dirs  = Compile_Regex(".*di?r?e?c?t?o?r?i?e?s?.*");

    const auto should_recurse = std::regex_match(Recurse, regex_true);

    const auto get_files = std::regex_match(Target, regex_files);
    const auto get_dirs  = std::regex_match(Target, regex_dirs);

    const auto root = std::filesystem::path(RootDir);
    if( !std::filesystem::is_directory(root) ){
        throw std::invalid_argument("RootDir argument is either not recognized as a directory or not accessible.");
    }

    bool ret = true;

    // Store the original state of the key in the global parameter table.
    const auto orig_val = get_as<std::string>(InvocationMetadata, Key);

    // Search for files/directories.
    std::list< std::filesystem::path > paths;

    YLOGINFO("Beginning search now");
    const auto f_search = [&](const auto &dir_iter){
        for(const auto &p :  dir_iter){
            if( get_dirs
            &&  std::filesystem::is_directory(p) ){
                paths.emplace_back(p);
                continue;
            }

            if( get_files
            &&  !std::filesystem::is_directory(p)
            &&  std::filesystem::exists(p) ){
                // Consider anything that exists and is not a directory as a file.
                paths.emplace_back(p);
                continue;
            }
        }
        return;
    };

    if(should_recurse){
        f_search(std::filesystem::recursive_directory_iterator(root));
    }else{
        f_search(std::filesystem::directory_iterator(root));
    }

    // Invoke the children operations for each path.
    YLOGINFO("Located " << paths.size() << " files/directories, beginning processing now");
    for(const auto &p : paths){
        InvocationMetadata[Key] = p.string();

        ret = Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, OptArgs.getChildren());
        if(!ret) break;
    }

    // Restore the key to its original state in the global parameter table.
    InvocationMetadata.erase(Key);
    if(orig_val){
        InvocationMetadata[Key] = orig_val.value();
    }

    return ret;
}

