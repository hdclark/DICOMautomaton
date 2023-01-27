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
                        YLOGWARN("Refusing to partition heterogeneous element '" << container_type << "' which contains " << distinct_vals.size() << " distinct key-values");
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
        compute_keys_and_partition(DICOM_data.rtplan_data);
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
// --------------------------------- RTPlan partitioning based on DICOM linkage --------------------------------------
// -------------------------------------------------------------------------------------------------------------------

Drover_Selection
Select_Drover( Drover &DICOM_data, std::list<std::shared_ptr<RTPlan>>::iterator tp_it ){
    Drover_Selection pd;
    pd.extras = DICOM_data;

    const auto get_first_value = [](auto ptr, const std::string &key) -> std::optional<std::string> {
        std::optional<std::string> out;

        const auto distinct_vals = Extract_Distinct_Values(ptr, key);
        if(distinct_vals.size() == 1){
            out = *(std::begin(distinct_vals));
        }
        return out;
    };
    const auto get_required_first_value = [&get_first_value](auto ptr, const std::string &key) -> std::string {
        auto out = get_first_value(ptr, key);
        if(!out) throw std::runtime_error("Required key '"_s + key + "' not available");
        return out.value();
    };

    auto tp = *tp_it; // pointer to RTPlan object.
    if(tp == nullptr){
        throw std::invalid_argument("RTPlan not accessible");
    }
    try{
        // Defer making the partition until we have all required metadata from the plan.
        //const auto tp_SOPClassUID = get_required_first_value(tp, "SOPClassUID"); // '1.2.840.10008.5.1.4.1.1.481.5'
        const auto tp_SOPInstanceUID = get_required_first_value(tp, "SOPInstanceUID");
        const auto tp_FrameOfReferenceUID = get_required_first_value(tp, "FrameOfReferenceUID");
        const auto tp_SeriesInstanceUID = get_required_first_value(tp, "SeriesInstanceUID");
        const auto tp_StudyInstanceUID = get_required_first_value(tp, "StudyInstanceUID");
        const auto tp_RTPlanLabel = get_required_first_value(tp, "RTPlanLabel");
        //const auto tp_RTPlanName = get_required_first_value(tp, "RTPlanName");

        // 'Move' the plan.
        pd.select.rtplan_data.emplace_back(tp);
        pd.extras.rtplan_data.remove(tp);

        // Look for referenced RTDOSE images.
        for(uint32_t i = 0; i < 100000; ++i){
            const std::string tp_key = "DoseReferenceSequence"_s + std::to_string(i) + "/DoseReferenceUID";
            const auto tp_DoseUID_opt = get_first_value(tp, tp_key);
            if(!tp_DoseUID_opt) break;

            for(auto &ia : DICOM_data.image_data){
                // Check if the plan references this image array.
                const auto ia_SOPInstanceUID_opt = get_first_value(ia, "SOPInstanceUID");
                if( ia_SOPInstanceUID_opt
                &&  (tp_DoseUID_opt == ia_SOPInstanceUID_opt) ){
                    pd.select.image_data.emplace_back(ia);
                    pd.extras.image_data.remove(ia);
                    continue;
                }

                // Check if this image array references the plan.
                const std::string ia_key = "ReferencedRTPlanSequence/ReferencedSOPInstanceUID";
                const auto ia_PlanUID_opt = get_first_value(ia, ia_key);
                if( ia_PlanUID_opt
                &&  (ia_PlanUID_opt == tp_SOPInstanceUID) ){
                    pd.select.image_data.emplace_back(ia);
                    pd.extras.image_data.remove(ia);
                    continue;
                }
            }
        }

        // Look for referenced RTSTRUCT contours.
        pd.select.Ensure_Contour_Data_Allocated();
        pd.extras.Ensure_Contour_Data_Allocated();
        for(uint32_t i = 0; i < 100000; ++i){
            if(!pd.extras.Has_Contour_Data()) break;

            const std::string tp_key = "ReferencedStructureSetSequence"_s + std::to_string(i) + "/ReferencedSOPInstanceUID";
            const auto tp_StructUID_opt = get_first_value(tp, tp_key);
            if(!tp_StructUID_opt) break;

            auto cc_it = std::begin(pd.extras.contour_data->ccs);
            const auto end_it = std::end(pd.extras.contour_data->ccs);
            while(cc_it != end_it){
                auto cc_ptr = &( *cc_it );

                // Check if the plan references this cc.
                const auto cc_SOPInstanceUID_opt = get_first_value(cc_ptr, "SOPInstanceUID");
                if( cc_SOPInstanceUID_opt
                &&  (tp_StructUID_opt == cc_SOPInstanceUID_opt) ){

                    // Move the cc from extras to select.
                    const auto next_it = std::next(cc_it);
                    pd.select.contour_data->ccs.splice( 
                        std::end(pd.select.contour_data->ccs),
                        pd.extras.contour_data->ccs,
                        cc_it );
                    cc_it = next_it;
                    // Do not advance, since cc_it is already the next iter.
                    continue;
                }

                ++cc_it;
            }
        }

        // Look for (indirectly) referenced images.
        for(auto &cc : pd.select.contour_data->ccs){
            // Note: it's possible that there might be suffixed nodes or '\'-separated values here.
            // For now I just ignore if there are multiples. Ideally, this should be more consistent during parsing.
            // Awaiting re-write of DICOM file handling, especially for RTSTRUCT files.  TODO.
            const std::string cc_key = "ReferencedFrameOfReferenceSequence/RTReferencedStudySequence/RTReferencedSeriesSequence/SeriesInstanceUID";
            auto cc_ptr = &( cc );
            const auto cc_SeriesUID_opt = get_first_value(cc_ptr, cc_key);
            if(!cc_SeriesUID_opt) break;
  
            auto ia_it = std::begin(pd.extras.image_data);
            const auto end_it = std::end(pd.extras.image_data);
            while(ia_it != end_it){
                auto ia_ptr = ( *ia_it );

                // Check if the cc references this image array.
                const auto ia_SeriesUID_opt = get_first_value(ia_ptr, "SeriesInstanceUID");
                if( ia_SeriesUID_opt
                &&  (cc_SeriesUID_opt == ia_SeriesUID_opt) ){
                    // Move the cc from extras to select.
                    const auto next_it = std::next(ia_it);
                    pd.select.image_data.splice( 
                        std::end(pd.select.image_data),
                        pd.extras.image_data,
                        ia_it );
                    ia_it = next_it;
                    // Do not advance, since ia_it is already the next iter.
                    continue;
                }

                ++ia_it;
            }
        }

    }catch(const std::exception &){}


    // Look for the corresponding RTDOSE images.

    // RTPLAN metadata:
    //   "DoseReferenceSequence0/DoseReferenceUID"
    //   "ReferencedStructureSetSequence0/ReferencedSOPInstanceUID"
    //   "FrameOfReferenceUID"
    //   "SeriesInstanceUID"
    //   "StudyInstanceUID"
    //   "RTPlanLabel"
    //   "RTPlanName"
    //   "SOPClassUID"  ('1.2.840.10008.5.1.4.1.1.481.5')
    //   "SOPInstanceUID"
    //
                    // (300c,0060) SQ (Sequence with explicit length #=1)      # 110, 1 ReferencedStructureSetSequence
                    //   (fffe,e000) na (Item with explicit length #=2)          # 102, 1 Item
                    //     (0008,1150) UI =RTStructureSetStorage                   #  30, 1 ReferencedSOPClassUID
                    //     (0008,1155) UI [1.2.246.352.205.4826671977035386720.1568118767464602552] #  56, 1 ReferencedSOPInstanceUID
                    //   (fffe,e00d) na (ItemDelimitationItem for re-encoding)   #   0, 0 ItemDelimitationItem
                    // (fffe,e0dd) na (SequenceDelimitationItem for re-encod.) #   0, 0 SequenceDelimitationItem


    // RTDOSE metadata:
    //         'ReferencedRTPlanSequence/ReferencedSOPClassUID' : ('1.2.840.10008.5.1.4.1.1.481.5')
    //         'ReferencedRTPlanSequence/ReferencedSOPInstanceUID'
    //         'SOPClassUID' : ('1.2.840.10008.5.1.4.1.1.481.2')
    //         'SOPInstanceUID'
    //         'SeriesInstanceUID'
    //         'StudyInstanceUID'

    // RTSTRUCT metadata:
    //           'FrameOfReferenceUID'    and/or  'ReferencedFrameOfReferenceSequence/FrameOfReferenceUID' ?
    //           'SOPClassUID' : ('1.2.840.10008.5.1.4.1.1.481.3')
    //           'SOPInstanceUID'
    //           'SeriesInstanceUID'
    //           'StudyInstanceUID'
    // 
    // (3006,0010) SQ (Sequence with explicit length #=1)      # 26354, 1 ReferencedFrameOfReferenceSequence
    //   (fffe,e000) na (Item with explicit length #=2)          # 26346, 1 Item
    //     (0020,0052) UI [1.2.840.113619.2.278.3.34220241.101.1620780463.520.14860.1] #  58, 1 FrameOfReferenceUID
    //     (3006,0012) SQ (Sequence with explicit length #=1)      # 26272, 1 RTReferencedStudySequence
    //       (fffe,e000) na (Item with explicit length #=3)          # 26264, 1 Item
    //         (0008,1150) UI =RTStructureSetStorage                   #  30, 1 ReferencedSOPClassUID
    //         (0008,1155) UI [1.2.840.113619.2.278.3.34220241.101.1620780463.519] #  50, 1 ReferencedSOPInstanceUID
    //         (3006,0014) SQ (Sequence with explicit length #=1)      # 26160, 1 RTReferencedSeriesSequence
    //           (fffe,e000) na (Item with explicit length #=2)          # 26152, 1 Item
    //             (0020,000e) UI [1.2.840.113619.2.278.3.34220241.101.1620780463.592] #  50, 1 SeriesInstanceUID
    //             (3006,0016) SQ (Sequence with explicit length #=251)    # 26086, 1 ContourImageSequence
    //               (fffe,e000) na (Item with explicit length #=2)          #  96, 1 Item
    //                 (0008,1150) UI =CTImageStorage                          #  26, 1 ReferencedSOPClassUID
    //                 (0008,1155) UI [1.2.840.113619.2.278.3.34220241.101.1620780463.593.251] #  54, 1 ReferencedSOPInstanceUID
    //               (fffe,e00d) na (ItemDelimitationItem for re-encoding)   #   0, 0 ItemDelimitationItem
    //               (fffe,e000) na (Item with explicit length #=2)          #  96, 1 Item
    //                 (0008,1150) UI =CTImageStorage                          #  26, 1 ReferencedSOPClassUID
    //                 (0008,1155) UI [1.2.840.113619.2.278.3.34220241.101.1620780463.593.250] #  54, 1 ReferencedSOPInstanceUID
    //               (fffe,e00d) na (ItemDelimitationItem for re-encoding)   #   0, 0 ItemDelimitationItem
    // ...
    // (3006,0020) SQ (Sequence with explicit length #=16)     # 1914, 1 StructureSetROISequence
    //   (fffe,e000) na (Item with explicit length #=4)          # 106, 1 Item
    //     (3006,0022) IS [33]                                     #   2, 1 ROINumber
    //     (3006,0024) UI [1.2.840.113619.2.278.3.34220241.101.1620780463.520.14860.1] #  58, 1 ReferencedFrameOfReferenceUID
    //     (3006,0026) LO [PTVEdit]                                #   8, 1 ROIName
    //     (3006,0036) CS [MANUAL]                                 #   6, 1 ROIGenerationAlgorithm
    //   (fffe,e00d) na (ItemDelimitationItem for re-encoding)   #   0, 0 ItemDelimitationItem
    // ...


    // CT metadata:
    //         'FrameOfReferenceUID' : '1.2.840.113619.2.278.3.34220241.101.1620780463.520.14860.1'
    //         'SOPClassUID' : ('1.2.840.10008.5.1.4.1.1.2')
    //         'SOPInstanceUID'
    //         'SeriesInstanceUID'
    //         'StudyInstanceUID'
    // (0008,1140) SQ (Sequence with explicit length #=1)      # 102, 1 ReferencedImageSequence
    //   (fffe,e000) na (Item with explicit length #=2)          #  94, 1 Item
    //     (0008,1150) UI =CTImageStorage                          #  26, 1 ReferencedSOPClassUID
    //     (0008,1155) UI [...] #  52, 1 ReferencedSOPInstanceUID
    //   (fffe,e00d) na (ItemDelimitationItem for re-encoding)   #   0, 0 ItemDelimitationItem
    // (fffe,e0dd) na (SequenceDelimitationItem for re-encod.) #   0, 0 SequenceDelimitationItem

    return pd;
}

Drover
Recombine_Selected_Drover( Drover_Selection pd ){
    Drover DICOM_data;
    DICOM_data.Consume(pd.select);
    DICOM_data.Consume(pd.extras);
    return DICOM_data;
}


