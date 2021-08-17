//False.cc - A part of DICOMautomaton 2021. Written by hal clark.

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

#include "False.h"


OperationDoc OpArgDocFalse() {
    OperationDoc out;
    out.name = "False";
    out.aliases.emplace_back("Throw");

    out.desc = "This operation is a control flow meta-operation that does not complete successfully."
               " It has no side effects.";

    return out;
}

Drover False(Drover DICOM_data,
             const OperationArgPkg& /*OptArgs*/,
             const std::map<std::string, std::string>& /*InvocationMetadata*/,
             const std::string& /*FilenameLex*/){

    throw std::runtime_error("This is a false truthiness signal."
                             " It should only be used within a conditional control flow meta-operation.");

    return DICOM_data;
}

