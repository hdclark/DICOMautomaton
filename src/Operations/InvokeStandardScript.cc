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

#include "InvokeStandardScript.h"


OperationDoc OpArgDocInvokeStandardScript(){
    OperationDoc out;
    out.name = "InvokeStandardScript";

    out.desc = 
        "This operation invokes a built-in DICOMautomaton script.";
        
    out.notes.emplace_back(
        "Scripts may require configuration via paramete table entries in order to function"
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

    return out;
}

bool InvokeStandardScript(Drover &DICOM_data,
                          const OperationArgPkg& OptArgs,
                          std::map<std::string, std::string>& InvocationMetadata,
                          const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ScriptStr = OptArgs.getValueStr("Script").value();

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

    // TODO: do I want to support a 'synthetic' parameter table environment?
    //       Doing so would make it challenging to allow side-effects to the parameter table.
    //auto l_InvocationMetadata = InvocationMetadata;
    //l_InvocationMetadata["method"] = contouring_method;

    const auto res = Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, Operations);
    return res;
}

