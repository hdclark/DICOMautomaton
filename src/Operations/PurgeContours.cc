//PurgeContours.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <optional>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "PurgeContours.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocPurgeContours(){
    OperationDoc out;
    out.name = "PurgeContours";
    out.desc = 
        "This routine purges (deletes) individual contours if they satisfy various criteria.";
        
    out.notes.emplace_back(
        "This operation considers only individual contours at the moment. It could be extended to operate on whole"
        " ROIs (i.e., contour_collections), or to perform a separate vote within each ROI. The individual contour"
        " approach was taken since filtering out small contour 'islands' is the primary use-case."
    );


    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";


    out.args.emplace_back();
    out.args.back().name = "InvertLogic";
    out.args.back().desc = "This option controls whether purged contours *should* or *should not* satisfy the specified"
                           " logical test criteria."
                           " If false (the default), this operation is equivalent to a 'purge if and only if' operation."
                           " If true, this operation is equivalent to a 'retain if and only if' operation."
                           " Note that this parameter is independent of the ROI selection criteria.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };


    out.args.emplace_back();
    out.args.back().name = "InvertSelection";
    out.args.back().desc = "This option controls whether purged contours *should* or *should not* satisfy the specified"
                           " ROI selection criteria."
                           " If false (the default), this operation only considers contours that match the"
                           " ROILabelRegex or NormalizedROILabelRegex; all other contours are ignored (and thus"
                           " will not be purged, i.e., a denylist)."
                           " If true, this operation only considers the *complement* of contours that match the"
                           " ROILabelRegex or NormalizedROILabelRegex, which can be used to purge all contours"
                           " except a handful (i.e., an allowlist).";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };


    out.args.emplace_back();
    out.args.back().name = "AreaAbove";
    out.args.back().desc = "If this option is provided with a valid positive number, contour(s) with an area"
                           " greater than the specified value are purged. Note that the DICOM coordinate"
                           " space is used. (Supplying the default, inf, will effectively disable this option.)";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "inf", "100.0", "1000", "10.23E8" };


    out.args.emplace_back();
    out.args.back().name = "AreaBelow";
    out.args.back().desc = "If this option is provided with a valid positive number, contour(s) with an area"
                           " less than the specified value are purged. Note that the DICOM coordinate"
                           " space is used. (Supplying the default, -inf, will effectively disable this option.)";
    out.args.back().default_val = "-inf";
    out.args.back().expected = true;
    out.args.back().examples = { "-inf", "100.0", "1000", "10.23E8" };


    out.args.emplace_back();
    out.args.back().name = "PerimeterAbove";
    out.args.back().desc = "If this option is provided with a valid positive number, contour(s) with a perimeter"
                           " greater than the specified value are purged. Note that the DICOM coordinate"
                           " space is used. (Supplying the default, inf, will effectively disable this option.)";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "inf", "10.0", "100", "10.23E4" };


    out.args.emplace_back();
    out.args.back().name = "PerimeterBelow";
    out.args.back().desc = "If this option is provided with a valid positive number, contour(s) with a perimeter"
                           " less than the specified value are purged. Note that the DICOM coordinate"
                           " space is used. (Supplying the default, -inf, will effectively disable this option.)";
    out.args.back().default_val = "-inf";
    out.args.back().expected = true;
    out.args.back().examples = { "-inf", "10.0", "100", "10.23E4" };


    out.args.emplace_back();
    out.args.back().name = "VertexCountAbove";
    out.args.back().desc = "If this option is provided with a valid positive number, contour(s) with a vertex"
                           " count greater than the specified value are purged. Note that the DICOM coordinate"
                           " space is used. (Supplying the default, inf, will effectively disable this option.)";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "inf", "10.0", "100", "10.23E4" };


    out.args.emplace_back();
    out.args.back().name = "VertexCountBelow";
    out.args.back().desc = "If this option is provided with a valid positive number, contour(s) with a vertex"
                           " count  less than the specified value are purged. Note that the DICOM coordinate"
                           " space is used. (Supplying the default, -inf, will effectively disable this option.)";
    out.args.back().default_val = "-inf";
    out.args.back().expected = true;
    out.args.back().examples = { "-inf", "10.0", "100", "10.23E4" };

    return out;
}



