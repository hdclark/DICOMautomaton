//Partition_Drover.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <list>
#include <iterator>
#include <string>
#include <utility>
#include <memory>
#include <type_traits>

#include "Structs.h"

#include "Partition_Drover.h"


// A routine that extracts metadata from each of the Drover members.
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

    // Sparse_Table.
    }else if constexpr (std::is_same_v< obj_t, Sparse_Table >){
        std::optional<std::string> val_opt = ( p->table.metadata.count(key) != 0 ) ?
                                               p->table.metadata[key] : std::optional<std::string>();
        if(val_opt) out.emplace_back(val_opt.value());
        return out;
    }

    throw std::logic_error("Type not detected properly. Refusing to continue.");
    return out;
}


// Split a Drover object according to the provided metadata keys.
Partitioned_Drover
Partition_Drover( Drover& DICOM_data,
                  std::list<std::string> keys_common ){

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
                std::list<std::string> value_signature;
                for(const auto &key : keys_common){
                    const auto uniq_vals = Extract_Unique_Metadata_Values(ptr, key);
                    if(uniq_vals.empty()){
                        break;
                    }else if(1 < uniq_vals.size()){
                        // Strictly require all metadata in each object to be consistent.
                        FUNCWARN("Refusing to partition heterogeneous element '" << container_type << "' which contains " << uniq_vals.size() << " unique values");
                        break;
                    }else{
                        value_signature.emplace_back(uniq_vals.front());
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
                    pd.na_partition.Concatenate({ptr});
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

                std::list<std::string> value_signature;
                for(const auto &key : keys_common){
                    const auto uniq_vals = Extract_Unique_Metadata_Values(ptr, key);
                    if(uniq_vals.size() != 1){ // Strictly require all metadata to be consistent.
                        break;
                    }else{
                        value_signature.emplace_back(uniq_vals.front());
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
                    pd.na_partition.Ensure_Contour_Data_Allocated();
                    pd.na_partition.contour_data->ccs.splice(
                      std::begin( pd.na_partition.contour_data->ccs),
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
    DICOM_data.Consume(pd.na_partition);

    return DICOM_data;
}

