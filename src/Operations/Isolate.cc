//Isolate.cc - A part of DICOMautomaton 2023. Written by hal clark.

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
#include "../Regex_Selectors.h"
#include "../Operation_Dispatcher.h"

#include "Isolate.h"

template <class T>
void
implement_additions_and_deletions( T &mainline,
                                   const T &snapshot,
                                   const T &current ){
    // This function updates a 'mainline' object by creating or deleting objects.
    // To detect whether an object was created/deleted, a prior 'snapshot' is compared with a 'current' object.
    // Objects are then moved to or removed from the mainline object to reflect the changes.

    if(snapshot.empty() && current.empty()) return;

    using value_T = typename T::value_type;
    std::list<value_T> sorted_snapshot( std::begin(snapshot), std::end(snapshot) );
    std::list<value_T> sorted_current(  std::begin(current),  std::end(current) );
    sorted_snapshot.sort();
    sorted_current.sort();

    // Get list of deleted objects.
    std::list<value_T> objects_deleted;
    std::set_difference( std::begin(sorted_snapshot), std::end(sorted_snapshot),
                         std::begin(sorted_current),  std::end(sorted_current),
                         std::back_inserter(objects_deleted) );

    // Get list of created objects.
    std::list<value_T> objects_created;
    std::set_difference( std::begin(sorted_current),  std::end(sorted_current),
                         std::begin(sorted_snapshot), std::end(sorted_snapshot),
                         std::back_inserter(objects_created) );

    // Implement the changes.
    for(const auto& o : objects_deleted){
        mainline.remove(o);
    }
    for(const auto& o : objects_created){
        mainline.emplace_back(o);
    }

    return;
}

OperationDoc OpArgDocIsolate() {
    OperationDoc out;
    out.name = "Isolate";

    out.desc = "This operation is a control flow meta-operation that selects only the specified objects (e.g., image"
               " arrays, surface meshes, etc.) and forwards them to a virtual Drover object where they are isolated"
               " from the objects that were not selected. This creates a 'view' of the Drover object containing only"
               " the selected objects. Children operations will operate on this isolated view where the unselected"
               " objects are hidden."
               "\n\n"
               "This operation is useful for implementing complicated filters. For example, selecting"
               " the third and seventh image arrays will present a view with *only* those two image arrays (in the"
               " first and second positions). Children operations can then address (only) those image arrays in the"
               " first and second place, rather than third and seventh place. When the children operations conclude,"
               " these image arrays would be returned to the third and seventh position in the original Drover.";

    out.notes.emplace_back(
        "This operation itself produces no side-effects. It does not alter the selected objects."
        " However, children operations may alter the selected objects and/or create side-effects."
    );
    out.notes.emplace_back(
        "The order of objects in the original Drover is retained when this operation concludes."
        " Objects deleted within the isolated view are also deleted from the original Drover object."
        " Objects created within the isolated view are inserted at the end of the original Drover object."
    );
    out.notes.emplace_back(
        "Selectors for this operation are only considered when you explicitly provide them."
        " By default, this operation will provide an empty view."
    );

    //out.args.emplace_back();
    //out.args.back() = RCWhitelistOpArgDoc();
    //out.args.back().name = "ROILabelRegex";
    //out.args.back().default_val = ".*";
    //out.args.back().expected = false;

    //out.args.emplace_back();
    //out.args.back() = NCWhitelistOpArgDoc();
    //out.args.back().name = "NormalizedROILabelRegex";
    //out.args.back().default_val = ".*";
    //out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
    out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = LSWhitelistOpArgDoc();
    out.args.back().name = "LineSelection";
    out.args.back().default_val = "last";
    out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";
    out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "PointSelection";
    out.args.back().default_val = "last";
    out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = T3WhitelistOpArgDoc();
    out.args.back().name = "TransformSelection";
    out.args.back().default_val = "last";
    out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";
    out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = TPWhitelistOpArgDoc();
    out.args.back().name = "RTPlanSelection";
    out.args.back().default_val = "last";
    out.args.back().expected = false;

    return out;
}

