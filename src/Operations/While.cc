//While.cc - A part of DICOMautomaton 2021. Written by hal clark.

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
#include <cstdint>
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
#include "../Write_File.h"
#include "../File_Loader.h"
#include "../Operation_Dispatcher.h"

#include "While.h"


OperationDoc OpArgDocWhile() {
    OperationDoc out;
    out.name = "While";
    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: control flow");

    out.desc = "This operation is a control flow meta-operation that repeatedly and sequentially invokes child"
               " operations (2-n) until the first child operation completes successfully.";

    out.notes.emplace_back(
        "This operation evaluates the first child (the conditional) before evaluating any other children."
        " So this operation represents a while-loop and not a do-while-loop."
    );
    out.notes.emplace_back(
        "Each repeat is performed sequentially, and all side-effects are carried forward for each iteration."
        " In particular, all selectors in child operations are evaluated lazily, at the moment when the child"
        " operation is invoked. If any non-conditional child operation does not complete successfully, it is"
        " treated as a 'break' statement and a true truthiness is returned."
    );

    out.args.emplace_back();
    out.args.back().name        = "N";
    out.args.back().desc        = "The maximum number of times to loop. If the loop reaches this number of iterations,"
                                  " then this operation returns false truthiness. If 'N' is negative or not provided,"
                                  " then looping will continue indefinitely.";
    out.args.back().default_val = "100";
    out.args.back().expected    = false;
    out.args.back().examples    = {"-1", "0", "5", "10", "1000"};


    return out;
}

bool While(Drover &DICOM_data,
              const OperationArgPkg& OptArgs,
              std::map<std::string, std::string>& InvocationMetadata,
              const std::string& FilenameLex){
    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto N = std::stol( OptArgs.getValueStr("N").value_or("-1") );

    //-----------------------------------------------------------------------------------------------------------------

    // Break into condition and body statements.
    auto children = OptArgs.getChildren();
    if(children.size() < 2){
        throw std::runtime_error("'While' statement requires 2 or more statements/child operations");
    }

    decltype(children) child_condition;
    decltype(children) child_body;
    if(!children.empty()) child_condition.splice( std::end(child_condition), children, std::begin(children) );
    child_body.splice( std::end(child_body), children );

    bool condition = false;
    int64_t i = 0;
    while(true){
        if( (0 <= N) && (N <= i++) ) return false;

        condition = Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, child_condition);
        if(!condition) break;

        if(!Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, child_body)){
            // Treat false truthiness in the body operations as a break statement, to continue execution.
            return true;
        }
    }

    return true;
}

