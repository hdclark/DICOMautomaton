//AnyOf.cc - A part of DICOMautomaton 2022. Written by hal clark.

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

#include "AnyOf.h"


OperationDoc OpArgDocAnyOf() {
    OperationDoc out;
    out.name = "AnyOf";
    out.aliases.emplace_back("FirstOf");
    out.aliases.emplace_back("Or");
    out.aliases.emplace_back("Coalesce");

    out.desc = "This operation is a control flow meta-operation that performs an 'any-of' or 'first-of' Boolean check"
               " by evaluating child operations. The first child operation that succeeds short-circuits the remaining"
               " checks and returns true. If no child operation succeeds, false is returned."
               " Side effects from all evaluated operations are possible.";

    out.notes.emplace_back(
        "Child operations are performed in order, and all side-effects are carried forward."
        " In particular, all selectors in child operations are evaluated lazily, at the moment when the child"
        " operation is invoked."
    );
    out.notes.emplace_back(
        "If this operation has no children, or no children complete successfully, then this operation signals"
        " false truthiness."
    );
    out.notes.emplace_back(
        "Some operations may succeed without directly signalling failure. For example, an operation that"
        " loops over all selected images may not throw if zero images are selected. This operation works well"
        " with other control flow meta-operations, for example as a conditional in an if-else statement."
    );

    return out;
}

bool AnyOf(Drover &DICOM_data,
           const OperationArgPkg& OptArgs,
           std::map<std::string, std::string>& InvocationMetadata,
           const std::string& FilenameLex){
    //-----------------------------------------------------------------------------------------------------------------

    auto children = OptArgs.getChildren();
    if(children.empty()){
        throw std::runtime_error("This operation requires at least one child operation");
    }

    for(const auto& child : children){
        const bool condition = Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, {child});
        if(condition) return true;
    }

    return false;
}

