//InvokeStandardScript.cc - A part of DICOMautomaton 2024. Written by hal clark.

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
#include "../Script_Loader.h"
#include "../Standard_Scripts.h"
#include "../Operation_Dispatcher.h"
#include "../Metadata.h"

#include "InvokeStandardScript.h"


OperationDoc OpArgDocInvokeStandardScript(){
    OperationDoc out;
    out.name = "InvokeStandardScript";

    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: scripting");
    out.tags.emplace_back("category: file import");

    out.desc = 
        "This operation invokes a built-in DICOMautomaton script.";
        
    out.notes.emplace_back(
        "Scripts may require configuration via parameter table entries in order to function"
        " correctly. Refer to the scripts themselves for documentation."
    );
        
    out.args.emplace_back();
    out.args.back().name = "Script";
    out.args.back().desc = "The name of the script to invoke.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    for(const auto &s : standard_scripts()){
        const auto n = s.category + "/" + s.name;
        out.args.back().examples.emplace_back(n);
    }
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back() = MetadataInjectionOpArgDoc();
    out.args.back().desc = "Key-value pairs in the form of 'key1@value1;key2@value2' that will be temporarily injected"
                           " into the global parameter table prior to invoking the script."
                           " After the script completes, the specified keys will be reset to their original values"
                           " (or removed if they were not previously set)."
                           "\n\n"
                           "Existing conflicting parameters will be temporarily overwritten."
                           " Both keys and values are case-sensitive."
                           "";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "key1@value1",
                                 "key1@value1;key2@value2",
                                 "key_with_underscores@'a value with spaces'" };

    return out;
}

bool InvokeStandardScript(Drover &DICOM_data,
                          const OperationArgPkg& OptArgs,
                          std::map<std::string, std::string>& InvocationMetadata,
                          const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ScriptStr = OptArgs.getValueStr("Script").value();
    const auto KeyValuesOpt = OptArgs.getValueStr("KeyValues");

    //-----------------------------------------------------------------------------------------------------------------
    const auto tokens = SplitStringToVector(ScriptStr, '/', 'd');
    if(tokens.size() != 2UL){
        throw std::invalid_argument("Script name not understood");
    }
    const auto category = tokens.at(0);
    const auto name = tokens.at(1);


    std::list<OperationArgPkg> Operations;
    const bool op_load_res = Load_Standard_Script( Operations, category, name );
    if(!op_load_res) throw std::runtime_error("Unable to load script");


    // Parse the user-provided key-value pairs, if any.
    metadata_map_t to_inject;
    if(KeyValuesOpt){
        to_inject = parse_key_values(KeyValuesOpt.value());
    }

    // Stow the original values for keys that will be injected.
    metadata_stow_t stowed;
    for(const auto &kvp : to_inject){
        stowed = stow_metadata(InvocationMetadata, stowed, kvp.first);
    }

    // Inject the new key-value pairs into the global parameter table.
    inject_metadata(InvocationMetadata, std::move(to_inject), metadata_preprocessing::none);

    // Ensure the original key-value pairs are restored on scope exit (including exceptions).
    metadata_stow_guard msg(InvocationMetadata, stowed);

    YLOGINFO("Invoking standard script '" << ScriptStr << "' now");
    const auto res = Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, Operations);

    return res;
}