bool Isolate(Drover &DICOM_data,
                        const OperationArgPkg& OptArgs,
                        std::map<std::string, std::string>& InvocationMetadata,
                        const std::string& FilenameLex){
    //---------------------------------------------- User Parameters --------------------------------------------------
    //const auto NormalizedROILabelRegexOpt = OptArgs.getValueStr("NormalizedROILabelRegex");
    //const auto ROILabelRegexOpt = OptArgs.getValueStr("ROILabelRegex");

    const auto ImageSelectionOpt = OptArgs.getValueStr("ImageSelection");

    const auto LineSelectionOpt = OptArgs.getValueStr("LineSelection");

    const auto MeshSelectionOpt = OptArgs.getValueStr("MeshSelection");

    const auto PointSelectionOpt = OptArgs.getValueStr("PointSelection");

    const auto TransSelectionOpt = OptArgs.getValueStr("TransformSelection");

    const auto TableSelectionOpt = OptArgs.getValueStr("TableSelection");

    const auto RTPlanSelectionOpt = OptArgs.getValueStr("RTPlanSelection");
    //-----------------------------------------------------------------------------------------------------------------
    DICOM_data.Ensure_Contour_Data_Allocated();

// TODO: can you figure out a way to support *reordering* ?
// Maybe with some sort of doubly-linked list where each object knows it's neighbours, but then you re-order based on
// the final Drover object???

    // Build the isolated Drover in such a way that the original Drover can be reassembled, even in the presence of
    // additions and deletions.
    Drover isolated;
    isolated.Ensure_Contour_Data_Allocated();

//    std::list<std::pair<std::string, std::string>> cc_meta_regex;
//    if(NormalizedROILabelRegexOpt){
//        cc_meta_regex.emplace_back( std::make_pair<std::string, std::string>("NormalizedROIName", std::string(NormalizedROILabelRegexOpt.value())) );
//    }
//    if(ROILabelRegexOpt){
//        cc_meta_regex.emplace_back( std::make_pair<std::string, std::string>("ROIName", std::string(ROILabelRegexOpt.value())) );
//    }
//    if(!cc_meta_regex.empty()){
//        auto cc_all = All_CCs( DICOM_data );
//        auto cc_ROIs = Whitelist( cc_all, cc_meta_regex );
//        YLOGINFO("Selected " << cc_ROIs.size() << " contour collections using ROILabelRegex/NormalizedROILabelRegex selectors");
//
//// TODO:
//// contour collections cannot be shared like other objects, so we need to *move* the selected ccs to the new drover.
//// std::list<std::reference_wrapper<contour_collection<double>>>
//// TODO
////        for(const auto& x_it_ptr : cc_ROIs) isolated.contour_data.push_back( *x_it_ptr );
//    }

    if(ImageSelectionOpt){
        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionOpt.value() );
        YLOGINFO("Selected " << IAs.size() << " image arrays using ImageSelection selector");

        for(const auto& x_it_ptr : IAs) isolated.image_data.push_back( *x_it_ptr );
    }

    if(PointSelectionOpt){
        auto PCs_all = All_PCs( DICOM_data );
        auto PCs = Whitelist( PCs_all, PointSelectionOpt.value() );
        YLOGINFO("Selected " << PCs.size() << " point clouds using PointSelection selector");

        for(const auto& x_it_ptr : PCs) isolated.point_data.push_back( *x_it_ptr );
    }

    if(MeshSelectionOpt){
        auto SMs_all = All_SMs( DICOM_data );
        auto SMs = Whitelist( SMs_all, MeshSelectionOpt.value() );
        YLOGINFO("Selected " << SMs.size() << " surface meshes using MeshSelection selector");

        for(const auto& x_it_ptr : SMs) isolated.smesh_data.push_back( *x_it_ptr );
    }

    if(RTPlanSelectionOpt){
        auto TPs_all = All_TPs( DICOM_data );
        auto TPs = Whitelist( TPs_all, RTPlanSelectionOpt.value() );
        YLOGINFO("Selected " << TPs.size() << " tables using RTPlanSelection selector");

        for(const auto& x_it_ptr : TPs) isolated.rtplan_data.push_back( *x_it_ptr );
    }

    if(LineSelectionOpt){
        auto LSs_all = All_LSs( DICOM_data );
        auto LSs = Whitelist( LSs_all, LineSelectionOpt.value() );
        YLOGINFO("Selected " << LSs.size() << " line samples using LineSelection selector");

        for(const auto& x_it_ptr : LSs) isolated.lsamp_data.push_back( *x_it_ptr );
    }

    if(TransSelectionOpt){
        auto T3s_all = All_T3s( DICOM_data );
        auto T3s = Whitelist( T3s_all, TransSelectionOpt.value() );
        YLOGINFO("Selected " << T3s.size() << " tables using TransSelection selector");

        for(const auto& x_it_ptr : T3s) isolated.trans_data.push_back( *x_it_ptr );
    }

    if(TableSelectionOpt){
        auto STs_all = All_STs( DICOM_data );
        auto STs = Whitelist( STs_all, TableSelectionOpt.value() );
        YLOGINFO("Selected " << STs.size() << " tables using TableSelection selector");

        for(const auto& x_it_ptr : STs) isolated.table_data.push_back( *x_it_ptr );
    }

    // Make a copy of the isolated objects which we can later use to track additions and deletions.
    // NOTE: contours should be removed before this step, then re-added afterward.
    Drover isolated_orig(isolated);

    // Execute children operations.
    const bool ret = Operation_Dispatcher(isolated, InvocationMetadata, FilenameLex, OptArgs.getChildren());

    
    // TODO: contours.
    //implement_additions_and_deletions(DICOM_data.contour_data, isolated_orig.contour_data, isolated.contour_data);
    implement_additions_and_deletions(DICOM_data.image_data, isolated_orig.image_data, isolated.image_data);
    implement_additions_and_deletions(DICOM_data.point_data, isolated_orig.point_data, isolated.point_data);
    implement_additions_and_deletions(DICOM_data.smesh_data, isolated_orig.smesh_data, isolated.smesh_data);
    implement_additions_and_deletions(DICOM_data.rtplan_data, isolated_orig.rtplan_data, isolated.rtplan_data);
    implement_additions_and_deletions(DICOM_data.lsamp_data, isolated_orig.lsamp_data, isolated.lsamp_data);
    implement_additions_and_deletions(DICOM_data.trans_data, isolated_orig.trans_data, isolated.trans_data);
    implement_additions_and_deletions(DICOM_data.table_data, isolated_orig.table_data, isolated.table_data);


    // Pass along the return the status of the children operations.
    return ret;
}

