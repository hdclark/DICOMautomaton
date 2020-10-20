//ForEachDistinct.cc - A part of DICOMautomaton 2020. Written by hal clark.

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
#include <list>
#include <memory>
#include <type_traits>

#include <boost/filesystem.hpp>

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

#include "ForEachDistinct.h"


template <class ptr>
std::list<std::string> Extract_Unique_Metadata_Values(ptr p, const std::string &key){
    std::list<std::string> out;
    if(p == nullptr) return out;

    using obj_t = std::decay_t<decltype(*p)>;

    // Image_Array.
    if constexpr (std::is_same_v< obj_t, Image_Array >){
        const auto uniq_vals = p->imagecoll.get_distinct_values_for_key(key);
        return uniq_vals;
    
    // Contour_Data (actually contour_collection<double>).
    //
    // NOTE: accessing Contour_Data elements is different from other Drover elements!
    }else if constexpr (std::is_same_v< obj_t, contour_collection<double> >){
        const auto uniq_vals = p->get_distinct_values_for_key(key);
        return uniq_vals;

    // Point_Cloud.
    }else if constexpr (std::is_same_v< obj_t, Point_Cloud >){
        std::optional<std::string> val_opt = ( p->pset.metadata.count(key) != 0 ) ?
                                               p->pset.metadata[key] : std::optional<std::string>();
        if(val_opt) out.emplace_back(val_opt.value());
        return out;

    // Surface_Mesh.
    }else if constexpr (std::is_same_v< obj_t, Surface_Mesh >){
        std::optional<std::string> val_opt = ( p->meshes.metadata.count(key) != 0 ) ?
                                               p->meshes.metadata[key] : std::optional<std::string>();
        if(val_opt) out.emplace_back(val_opt.value());
        return out;

    // TPlan_Config.
    }else if constexpr (std::is_same_v< obj_t, TPlan_Config >){
        std::optional<std::string> val_opt = ( p->metadata.count(key) != 0 ) ?
                                               p->metadata[key] : std::optional<std::string>();
        if(val_opt) out.emplace_back(val_opt.value());
        return out;

    // Line_Sample.
    }else if constexpr (std::is_same_v< obj_t, Line_Sample >){
        std::optional<std::string> val_opt = ( p->line.metadata.count(key) != 0 ) ?
                                               p->line.metadata[key] : std::optional<std::string>();
        if(val_opt) out.emplace_back(val_opt.value());
        return out;

    // Transform3.
    }else if constexpr (std::is_same_v< obj_t, Transform3 >){
        std::optional<std::string> val_opt = ( p->metadata.count(key) != 0 ) ?
                                               p->metadata[key] : std::optional<std::string>();
        if(val_opt) out.emplace_back(val_opt.value());
        return out;
    }

    throw std::logic_error("Type not detected properly. Refusing to continue.");
    return out;
}


OperationDoc OpArgDocForEachDistinct() {
    OperationDoc out;
    out.name = "ForEachDistinct";

    out.desc = "This operation is a control flow meta-operation that partitions all available data and invokes all"
               " child operations once for each distinct partition.";

    out.notes.emplace_back(
        "If this operation has no children, this operation will evaluate to a no-op."
    );
    out.notes.emplace_back(
        "Each invocation is performed sequentially, and all side-effects are carried forward for each iteration."
        " However, partitions are generated before any child operations are invoked, so newly-added elements (e.g.,"
        " new Image_Arrays) created by one invocation will not participate in subsequent invocations."
        " The final order of the partitions is arbitrary."
    );
    out.notes.emplace_back(
        " This operation will most often be used to process data group-wise rather than as a whole."
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

    return out;
}

Drover ForEachDistinct(Drover DICOM_data,
              const OperationArgPkg& OptArgs,
              const std::map<std::string, std::string>& InvocationMetadata,
              const std::string& FilenameLex){
    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto KeysCommonStr = OptArgs.getValueStr("KeysCommon").value();

    //-----------------------------------------------------------------------------------------------------------------

    // Parse the chain of metadata keys.
    std::list<std::string> KeysCommon;
    for(auto a : SplitStringToVector(KeysCommonStr, ';', 'd')){
        KeysCommon.emplace_back(a);
    }

    if(!KeysCommon.empty()){
        // Grouping data structures.
        std::map<std::list<std::string>, Drover> partitions;
        Drover na_partition;

        // Cycle through all elements in the original Drover class, computing the 'key' for each.
        // Use the key's value(s) to decide which partition to add the data to.
        auto compute_keys_and_partition = [&] (auto container_of_ptrs){
            for(auto & ptr : container_of_ptrs){
                std::list<std::string> value_signature;
                for(const auto &key : KeysCommon){
                    const auto uniq_vals = Extract_Unique_Metadata_Values(ptr, key);
                    if(uniq_vals.size() != 1){ // Strictly require all metadata to be consistent.
                        break;
                    }else{
                        value_signature.emplace_back(uniq_vals.front());
                    }
                }
                //FUNCWARN("The lookup for " << typeid(decltype(container_of_ptrs)).name() << " resulted in " << value_signature.size() << " unique values");
                if(value_signature.size() == KeysCommon.size()){
                    partitions[value_signature].Concatenate({ptr});
                }else{
                    na_partition.Concatenate({ptr});
                }
            }
        };

        compute_keys_and_partition(DICOM_data.image_data);
        compute_keys_and_partition(DICOM_data.point_data);
        compute_keys_and_partition(DICOM_data.smesh_data);
        compute_keys_and_partition(DICOM_data.tplan_data);
        compute_keys_and_partition(DICOM_data.lsamp_data);
        compute_keys_and_partition(DICOM_data.trans_data);

        if(DICOM_data.Has_Contour_Data()){
            while(!DICOM_data.contour_data->ccs.empty()){
                auto it = DICOM_data.contour_data->ccs.begin();
                auto ptr = &( *it ); // Pointer to contour_collection<double> rather than whole Contour_Data.

                std::list<std::string> value_signature;
                for(const auto &key : KeysCommon){
                    const auto uniq_vals = Extract_Unique_Metadata_Values(ptr, key);
                    if(uniq_vals.size() != 1){ // Strictly require all metadata to be consistent.
                        break;
                    }else{
                        value_signature.emplace_back(uniq_vals.front());
                    }
                }

                if(value_signature.size() == KeysCommon.size()){
                    partitions[value_signature].Ensure_Contour_Data_Allocated();
                    partitions[value_signature].contour_data->ccs.splice( std::begin(partitions[value_signature].contour_data->ccs),
                        DICOM_data.contour_data->ccs, it);
                }else{
                    na_partition.Ensure_Contour_Data_Allocated();
                    na_partition.contour_data->ccs.splice( std::begin( na_partition.contour_data->ccs),
                        DICOM_data.contour_data->ccs, it);
                }
            }
        }

        // Invoke children operations over each valid partition.
        FUNCINFO("Performing children operations over " << partitions.size() << " partitions (+1 'N/A' partition)");
        for(auto & p : partitions){
            if(!Operation_Dispatcher(p.second, InvocationMetadata, FilenameLex, OptArgs.getChildren())){
                throw std::runtime_error("Child analysis failed. Cannot continue");
            }
        }

        // Combine all partitions back into a single Drover object to capture all additions/removals/modifications.
        DICOM_data = Drover();
        for(auto & p : partitions){
            DICOM_data.Consume(p.second);
        }
        DICOM_data.Consume(na_partition);
    }

    return DICOM_data;
}

