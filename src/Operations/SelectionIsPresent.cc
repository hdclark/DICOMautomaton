//SelectionIsPresent.cc - A part of DICOMautomaton 2022. Written by hal clark.

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

#include "SelectionIsPresent.h"


OperationDoc OpArgDocSelectionIsPresent() {
    OperationDoc out;
    out.name = "SelectionIsPresent";

    out.desc = "This operation is a control flow meta-operation that evaluates whether the provided selection"
               " criteria selects one or more objects."
               "\n\n"
               "For example, if you need to know whether there is an image array"
               " with a given metadata key-value pair, this operation will return a logical 'true' if and only"
               " if the image array can be located. This operation allows for branching logic, where operations"
               " will be taken only when data is (or is not) available.";

    out.notes.emplace_back(
        "Multiple selection criteria can be provided. If multiple criteria are specified, this operation"
        " returns the logical 'AND' for each selection criteria (e.g., has images AND has contours)."
        " If no selection criteria are provided, this operation fails therefore evaluates logically to false."
    );
    out.notes.emplace_back(
        "This operation is read-only and produces no side-effects. It does not alter the selection."
    );
    out.notes.emplace_back(
        "Selectors for this operation are only considered when you explicitly provide them."
        " The default values are not used by this operation."
    );
    out.notes.emplace_back(
        "Note that many operations will tolerate empty selections, degrading to a no-op."
        " This operation is useful as a side-effect-free option for operations that do not tolerate empty selections."
    );

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";
    out.args.back().expected = false;

    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";
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
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";
    out.args.back().expected = false;

    return out;
}

bool SelectionIsPresent(Drover &DICOM_data,
                        const OperationArgPkg& OptArgs,
                        std::map<std::string, std::string>& InvocationMetadata,
                        const std::string& FilenameLex){
    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto NormalizedROILabelRegexOpt = OptArgs.getValueStr("NormalizedROILabelRegex");
    const auto ROILabelRegexOpt = OptArgs.getValueStr("ROILabelRegex");

    const auto ImageSelectionOpt = OptArgs.getValueStr("ImageSelection");

    const auto LineSelectionOpt = OptArgs.getValueStr("LineSelection");

    const auto MeshSelectionOpt = OptArgs.getValueStr("MeshSelection");

    const auto PointSelectionOpt = OptArgs.getValueStr("PointSelection");

    const auto TableSelectionOpt = OptArgs.getValueStr("TableSelection");
    //-----------------------------------------------------------------------------------------------------------------

    int64_t selectors_present = 0;
    int64_t selection_present = 0;

    // Note: contours are handled differently here compared to most other operations in order to separate selection from 
    // normalized vs raw names.
    if(NormalizedROILabelRegexOpt){
        auto cc_all = All_CCs( DICOM_data );
        auto cc_ROIs = Whitelist( cc_all, { { "NormalizedROIName", NormalizedROILabelRegexOpt.value() } } );
        YLOGINFO("Selected " << cc_ROIs.size() << " contours using NormalizedROILabelRegex selector");

        ++selectors_present;
        if(!cc_ROIs.empty()) ++selection_present;
    }

    if(ROILabelRegexOpt){
        auto cc_all = All_CCs( DICOM_data );
        auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegexOpt.value() } } );
        YLOGINFO("Selected " << cc_ROIs.size() << " contours using ROILabelRegex selector");

        ++selectors_present;
        if(!cc_ROIs.empty()) ++selection_present;
    }

    if(ImageSelectionOpt){
        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionOpt.value() );
        YLOGINFO("Selected " << IAs.size() << " image arrays using ImageSelection selector");

        ++selectors_present;
        if(!IAs.empty()) ++selection_present;
    }

    if(LineSelectionOpt){
        auto LSs_all = All_LSs( DICOM_data );
        auto LSs = Whitelist( LSs_all, LineSelectionOpt.value() );
        YLOGINFO("Selected " << LSs.size() << " line samples using LineSelection selector");

        ++selectors_present;
        if(!LSs.empty()) ++selection_present;
    }

    if(MeshSelectionOpt){
        auto SMs_all = All_SMs( DICOM_data );
        auto SMs = Whitelist( SMs_all, MeshSelectionOpt.value() );
        YLOGINFO("Selected " << SMs.size() << " surface meshes using MeshSelection selector");

        ++selectors_present;
        if(!SMs.empty()) ++selection_present;
    }

    if(PointSelectionOpt){
        auto PCs_all = All_PCs( DICOM_data );
        auto PCs = Whitelist( PCs_all, PointSelectionOpt.value() );
        YLOGINFO("Selected " << PCs.size() << " point clouds using PointSelection selector");

        ++selectors_present;
        if(!PCs.empty()) ++selection_present;
    }

    if(TableSelectionOpt){
        auto STs_all = All_STs( DICOM_data );
        auto STs = Whitelist( STs_all, TableSelectionOpt.value() );
        YLOGINFO("Selected " << STs.size() << " tables using TableSelection selector");

        ++selectors_present;
        if(!STs.empty()) ++selection_present;
    }

    if(selectors_present == 0){
        throw std::invalid_argument("No selectors provided, nothing to evaluate");
    }
    return (selectors_present == selection_present);
}

