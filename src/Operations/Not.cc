//Not.cc - A part of DICOMautomaton 2021. Written by hal clark.

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
#include "../Operation_Dispatcher.h"

#include "Not.h"


OperationDoc OpArgDocNot() {
    OperationDoc out;
    out.name = "Not";

    out.desc = "This operation is a control flow meta-operation that requires no child operation to complete successfully.";

    out.notes.emplace_back(
        "If this operation has no children, this operation will evaluate to a no-op."
    );
    out.notes.emplace_back(
        "Each child is performed sequentially in the order specified, and all side-effects are carried forward."
        " In particular, all selectors in child operations are evaluated lazily, at the moment when the child"
        " operation is invoked."
    );

    return out;
}

Drover Not(Drover DICOM_data,
           const OperationArgPkg& OptArgs,
           const std::map<std::string, std::string>& InvocationMetadata,
           const std::string& FilenameLex){

    //-----------------------------------------------------------------------------------------------------------------

    auto children = OptArgs.getChildren();
    while(!children.empty()){
        decltype(children) first_child;
        first_child.splice( std::end(first_child), children, std::begin(children) );

        bool condition = false;
        try{
            condition = Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, first_child);
        }catch(const std::exception &){ }

        if(condition){
            throw std::runtime_error("Child evaluated to 'true'");
        }
    }

    return DICOM_data;
}

