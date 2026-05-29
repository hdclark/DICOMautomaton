//True.cc - A part of DICOMautomaton 2021. Written by hal clark.

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

#include "True.h"


OperationDoc OpArgDocTrue() {
    OperationDoc out;
    out.name = "True";

    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: control flow");

    out.desc = "This operation is a control flow meta-operation that completes successfully."
               " It has no side effects and evaluates to a no-op.";

    return out;
}

bool True(Drover& /*DICOM_data*/,
          const OperationArgPkg& /*OptArgs*/,
          std::map<std::string, std::string>& /*InvocationMetadata*/,
          const std::string& /*FilenameLex*/){

    return true;
}

