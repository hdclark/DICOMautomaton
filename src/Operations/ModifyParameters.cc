//ModifyParameters.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <asio.hpp>
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
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../Write_File.h"
#include "../File_Loader.h"
#include "../String_Parsing.h"
#include "../Operation_Dispatcher.h"
#include "../Dialogs/Text_Query.h"

#include "ModifyParameters.h"


OperationDoc OpArgDocModifyParameters(){
    OperationDoc out;
    out.name = "ModifyParameters";

    out.desc = 
        "Define or delete a key-value parameter into/from the global parameter table.";

    out.args.emplace_back();
    out.args.back().name = "Actions";
    out.args.back().desc = "Three actions are understood: 'define', 'insert', and 'remove'."
                           "\n"
                           "The 'define' action accepts a key-value pair and injects it into the global parameter"
                           " table. Note that this operation will overwrite any existing parameters with the"
                           " same key."
                           "\n"
                           "The 'insert' action behaves like 'define' except it will not overwrite any existing"
                           " parameters."
                           "\n"
                           "The 'delete' action accepts a key and removes it from the global parameter"
                           " table if it is already present. Otherwise, no action is taken."
                           "";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "define(key_1, value_1)",
                                 "insert(key_1, value_1); define('key 2', 'value 2\\, which has a comma')",
                                 "remove('key 3')" };

    return out;
}

bool ModifyParameters(Drover &DICOM_data,
                      const OperationArgPkg& OptArgs,
                      std::map<std::string, std::string>& InvocationMetadata,
                      const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ActionsStr = OptArgs.getValueStr("Actions").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_define = Compile_Regex("^defi?n?e?$");
    const auto regex_insert = Compile_Regex("^in?s?e?r?t?$|^in?j?e?c?t?$");
    const auto regex_remove = Compile_Regex("^re?m?o?v?e?$|^dele?t?e?$");

    // Extract the queries.
    const auto pfs = parse_functions(ActionsStr);
    if(pfs.empty()){
        throw std::invalid_argument("No parameters specified");
    }

    for(const auto &pf : pfs){

        // Definitions.
        if(std::regex_match(pf.name, regex_define)){
            if(pf.parameters.size() != 2){
                throw std::invalid_argument("Incorrect number of arguments were provided: define() requires two arguments");
            }

            const auto key = pf.parameters.at(0).raw;
            const auto val = pf.parameters.at(1).raw;

            FUNCINFO("Defining parameter with key '" << key << "' : '" << val << "'");
            InvocationMetadata[key] = val;

        // Insertions.
        }else if(std::regex_match(pf.name, regex_insert)){
            if(pf.parameters.size() != 2){
                throw std::invalid_argument("Incorrect number of arguments were provided: insert() requires two arguments");
            }

            const auto key = pf.parameters.at(0).raw;
            const auto val = pf.parameters.at(1).raw;

            FUNCINFO("Inserting parameter with key '" << key << "' : '" << val << "'");
            if(InvocationMetadata.count(key) == 0){
                InvocationMetadata[key] = val;
            }

        // Removals.
        }else if(std::regex_match(pf.name, regex_remove)){
            if(pf.parameters.size() != 1){
                throw std::invalid_argument("Incorrect number of arguments were provided: remove() requires one argument");
            }

            const auto key = pf.parameters.at(0).raw;

            FUNCINFO("Removing key '" << key << "'");
            InvocationMetadata.erase(key);

        }else{
            throw std::invalid_argument("Action not understood");
        }
    }

    return true;
}