bool PurgeContours(Drover &DICOM_data,
                     const OperationArgPkg& OptArgs,
                     std::map<std::string, std::string>& /*InvocationMetadata*/,
                     const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();

    const auto InvertLogicStr = OptArgs.getValueStr("InvertLogic").value();
    const auto InvertSelectionStr = OptArgs.getValueStr("InvertSelection").value();

    const auto AreaAbove = std::stod( OptArgs.getValueStr("AreaAbove").value() );
    const auto AreaBelow = std::stod( OptArgs.getValueStr("AreaBelow").value() );
    const auto PerimeterAbove = std::stod( OptArgs.getValueStr("PerimeterAbove").value() );
    const auto PerimeterBelow = std::stod( OptArgs.getValueStr("PerimeterBelow").value() );
    const auto VertexCountAbove = std::stod( OptArgs.getValueStr("VertexCountAbove").value() );
    const auto VertexCountBelow = std::stod( OptArgs.getValueStr("VertexCountBelow").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = Compile_Regex("^tr?u?e?$");

    const auto roiregex = Compile_Regex(ROILabelRegex);
    const auto roinormalizedregex = Compile_Regex(NormalizedROILabelRegex);

    // This function handles toggling between normal and inverted logic when the user requests it.
    const auto handle_inverted_logic = (std::regex_match(InvertLogicStr, regex_true)) ?
                      [](bool in){ return !in; }
                    : [](bool in){ return in; };

    const auto handle_inverted_selection = (std::regex_match(InvertSelectionStr, regex_true)) ?
                      [](bool in){ return !in; }
                    : [](bool in){ return in; };

    // Selection criteria tests.
    auto matches_ROIName = [=](const contour_of_points<double> &cop) -> bool {
                   const auto ROINameOpt = cop.GetMetadataValueAs<std::string>("ROIName");
                   const auto ROIName = ROINameOpt.value_or("");
                   return (std::regex_match(ROIName,roiregex));
    };
    auto matches_NormalizedROIName = [=](const contour_of_points<double> &cop) -> bool {
                   const auto ROINameOpt = cop.GetMetadataValueAs<std::string>("NormalizedROIName");
                   const auto ROIName = ROINameOpt.value_or("");
                   return (std::regex_match(ROIName,roiregex));
    };
    auto matches_either_name = [=](const contour_of_points<double> &cop) -> bool {
                   return handle_inverted_selection( matches_ROIName(cop) || matches_NormalizedROIName(cop) );
    };

    // Purge criteria tests.
    //
    // These closures nominally return true when a contour should be removed, but the logic can be inverted according to
    // the user.
    auto area_criteria = [=](const contour_of_points<double> &cop) -> bool {
        if( !std::isnan( AreaAbove ) ){
            const auto area = std::abs(cop.Get_Signed_Area());
            if( !std::isfinite(area) ) return true; // Unconditionally remove if not finite.
            if( area >= AreaAbove ) return handle_inverted_logic(true);
        }
        if( !std::isnan( AreaBelow ) ){
            const auto area = std::abs(cop.Get_Signed_Area());
            if( !std::isfinite(area) ) return true; // Unconditionally remove if not finite.
            if( area <= AreaBelow) return handle_inverted_logic(true);
        }
        return handle_inverted_logic(false);
    };
    auto perimeter_criteria = [=](const contour_of_points<double> &cop) -> bool {
        if( !std::isnan( PerimeterAbove )){
            const auto perimeter = cop.Perimeter();
            if( !std::isfinite(perimeter) ) return true; // Unconditionally remove if not finite.
            if( perimeter >= PerimeterAbove ) return handle_inverted_logic(true);
        }
        if( !std::isnan( PerimeterBelow )){
            const auto perimeter = cop.Perimeter();
            if( !std::isfinite(perimeter) ) return true; // Unconditionally remove if not finite.
            if( perimeter <= PerimeterBelow ) return handle_inverted_logic(true);
        }
        return handle_inverted_logic(false);
    };
    auto vert_count_criteria = [=](const contour_of_points<double> &cop) -> bool {
        if( !std::isnan( VertexCountAbove )){
            const auto vert_count = static_cast<double>(cop.points.size());
            if( vert_count >= VertexCountAbove ) return handle_inverted_logic(true);
        }
        if( !std::isnan( VertexCountBelow )){
            const auto vert_count = static_cast<double>(cop.points.size());
            if( vert_count <= VertexCountBelow ) return handle_inverted_logic(true);
        }
        return handle_inverted_logic(false);
    };

    //Walk the contours, testing each contour iff selected.
    auto remove_all_criteria = [&](const contour_of_points<double> &cop) -> bool {
        const auto matches_selection = matches_either_name(cop);
        if(!matches_selection) return false; // If contour does not match selection criteria, ignore it.

        const auto purge_via_area = area_criteria(cop);
        const auto purge_via_perimeter = perimeter_criteria(cop);
        const auto purge_via_vert_count = vert_count_criteria(cop);
        const auto should_purge = purge_via_area || purge_via_perimeter || purge_via_vert_count;
        return should_purge;
    };
    for(auto & cc : DICOM_data.contour_data->ccs){
        cc.contours.remove_if( remove_all_criteria );
    }

    // Purge any empty contour collections.
    DICOM_data.contour_data->ccs.remove_if( [](const contour_collection<double> &cc){ return cc.contours.empty(); } );

    return true;
}

