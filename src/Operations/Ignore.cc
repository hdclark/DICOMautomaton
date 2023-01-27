//Ignore.cc - A part of DICOMautomaton 2022. Written by hal clark.

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

#include "Ignore.h"


OperationDoc OpArgDocIgnore() {
    OperationDoc out;
    out.name = "Ignore";
    out.aliases.emplace_back("Always");
    out.aliases.emplace_back("Force");

    out.desc = "This operation is a control flow meta-operation that ignores the return"
               " value of all child operations.";

    out.notes.emplace_back(
        "Child operations are performed in order, and all side-effects are carried forward."
        " In particular, all selectors in child operations are evaluated lazily, at the moment when the child"
        " operation is invoked."
    );
    out.notes.emplace_back(
        "This operation ' will always succeed, even if no children are present."
        " This operation works well with idempotent or non-critical children operations."
    );

    return out;
}

bool Ignore(Drover &DICOM_data,
           const OperationArgPkg& OptArgs,
           std::map<std::string, std::string>& InvocationMetadata,
           const std::string& FilenameLex){
    //-----------------------------------------------------------------------------------------------------------------

    auto children = OptArgs.getChildren();
    for(const auto& child : children){
        Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, {child});
    }

    return true;
}

