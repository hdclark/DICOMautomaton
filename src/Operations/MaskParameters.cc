//MaskParameters.cc - A part of DICOMautomaton 2024. Written by hal clark.

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

#include "MaskParameters.h"


OperationDoc OpArgDocMaskParameters() {
    OperationDoc out;
    out.name = "MaskParameters";
    out.aliases.emplace_back("MaskMetadata");

    out.desc = "This operation is a meta-operation that temporarily alters the global parameter table."
               " Child operations are executed with the adjusted parameter table, which affects what key-values"
               " appear.";

    out.notes.emplace_back(
        "The parameter table is a shared object that all operations have access to. This operation creates a"
        " snapshot of the parameter table, optionally modifies the copy, invokes children operations, and then"
        " resets the original parameter table."
    );

    out.args.emplace_back();
    out.args.back().name = "Method";
    out.args.back().desc = "Controls how the parameter table is merged after invoking children operations."
                           "\n\n"
                           "'reset' causes the temporary copy to be discarded and the original,"
                           " unmodified parameter table to be reinstated."
                           "\n\n"
                           "'retain' causes the temporary copy to permanently take the place of the original"
                           " parameter table."
                           "\n\n"
                           "'transaction' causes the temporary copy to permanently take the place of the original"
                           " parameter table, but *only* if all children operations complete successfully."
                           " If the children operations fail or return false, the original, unmodified parameter table"
                           " will be reinstated. This method is helpful to ensure a multi-part operation is either"
                           " completed fully, or has no impact."
                           "";
    out.args.back().default_val = "reset";
    out.args.back().expected = true;
    out.args.back().examples = { "reset", "retain", "transaction" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}

bool MaskParameters(Drover &DICOM_data,
                    const OperationArgPkg& OptArgs,
                    std::map<std::string, std::string>& InvocationMetadata,
                    const std::string& FilenameLex){
    //-----------------------------------------------------------------------------------------------------------------
    const auto MethodStr = OptArgs.getValueStr("Method").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = Compile_Regex("^tr?u?e?$");

    const auto regex_reset = Compile_Regex("^rese?t?");
    const auto regex_retain = Compile_Regex("^reta?i?n?");
    const auto regex_trans = Compile_Regex("^tr?a?n?s?a?c?t?i?o?n?");

    const bool should_reset = std::regex_match(MethodStr, regex_reset);
    const bool should_retain = std::regex_match(MethodStr, regex_retain);
    const bool should_trans = std::regex_match(MethodStr, regex_trans);

    auto l_InvocationMetadata = InvocationMetadata;

    if(false){
    }else if(should_reset){
    }else if(should_retain){
    }else if(should_trans){
    }else{
        throw std::runtime_error("Method not understood");
    }

    auto children = OptArgs.getChildren();
    const bool res = Operation_Dispatcher(DICOM_data, l_InvocationMetadata, FilenameLex, children);

    if(false){
    }else if(should_reset){
        // Do nothing, children were already using a temporary.

    }else if(should_retain){
        // Promote the temporary.
        InvocationMetadata = l_InvocationMetadata;

    }else if(should_trans){
        // Promote the temporary iff successful.
        if(res){
            InvocationMetadata = l_InvocationMetadata;
        }
    }else{
        throw std::runtime_error("Method not understood");
    }

    return res;
}

