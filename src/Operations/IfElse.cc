//IfElse.cc - A part of DICOMautomaton 2021. Written by hal clark.

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
#include "../Write_File.h"
#include "../File_Loader.h"
#include "../Operation_Dispatcher.h"

#include "IfElse.h"


OperationDoc OpArgDocIfElse() {
    OperationDoc out;
    out.name = "IfElse";
    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: control flow");

    out.aliases.emplace_back("If");
    out.aliases.emplace_back("IfThenElse");
    out.aliases.emplace_back("ElseIf");


    out.desc = "This operation is a control flow meta-operation that performs an 'if-then' or 'if-then-else' by evaluating"
               " child operations. If the first child operation (the conditional) completes without throwing an"
               " exception, then the second operation is performed. Otherwise the third operation ('else statement'),"
               " if present, is performed. Side effects from all evaluated operations are possible.";

    out.notes.emplace_back(
        "Child operations are performed in order, and all side-effects are carried forward."
        " In particular, all selectors in child operations are evaluated lazily, at the moment when the child"
        " operation is invoked."
    );
    out.notes.emplace_back(
        "A single operation is required for each of the condition, 'then' path, and 'else' path."
        " Multiple operations can be wrapped (i.e., combined together) to make a single child operation."
    );
    out.notes.emplace_back(
        "Some operations may succeed without directly signalling failure. For example, an operation that"
        " loops over all selected images may not throw if zero images are selected. This operation works best"
        " with other control flow meta-operations."
    );

    return out;
}

bool IfElse(Drover &DICOM_data,
              const OperationArgPkg& OptArgs,
              std::map<std::string, std::string>& InvocationMetadata,
              const std::string& FilenameLex){
    //-----------------------------------------------------------------------------------------------------------------

    auto children = OptArgs.getChildren();
    if(!isininc(2U,children.size(),3U)){
        throw std::runtime_error("'If' statement accepts 2-3 statements/child operations");
    }

    // Break into condition, success, and failure statements.
    decltype(children) child_condition;
    decltype(children) child_then;
    decltype(children) child_else;
    if(!children.empty()) child_condition.splice( std::end(child_condition), children, std::begin(children) );
    if(!children.empty()) child_then.splice( std::end(child_then), children, std::begin(children) );
    if(!children.empty()) child_else.splice( std::end(child_else), children );
    

    const bool condition = Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, {child_condition});
    if(condition){
        if(!child_then.empty()){
            if(!Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, child_then)){
                throw std::runtime_error("'If' statement child operation in 'true' path failed");
            }
        }
    }else{
        if(!child_else.empty()){
            if(!Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, child_else)){
                throw std::runtime_error("'If' statement child operation in 'false' path failed");
            }
        }
    }

    return true;
}

