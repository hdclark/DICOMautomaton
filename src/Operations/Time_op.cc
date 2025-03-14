//Time_op.cc - A part of DICOMautomaton 2022. Written by hal clark.

#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <sstream>
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
#include <chrono>
#include <filesystem>
#include <array>
#include <ctime>
#include <iomanip>

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

#include "Time_op.h"


OperationDoc OpArgDocTime() {
    OperationDoc out;
    out.name = "Time";

    out.tags.emplace_back("category: meta");

    out.desc = "This operation is a control flow meta-operation that times how long it takes to execute the"
               " child operations.";

    out.notes.emplace_back(
        "Child operations are performed in order, and all side-effects are carried forward."
        " In particular, all selectors in child operations are evaluated lazily, at the moment when the child"
        " operation is invoked."
    );
    out.notes.emplace_back(
        "This operation behaves equivalent to the boolean 'And' meta-operation."
        " If a child operation fails, the remaining child operations are not performed."
    );

    return out;
}

bool Time(Drover &DICOM_data,
          const OperationArgPkg& OptArgs,
          std::map<std::string, std::string>& InvocationMetadata,
          const std::string& FilenameLex){
    //-----------------------------------------------------------------------------------------------------------------
    // We're stuck with C-style conversion until C++20.
    const auto time_str = [](std::time_t t){
        return ygor::get_localtime_str(t, "%Y-%m-%d %H:%M:%S");
    };

    const auto start = std::chrono::system_clock::now();
    YLOGINFO("Start time: " << time_str(std::chrono::system_clock::to_time_t(start)));

    const bool res = Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, OptArgs.getChildren());

    const auto end = std::chrono::system_clock::now();
    YLOGINFO("Stop time: " << time_str(std::chrono::system_clock::to_time_t(end)));

    const std::chrono::duration<double> diff = (end - start);

    std::stringstream ss;
    ss << std::fixed << std::setprecision(6) << diff.count();
    const auto diff_str = ss.str();
    YLOGINFO("Elapsed time: " << diff_str << " seconds");

    if(!res){
        throw std::runtime_error("Child operation failed");
    }

    return true;
}

