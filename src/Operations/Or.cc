//Or.cc - A part of DICOMautomaton 2021. Written by hal clark.

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

#include "Or.h"


OperationDoc OpArgDocOr() {
    OperationDoc out;
    out.name = "Or";
    out.aliases.emplace_back("Coalesce");

    out.desc = "This operation is a control flow meta-operation that evaluates all children operations until one"
               " completes successfully.";

    out.notes.emplace_back(
        "If this operation has no children, or no children complete successfully, then this operation signals"
        " false truthiness."
    );
    out.notes.emplace_back(
        "Each child is performed sequentially and lazily in the order specified, with all side-effects carried forward."
        " In particular, all selectors in child operations are evaluated lazily, at the moment when the child"
        " operation is invoked. After the first child completes successfully, no other children will be evaluated."
    );

    return out;
}

bool Or(Drover &DICOM_data,
          const OperationArgPkg& OptArgs,
          std::map<std::string, std::string>& InvocationMetadata,
          const std::string& FilenameLex){

    //-----------------------------------------------------------------------------------------------------------------

    bool condition = false;
    auto children = OptArgs.getChildren();
    while(!children.empty()){
        decltype(children) first_child;
        first_child.splice( std::end(first_child), children, std::begin(children) );

        try{
            condition = Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, first_child);
        }catch(const std::exception &){ }

        if(condition) break;
    }

    if(!condition) throw std::runtime_error("No child evaluated to 'true'");

    return true;
}

