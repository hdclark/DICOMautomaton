//PromoteMetadata.cc - A part of DICOMautomaton 2023. Written by hal clark.

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

#include "PromoteMetadata.h"


OperationDoc OpArgDocPromoteMetadata() {
    OperationDoc out;
    out.name = "PromoteMetadata";

    out.desc = "This operation can be used to copy metadata from an object to the global parameter table."
               "\n\n"
               "Metadata in the global parameter table is accessible in most operations where metadata can be assigned"
               " to objects, so this operation effectively allows one to copy metadata from one object to another.";

    out.notes.emplace_back(
        "Metadata can be copied from any selected object, regardless of the modality or type of object."
    );
    out.notes.emplace_back(
        "Composite objects can have different metadata for each sub-object. For example, image arrays are composed"
        " of multiple images, and each image can have it's own metadata (e.g., ImagePosition or SliceNumber)."
        " How multiple distinct metadata values are handled can be adjusted."
    );
    out.notes.emplace_back(
        "Selectors for this operation are only considered when you explicitly provide them."
        " By default, this operation will not select any objects."
    );
    out.notes.emplace_back(
        "This operation will succeed only if a metadata key-value is written to the global parameter table."
        " If no objects are selected or no metadata is found, the specified key will be removed from the"
        " table."
    );

    out.args.emplace_back();
    out.args.back().name = "KeySelection";
    out.args.back().desc = "The key to extract from the key-value metadata."
                           " The corresponding value will be extracted to the global parameter table.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "ROIName", "SliceThickness", "xyz" };

    out.args.emplace_back();
    out.args.back().name = "NewKey";
    out.args.back().desc = "The key to assign the metadata value when it is stored in the global parameter table."
                           "\n\n"
                           "An existing metadata key-value with the given key will be overwritten if the promotion is"
                           " successful."
                           "\n\n"
                           "Note that any existing key will initially be removed, and only replaced if the promotion"
                           " is successful.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "extracted_ROIName", "xyz_from_contours" };

    out.args.emplace_back();
    out.args.back().name = "DefaultValue";
    out.args.back().desc = "A value that will be inserted if no items are selected or no metadata is available."
                           " Omitting this parameter will disable promotion when no metadata are available."
                           "\n\n"
                           "Note that insertion of a default value will still result in the operation signalling"
                           " a failure to promote.";
    out.args.back().default_val = "N/A";
    out.args.back().expected = false;
    out.args.back().examples = { "N/A", "(missing)", "NIL" };

    out.args.emplace_back();
    out.args.back().name = "ValueSeparator";
    out.args.back().desc = "If multiple distinct metadata values are present, they will be combined together with"
                           " this separator. Providing an empty separator will disable concatenation and only one value"
                           " (the last sorted value) will be promoted.";
    out.args.back().default_val = R"***(\)***";
    out.args.back().expected = false;
    out.args.back().examples = { R"***(\)***", "", ",", "\t" };

    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";
    out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";
    out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = CCWhitelistOpArgDoc();
    out.args.back().name = "ROISelection";
    out.args.back().default_val = "all";
    out.args.back().expected = false;

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

bool PromoteMetadata(Drover &DICOM_data,
                     const OperationArgPkg& OptArgs,
                     std::map<std::string, std::string>& InvocationMetadata,
                     const std::string& FilenameLex){
    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto KeySelection = OptArgs.getValueStr("KeySelection").value();
    const auto NewKey = OptArgs.getValueStr("NewKey").value();
    const auto ValueSeparator = OptArgs.getValueStr("ValueSeparator").value_or(R"***(\)***");
    const auto DefaultValueOpt = OptArgs.getValueStr("DefaultValue");

    const auto NormalizedROILabelRegexOpt = OptArgs.getValueStr("NormalizedROILabelRegex");
    const auto ROILabelRegexOpt = OptArgs.getValueStr("ROILabelRegex");
    const auto ROISelectionOpt = OptArgs.getValueStr("ROISelection");

    const auto ImageSelectionOpt = OptArgs.getValueStr("ImageSelection");

    const auto LineSelectionOpt = OptArgs.getValueStr("LineSelection");

    const auto MeshSelectionOpt = OptArgs.getValueStr("MeshSelection");

    const auto PointSelectionOpt = OptArgs.getValueStr("PointSelection");

    const auto TransSelectionOpt = OptArgs.getValueStr("TransformSelection");

    const auto TableSelectionOpt = OptArgs.getValueStr("TableSelection");

    const auto RTPlanSelectionOpt = OptArgs.getValueStr("RTPlanSelection");
    //-----------------------------------------------------------------------------------------------------------------

    // Inserts the key-value's value, but only if there is a single unique value extracted.
    InvocationMetadata.erase(NewKey);
    std::set<std::string> values;

    const auto ingest = [&](const std::set<std::string> &l_values){
        for(const auto &v : l_values){
            values.insert(v);
        }
        return;
    };

    if( ROILabelRegexOpt
    ||  NormalizedROILabelRegexOpt
    ||  ROISelectionOpt ){
        auto CCs_all = All_CCs( DICOM_data );
        auto CCs = Whitelist( CCs_all, ROILabelRegexOpt, NormalizedROILabelRegexOpt, ROISelectionOpt );
        YLOGINFO("Selected " << CCs.size() << " contour ROIs using selector");

        for(const auto& cc_refw : CCs) ingest( Extract_Distinct_Values( &(cc_refw.get()), KeySelection ) );
    }

    if(ImageSelectionOpt){
        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionOpt.value() );
        YLOGINFO("Selected " << IAs.size() << " image arrays using ImageSelection selector");

        for(const auto& x_it_ptr : IAs) ingest( Extract_Distinct_Values( *x_it_ptr, KeySelection ) );
    }

    if(PointSelectionOpt){
        auto PCs_all = All_PCs( DICOM_data );
        auto PCs = Whitelist( PCs_all, PointSelectionOpt.value() );
        YLOGINFO("Selected " << PCs.size() << " point clouds using PointSelection selector");

        for(const auto& x_it_ptr : PCs) ingest( Extract_Distinct_Values( *x_it_ptr, KeySelection ) );
    }

    if(MeshSelectionOpt){
        auto SMs_all = All_SMs( DICOM_data );
        auto SMs = Whitelist( SMs_all, MeshSelectionOpt.value() );
        YLOGINFO("Selected " << SMs.size() << " surface meshes using MeshSelection selector");

        for(const auto& x_it_ptr : SMs) ingest( Extract_Distinct_Values( *x_it_ptr, KeySelection ) );
    }

    if(RTPlanSelectionOpt){
        auto TPs_all = All_TPs( DICOM_data );
        auto TPs = Whitelist( TPs_all, RTPlanSelectionOpt.value() );
        YLOGINFO("Selected " << TPs.size() << " tables using RTPlanSelection selector");

        for(const auto& x_it_ptr : TPs) ingest( Extract_Distinct_Values( *x_it_ptr, KeySelection ) );
    }

    if(LineSelectionOpt){
        auto LSs_all = All_LSs( DICOM_data );
        auto LSs = Whitelist( LSs_all, LineSelectionOpt.value() );
        YLOGINFO("Selected " << LSs.size() << " line samples using LineSelection selector");

        for(const auto& x_it_ptr : LSs) ingest( Extract_Distinct_Values( *x_it_ptr, KeySelection ) );
    }

    if(TransSelectionOpt){
        auto T3s_all = All_T3s( DICOM_data );
        auto T3s = Whitelist( T3s_all, TransSelectionOpt.value() );
        YLOGINFO("Selected " << T3s.size() << " tables using TransSelection selector");

        for(const auto& x_it_ptr : T3s) ingest( Extract_Distinct_Values( *x_it_ptr, KeySelection ) );
    }

    if(TableSelectionOpt){
        auto STs_all = All_STs( DICOM_data );
        auto STs = Whitelist( STs_all, TableSelectionOpt.value() );
        YLOGINFO("Selected " << STs.size() << " tables using TableSelection selector");

        for(const auto& x_it_ptr : STs) ingest( Extract_Distinct_Values( *x_it_ptr, KeySelection ) );
    }

    bool metadata_was_promoted = false;
    if(!values.empty()){
        metadata_was_promoted = true;

        std::string val;
        for(const auto &v : values){
            val = (val.empty() || ValueSeparator.empty()) ? v : (val + ValueSeparator + v);
        }
        InvocationMetadata[NewKey] = val;

    }else if( values.empty()
          &&  DefaultValueOpt ){
        InvocationMetadata[NewKey] = DefaultValueOpt.value();
    }
    return metadata_was_promoted;
}

