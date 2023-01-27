//Sleep.cc - A part of DICOMautomaton 2022. Written by hal clark.

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
#include <thread>

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

#include "Sleep.h"


OperationDoc OpArgDocSleep() {
    OperationDoc out;
    out.name = "Sleep";
    out.aliases.emplace_back("Delay");
    out.aliases.emplace_back("Wait");

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

    out.args.emplace_back();
    out.args.back().name = "Duration";
    out.args.back().desc = "The length of time to wait, in seconds.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.1", "1.23", "5" };

    return out;
}

bool Sleep(Drover&,
           const OperationArgPkg& OptArgs,
           std::map<std::string, std::string>&,
           const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto Duration = std::stod( OptArgs.getValueStr("Duration").value() );

    //-----------------------------------------------------------------------------------------------------------------

    bool res = true;
    if( isininc(0.0f, Duration, 60.0f * 60.0f * 24.0f) ){
        const auto in_ms = static_cast<long int>( std::round(1000.0 * Duration) );
        const auto d = std::chrono::milliseconds(in_ms);
        std::this_thread::sleep_for(d);

    }else{
        YLOGWARN("Refusing to sleep for given duration");
        res = false;
    }

    return res;
}

