//PrintMetadata.cc - A part of DICOMautomaton 2024. Written by hal clark.

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
#include <utility>
#include <vector>

#include <filesystem>

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorString.h"

#include "Explicator.h"

#include "../Structs.h"
#include "../Metadata.h"
#include "../Regex_Selectors.h"
#include "../Operation_Dispatcher.h"

#include "PrintMetadata.h"


OperationDoc OpArgDocPrintMetadata() {
    OperationDoc out;
    out.name = "PrintMetadata";

    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: parameter table");

    out.desc = "This operation can be used to print the value corresponding to a key in the global parameter table."
               "\n\n"
               "This operation is meant to be used to extract information as part of a pipeline, where stdout"
               " can be intercepted.";

    out.notes.emplace_back(
        "The output is printed to stdout."
    );
    out.notes.emplace_back(
        "If the key does not exist, nothing will be emitted."
    );
    out.notes.emplace_back(
        "This operation will succeed only if there is a key-value present with the specified key."
    );

    out.args.emplace_back();
    out.args.back().name = "Key";
    out.args.back().desc = "The key selection.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "ROIName", "SliceThickness", "xyz" };

    return out;
}

bool PrintMetadata(Drover &DICOM_data,
                   const OperationArgPkg& OptArgs,
                   std::map<std::string, std::string>& InvocationMetadata,
                   const std::string& FilenameLex){
    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto Key = OptArgs.getValueStr("Key").value();

    //-----------------------------------------------------------------------------------------------------------------

    bool key_was_present = false;
    if(InvocationMetadata.count(Key) != 0UL){
       std::cout << InvocationMetadata[Key] << std::endl;
       key_was_present = true;
    }
    return key_was_present;
}

