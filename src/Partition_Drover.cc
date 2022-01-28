//Partition_Drover.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <list>
#include <set>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <memory>
#include <type_traits>

#include "Structs.h"
#include "Metadata.h"

#include "Partition_Drover.h"


// -------------------------------------------------------------------------------------------------------------------
// ------------------------------------ Key-value based metadata partitioning ----------------------------------------
// -------------------------------------------------------------------------------------------------------------------

// Split a Drover object according to the provided metadata keys.
Partitioned_Drover
Partition_Drover( Drover& DICOM_data,
                  std::set<std::string> keys_common ){

    Partitioned_Drover pd;
    pd.keys_common = keys_common;

    if(keys_common.empty()){
        pd.na_partition = DICOM_data;

    }else{
        // Cycle through all elements in the original Drover class, computing the 'key' for each.
        // Use the key's value(s) to decide which partition to add the data to.
        auto compute_keys_and_partition = [&] (auto container_of_ptrs){
            const std::string container_type = typeid(decltype(container_of_ptrs)).name();
            for(auto & ptr : container_of_ptrs){
                std::set<std::string> value_signature;
                for(const auto &key : keys_common){
                    const auto distinct_vals = Extract_Distinct_Values(ptr, key);
                    if(distinct_vals.empty()){
                        break;
                    }else if(1 < distinct_vals.size()){
                        // Strictly require all sub-object metadata in each object to be consistent.
                        FUNCWARN("Refusing to partition heterogeneous element '" << container_type << "' which contains " << distinct_vals.size() << " distinct key-values");
                        break;
                    }else{
                        value_signature.insert(*std::begin(distinct_vals));
                    }
                }

                if(value_signature.size() == keys_common.size()){
                    if(pd.index.count( value_signature ) == 0){
                        pd.partitions.emplace_back();
                        pd.index[ value_signature ] = std::prev( std::end( pd.partitions ) );
                    }
                    auto partition_it = pd.index[value_signature];
                    partition_it->Concatenate({ptr});
                }else{
                    if(!pd.na_partition) pd.na_partition.emplace();
                    pd.na_partition.value().Concatenate({ptr});
                }
            }
        };

        compute_keys_and_partition(DICOM_data.image_data);
        compute_keys_and_partition(DICOM_data.point_data);
        compute_keys_and_partition(DICOM_data.smesh_data);
        compute_keys_and_partition(DICOM_data.tplan_data);
        compute_keys_and_partition(DICOM_data.lsamp_data);
        compute_keys_and_partition(DICOM_data.trans_data);
        compute_keys_and_partition(DICOM_data.table_data);

        if(DICOM_data.Has_Contour_Data()){
            while(!DICOM_data.contour_data->ccs.empty()){
                auto it = DICOM_data.contour_data->ccs.begin();
                auto ptr = &( *it ); // Pointer to contour_collection<double> rather than whole Contour_Data.

                std::set<std::string> value_signature;
                for(const auto &key : keys_common){
                    const auto distinct_vals = Extract_Distinct_Values(ptr, key);
                    if(distinct_vals.size() != 1){ // Strictly require all sub-object metadata to be consistent.
                        break;
                    }else{
                        value_signature.insert(*std::begin(distinct_vals));
                    }
                }

                if(value_signature.size() == keys_common.size()){
                    if(pd.index.count( value_signature ) == 0){
                        pd.partitions.emplace_back();
                        pd.index[ value_signature ] = std::prev( std::end( pd.partitions ) );
                    }
                    auto partition_it = pd.index[value_signature];
                    partition_it->Ensure_Contour_Data_Allocated();
                    partition_it->contour_data->ccs.splice(
                      std::begin(partition_it->contour_data->ccs),
                      DICOM_data.contour_data->ccs, it);
                }else{
                    if(!pd.na_partition) pd.na_partition.emplace();
                    pd.na_partition.value().Ensure_Contour_Data_Allocated();
                    pd.na_partition.value().contour_data->ccs.splice(
                      std::begin( pd.na_partition.value().contour_data->ccs),
                      DICOM_data.contour_data->ccs, it);
                }
            }
        }
    }

    return pd;
}

// Re-combine a Partitioned_Drover into a regular Drover.
Drover
Combine_Partitioned_Drover( Partitioned_Drover &pd ){
    Drover DICOM_data;

    for(auto & d : pd.partitions){
        DICOM_data.Consume(d);
    }
    if(pd.na_partition){
        DICOM_data.Consume(pd.na_partition.value());
    }

    return DICOM_data;
}

// -------------------------------------------------------------------------------------------------------------------
// ---------------------------------- TPlan partitioning based on DICOM linkage --------------------------------------
// -------------------------------------------------------------------------------------------------------------------
/*
struct TPlan_Partitioned_Drover {
    std::list< Drover > partitions;
    Drover na_partition;
    Drover original_order;
};
*/

/*
TPlan_Partitioned_Drover
TPlan_Partition_Drover( Drover &d ){

    // 
}

Drover
Combine_TPlan_Partitioned_Drover( TPlan_Partitioned_Drover &pd ){
    Drover DICOM_data = original_order;

    for(auto & d : pd.partitions){
        DICOM_data.Consume(d);
    }
    DICOM_data.Consume(pd.na_partition);

    // De-duplicate since many objects are likely to be duplicated.

    // ...TODO...

    return DICOM_data;
}
*/

