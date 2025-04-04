//EvaluateParameters.cc - A part of DICOMautomaton 2024. Written by hal clark.

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

#include "EvaluateParameters.h"


OperationDoc OpArgDocEvaluateParameters(){
    OperationDoc out;
    out.name = "EvaluateParameters";

    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: control flow");
    out.tags.emplace_back("category: parameter table");

    out.desc = 
        "Exposes the global parameter metadata table for query and evaluation.";

    out.args.emplace_back();
    out.args.back().name = "Contains";
    out.args.back().desc = "Key@value pairs that can be used to check for the presence of specific metadata."
                           " Keys are interpretted verbatim, but values are interpretted as regex."
                           "\n\n"
                           "Note that if the key is absent in the table, the value will never match."
                           "\n\n"
                           "Note to query if a given key is present, regardless of the value, use a regex"
                           " that matches any input, e.g., 'key@.*'."
                           "";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "Modality@CT",
                                 "StudyDate@.*2024.*",
                                 "SomeMetadataKey@.*" };

    return out;
}

bool EvaluateParameters(Drover& /*DICOM_data*/,
                        const OperationArgPkg& OptArgs,
                        std::map<std::string, std::string>& InvocationMetadata,
                        const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ContainsOpt = OptArgs.getValueStr("Contains");

    //-----------------------------------------------------------------------------------------------------------------
    bool ret = false;

    if(ContainsOpt){
        auto tokens = SplitStringToVector(ContainsOpt.value(),'@','d');
        if(tokens.size() != 2UL){
            throw std::invalid_argument("'Contains' parameter not understood");
        }
        const auto l_key = tokens.at(0);
        const auto l_val = tokens.at(1);

        const auto im_val = get_as<std::string>(InvocationMetadata, l_key);

        if(im_val){
            const auto regex_l_val = Compile_Regex(l_val);

            const bool matches = std::regex_match(im_val.value(), regex_l_val);
            ret = matches;
        }
    }

    return ret;
}

