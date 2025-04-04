//Repeat.cc - A part of DICOMautomaton 2020. Written by hal clark.

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
#include <cstdint>

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

#include "Repeat.h"


OperationDoc OpArgDocRepeat() {
    OperationDoc out;
    out.name = "Repeat";

    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: control flow");

    out.desc = "This operation is a control flow meta-operation that repeatedly and sequentially invokes all child"
               " operations the given number of times.";

    out.notes.emplace_back(
        "If this operation has no children, this operation will evaluate to a no-op."
    );
    out.notes.emplace_back(
        "Each repeat is performed sequentially, and all side-effects are carried forward for each iteration."
        " In particular, all selectors in child operations are evaluated lazily, at the moment when the child"
        " operation is invoked."
    );
    out.notes.emplace_back(
        " This operation will most often be used to repeat operations that compose naturally, such as"
        " repeatedly applying a small Gaussian filter to simulate a single Gaussian filter with a large"
        " kernel, iteratively refining a calculation, loading multiple copies of the same file, or"
        " attempting a given analysis while waiting for data from a remote server."
    );

    out.args.emplace_back();
    out.args.back().name        = "N";
    out.args.back().desc        = "The number of times to repeat the children operations.";
    out.args.back().default_val = "0";
    out.args.back().expected    = true;
    out.args.back().examples    = {"0", "1", "5", "10", "1000"};

    return out;
}

bool Repeat(Drover &DICOM_data,
              const OperationArgPkg& OptArgs,
              std::map<std::string, std::string>& InvocationMetadata,
              const std::string& FilenameLex){
    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto N = std::stol( OptArgs.getValueStr("N").value() );

    //-----------------------------------------------------------------------------------------------------------------
    if(N < 0) throw std::invalid_argument("N must be positive");

    YLOGINFO("Repeating " << OptArgs.getChildren().size() << " immediate children operations " << N << " times");

    for(int64_t i = 0; i < N; ++i){
        if(!Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, OptArgs.getChildren())){
            throw std::runtime_error("Child operation failed");
        }
    }

    return true;
}

