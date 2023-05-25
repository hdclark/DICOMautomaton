//Fork.cc - A part of DICOMautomaton 2023. Written by hal clark.

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

#include <unistd.h>           //fork().
#include <cstdlib>            //quick_exit(), EXIT_SUCCESS.

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../Operation_Dispatcher.h"

#include "Fork.h"


OperationDoc OpArgDocFork() {
    OperationDoc out;
    out.name = "Fork";
    out.aliases.emplace_back("Spawn");

    out.desc = "This operation is a control flow meta-operation that causes child operations to be executed"
               " in a POSIX fork. The main process returns immediately after forking, while the fork executes"
               " the children operations.";

    out.notes.emplace_back(
        "The return value of the main process describes whether the fork was successful, not whether the"
        " children operations succeeded. Since the forked process is detached, the return value of the"
        " children operations are ignored; however, execution will otherwise continue normally,"
        " terminating if an operation fails."
    );
    out.notes.emplace_back(
        "The forked process is detached from the main process, so any state changes in the fork are contained"
        " to the fork only."
    );
    out.notes.emplace_back(
        "Child operations are performed in order, and all side-effects are carried forward in the fork."
        " In particular, all selectors in child operations are evaluated lazily, at the moment when the child"
        " operation is invoked."
    );

    return out;
}

bool Fork(Drover &DICOM_data,
          const OperationArgPkg &OptArgs,
          std::map<std::string, std::string> &InvocationMetadata,
          const std::string &FilenameLex ){
    //-----------------------------------------------------------------------------------------------------------------
    auto children = OptArgs.getChildren();

    auto pid = fork();
    if(pid == -1){ // Parent process.
        throw std::runtime_error("Unable to fork");

    }else if(0 < pid){ // Parent process.
        YLOGINFO("Successfully forked process " << pid);

    }else if(pid == 0){ // Child process.
        if(!Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, children)){
            YLOGERR("Forked child operations failed");
        }
        quick_exit(EXIT_SUCCESS);

    }else{
        throw std::runtime_error("Unrecognized fork status");
    }

    return true;
}

