//ForEachDistinct.cc - A part of DICOMautomaton 2020. Written by hal clark.

#include <asio.hpp>
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

#include "ForEachDistinct.h"


OperationDoc OpArgDocForEachDistinct() {
    OperationDoc out;
    out.name = "ForEachDistinct";

    out.desc = "This operation is a control flow meta-operation that partitions all available data and invokes all"
               " child operations once for each distinct partition.";

    out.notes.emplace_back(
        "If this operation has no children, this operation will evaluate to a no-op."
    );
    out.notes.emplace_back(
        "This operation will only partition homogeneous objects, i.e., composite objects in which all sub-objects"
        " share the same set of metadata (e.g., image arrays, since each image carries its own metadata)."
        " This guarantees there will be no side-effects due to the partitioning."
        " For this reason, this operation is most commonly used on high-level metadata tags that are expected to be"
        " uniform across sub-objects."
        " See the GroupImages operation to permanently partition heterogeneous image arrays."
    );
    out.notes.emplace_back(
        "Each invocation is performed sequentially, and all modifications are carried forward for each grouping."
        " However, partitions are generated before any child operations are invoked, so newly-added elements (e.g.,"
        " new Image_Arrays) created by one invocation will not participate in subsequent invocations."
        " The order of the de-partitioned data is stable, though additional elements added will follow the partition"
        " they were generated from (and will thus not necessarily be placed at the last position)."
    );
    out.notes.emplace_back(
        "This operation will most often be used to process data group-wise rather than as a whole."
    );

    out.args.emplace_back();
    out.args.back().name = "KeysCommon";
    out.args.back().desc = "Metadata keys to use for exact-match groupings on all data types."
                           " For each partition that is produced,"
                           " every element will share the same key-value pair. This is generally useful for non-numeric"
                           " (or integer, date, etc.) key-values. A ';'-delimited list can be specified to group"
                           " on multiple criteria simultaneously. An empty string disables metadata-based grouping.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "FrameOfReferenceUID",
                                 "BodyPartExamined;StudyDate",
                                 "SeriesInstanceUID", 
                                 "StationName" };

    out.args.emplace_back();
    out.args.back().name = "IncludeNA";
    out.args.back().desc = "Whether to perform the loop body for the 'N/A' (i.e., non-matching) group if non-empty.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}

bool ForEachDistinct(Drover &DICOM_data,
              const OperationArgPkg& OptArgs,
              std::map<std::string, std::string>& InvocationMetadata,
              const std::string& FilenameLex){
    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto KeysCommonStr = OptArgs.getValueStr("KeysCommon").value();
    const auto IncludeNAStr = OptArgs.getValueStr("IncludeNA").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = Compile_Regex("^tr?u?e?$");
    const bool IncludeNA = std::regex_match(IncludeNAStr, regex_true);

    // Parse the chain of metadata keys.
    std::set<std::string> KeysCommon;
    for(auto a : SplitStringToVector(KeysCommonStr, ';', 'd')){
        KeysCommon.insert(a);
    }

    if(!KeysCommon.empty()){
        auto pd = Partition_Drover(DICOM_data, KeysCommon );

        // Invoke children operations over each valid partition.
        YLOGINFO("Performing children operation(s) over " << pd.partitions.size() << " partitions");
        for(auto & d : pd.partitions){
            if(!Operation_Dispatcher(d, InvocationMetadata, FilenameLex, OptArgs.getChildren())){
                throw std::runtime_error("Child analysis failed. Cannot continue");
            }
        }

        if(IncludeNA && pd.na_partition){
            YLOGINFO("Performing children operation(s) for 'N/A' partition");
            if(!Operation_Dispatcher(pd.na_partition.value(), InvocationMetadata, FilenameLex, OptArgs.getChildren())){
                throw std::runtime_error("Child analysis failed. Cannot continue");
            }
        }

        // Combine all partitions back into a single Drover object to capture all additions/removals/modifications.
        DICOM_data = Combine_Partitioned_Drover( pd );
    }

    return true;
}

