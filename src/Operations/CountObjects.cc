//CountObjects.cc - A part of DICOMautomaton 2025. Written by hal clark.

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

#include "CountObjects.h"


OperationDoc OpArgDocCountObjects() {
    OperationDoc out;
    out.name = "CountObjects";

    out.tags.emplace_back("category: meta");

    out.desc = "This operation is a meta-operation that counts the number of selected objects and stores the result"
               " in the global parameter table. It can be used to control for loops and test for the presence of"
               " data to make conditional program flows.";

    out.notes.emplace_back(
        "Multiple selection criteria can be provided. If multiple criteria are specified, this operation"
        " returns the total number of objects selected."
    );
    out.notes.emplace_back(
        "This operation is read-only and produces no side-effects. It does not alter the selection."
    );
    out.notes.emplace_back(
        "Selectors for this operation are only considered when you explicitly provide them."
        " The default values are not used by this operation."
    );

    out.args.emplace_back();
    out.args.back().name = "Key";
    out.args.back().desc = "The key used to insert the object count into the key-value global parameter table.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "count", "N", "x" };

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
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";
    out.args.back().expected = false;

    return out;
}

bool CountObjects(Drover &DICOM_data,
                  const OperationArgPkg& OptArgs,
                  std::map<std::string, std::string>& InvocationMetadata,
                  const std::string& FilenameLex){
    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto Key = OptArgs.getValueStr("Key").value();

    const auto NormalizedROILabelRegexOpt = OptArgs.getValueStr("NormalizedROILabelRegex");
    const auto ROILabelRegexOpt = OptArgs.getValueStr("ROILabelRegex");
    const auto ROISelectionOpt = OptArgs.getValueStr("ROISelection");

    const auto ImageSelectionOpt = OptArgs.getValueStr("ImageSelection");

    const auto LineSelectionOpt = OptArgs.getValueStr("LineSelection");

    const auto MeshSelectionOpt = OptArgs.getValueStr("MeshSelection");

    const auto PointSelectionOpt = OptArgs.getValueStr("PointSelection");

    const auto TableSelectionOpt = OptArgs.getValueStr("TableSelection");
    //-----------------------------------------------------------------------------------------------------------------

    int64_t selectors_present = 0;
    int64_t contour_selectors = 0;
    int64_t objects = 0;

    // Note: contours are handled differently here compared to most other operations in order to separate selection from 
    // normalized vs raw names.
    if(NormalizedROILabelRegexOpt){
        auto cc_all = All_CCs( DICOM_data );
        auto cc_ROIs = Whitelist( cc_all, { { "NormalizedROIName", NormalizedROILabelRegexOpt.value() } } );
        YLOGINFO("Selected " << cc_ROIs.size() << " contours using NormalizedROILabelRegex selector");

        ++selectors_present;
        ++contour_selectors;
        objects += static_cast<int64_t>( cc_ROIs.size() );
    }

    if(ROILabelRegexOpt){
        auto cc_all = All_CCs( DICOM_data );
        auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegexOpt.value() } } );
        YLOGINFO("Selected " << cc_ROIs.size() << " contours using ROILabelRegex selector");

        ++selectors_present;
        ++contour_selectors;
        objects += static_cast<int64_t>( cc_ROIs.size() );
    }

    if(ROISelectionOpt){
        auto cc_all = All_CCs( DICOM_data );
        auto cc_ROIs = Whitelist( cc_all, ROISelectionOpt.value() );
        YLOGINFO("Selected " << cc_ROIs.size() << " contours using ROISelection selector");

        ++selectors_present;
        ++contour_selectors;
        objects += static_cast<int64_t>( cc_ROIs.size() );
    }

    if(1 < contour_selectors){
        throw std::invalid_argument("Multiple contour selectors are not currently supported");
    }

    if(ImageSelectionOpt){
        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionOpt.value() );
        YLOGINFO("Selected " << IAs.size() << " image arrays using ImageSelection selector");

        ++selectors_present;
        objects += static_cast<int64_t>( IAs.size() );
    }

    if(LineSelectionOpt){
        auto LSs_all = All_LSs( DICOM_data );
        auto LSs = Whitelist( LSs_all, LineSelectionOpt.value() );
        YLOGINFO("Selected " << LSs.size() << " line samples using LineSelection selector");

        ++selectors_present;
        objects += static_cast<int64_t>( LSs.size() );
    }

    if(MeshSelectionOpt){
        auto SMs_all = All_SMs( DICOM_data );
        auto SMs = Whitelist( SMs_all, MeshSelectionOpt.value() );
        YLOGINFO("Selected " << SMs.size() << " surface meshes using MeshSelection selector");

        ++selectors_present;
        objects += static_cast<int64_t>( SMs.size() );
    }

    if(PointSelectionOpt){
        auto PCs_all = All_PCs( DICOM_data );
        auto PCs = Whitelist( PCs_all, PointSelectionOpt.value() );
        YLOGINFO("Selected " << PCs.size() << " point clouds using PointSelection selector");

        ++selectors_present;
        objects += static_cast<int64_t>( PCs.size() );
    }

    if(TableSelectionOpt){
        auto STs_all = All_STs( DICOM_data );
        auto STs = Whitelist( STs_all, TableSelectionOpt.value() );
        YLOGINFO("Selected " << STs.size() << " tables using TableSelection selector");

        ++selectors_present;
        objects += static_cast<int64_t>( STs.size() );
    }

    if(selectors_present == 0){
        throw std::invalid_argument("No selectors provided, nothing to evaluate");
    }

    InvocationMetadata[Key] = std::to_string(objects);
    return true;
}

