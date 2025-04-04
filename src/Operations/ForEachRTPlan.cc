//ForEachRTPlan.cc - A part of DICOMautomaton 2022. Written by hal clark.

#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <set>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <list>
#include <memory>
#include <type_traits>

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
#include "../Partition_Drover.h"

#include "ForEachRTPlan.h"


OperationDoc OpArgDocForEachRTPlan() {
    OperationDoc out;
    out.name = "ForEachRTPlan";

    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: rtplan processing");

    out.desc = "This operation is a control flow meta-operation that creates a 'view' of all available data such"
               " that each grouping contains a single treatment plan and any supplementary data it references"
               " (e.g., CT images, RTDOSE images, RTSTRUCT contours, etc.).";

    out.notes.emplace_back(
        "If this operation has no children, it will evaluate to a no-op."
    );
    out.notes.emplace_back(
        "The same supplementary data may appear in multiple groupings (e.g., if multiple plans reference the same"
        " images)."
    );
    out.notes.emplace_back(
        "This operation will only partition homogeneous objects, i.e., composite objects in which all sub-objects"
        " share the same set of metadata (e.g., image arrays, since each image carries its own metadata)."
        " See the GroupImages operation to permanently partition heterogeneous image arrays."
    );
    out.notes.emplace_back(
        "Each invocation is performed sequentially, and all modifications are carried forward for each grouping."
        " Groups are generated on-the-fly, so newly-added elements (e.g.,"
        " new images) created by one invocation are available to participate in subsequent invocations."
    );
    out.notes.emplace_back(
        "The order of all elements, whether included in a plan's group or not, will potentially be re-ordered"
        " after this operation."
    );

    out.args.emplace_back();
    out.args.back() = TPWhitelistOpArgDoc();
    out.args.back().name = "RTPlanSelection";
    out.args.back().default_val = "all";


    out.args.emplace_back();
    out.args.back().name = "IncludeNA";
    out.args.back().desc = "Whether to perform the loop body for the 'N/A' (i.e., non-matching) group if non-empty.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}

bool ForEachRTPlan(Drover &DICOM_data,
                  const OperationArgPkg& OptArgs,
                  std::map<std::string, std::string>& InvocationMetadata,
                  const std::string& FilenameLex){
    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto RTPlanSelectionStr = OptArgs.getValueStr("RTPlanSelection").value();
    const auto IncludeNAStr = OptArgs.getValueStr("IncludeNA").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = Compile_Regex("^tr?u?e?$");
    const bool IncludeNA = std::regex_match(IncludeNAStr, regex_true);

    // Cycle over the Line_Segments, processing each one-at-a-time.
    auto TPs_all = All_TPs( DICOM_data );
    auto TPs = Whitelist( TPs_all, RTPlanSelectionStr );

    for(auto & tp_it : TPs){
        // Select only the components that are relevant to the given plan.
        auto s = Select_Drover(DICOM_data, tp_it);

        if(!Operation_Dispatcher(s.select, InvocationMetadata, FilenameLex, OptArgs.getChildren())){
            throw std::runtime_error("Child analysis failed. Cannot continue");
        }

        // Recombine the split pieces, incorporating any additions/deletions/modifications.
        DICOM_data = Recombine_Selected_Drover(s);
    }

    return true;
}

