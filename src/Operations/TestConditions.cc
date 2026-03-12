//TestConditions.cc - A part of DICOMautomaton 2025. Written by hal clark.

#include <algorithm>
#include <optional>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <cmath>
#include <cstdint>
#include <limits>

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../String_Parsing.h"

#include "TestConditions.h"


OperationDoc OpArgDocTestConditions(){
    OperationDoc out;
    out.name = "TestConditions";

    out.tags.emplace_back("category: meta");

    out.desc =
        "This operation evaluates one or more user-specified conditions about the state of the Drover class."
        " If any condition is not satisfied, the operation throws an exception, causing the pipeline to fail."
        "\n\n"
        "Conditions are specified using a function-call syntax parsed via parse_functions()."
        " Multiple conditions can be separated with semicolons."
        "\n\n"
        "Supported conditions include:"
        "\n"
        " - image_array_count(N): confirm exactly N Image_Arrays are present.\n"
        " - contour_collection_count(N): confirm exactly N contour_collections are present.\n"
        " - point_cloud_count(N): confirm exactly N Point_Clouds are present.\n"
        " - surface_mesh_count(N): confirm exactly N Surface_Meshes are present.\n"
        " - rtplan_count(N): confirm exactly N RTPlans are present.\n"
        " - line_sample_count(N): confirm exactly N Line_Samples are present.\n"
        " - transform3_count(N): confirm exactly N Transform3s are present.\n"
        " - table_count(N): confirm exactly N Sparse_Tables are present.\n"
        " - image_count(N): confirm exactly N images in the selected Image_Array(s).\n"
        " - contour_count(N): confirm exactly N contours in the selected contour_collection(s).\n"
        " - pixel_minmax(MIN, MAX) or pixel_minmax(MIN, MAX, tolerance): confirm the minimum and/or"
        " maximum pixel value. MIN or MAX can be non-numeric to disable that check. Tolerance applies"
        " to both enabled checks.\n"
        "\n"
        "This operation is useful for integration testing and for verifying pre- and post-conditions"
        " in DCMA scripts.";

    out.args.emplace_back();
    out.args.back().name = "Conditions";
    out.args.back().desc = "A list of conditions to evaluate, where each function represents a single condition."
                           " Multiple conditions are separated by semicolons."
                           "\n\n"
                           "For example, 'image_array_count(2); contour_collection_count(3)'"
                           " will confirm that there are exactly 2 Image_Arrays and 3 contour_collections.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "image_array_count(1)",
                                 "contour_collection_count(2)",
                                 "image_array_count(1); contour_collection_count(0)",
                                  "pixel_minmax(0, 70, 0.5)",
                                 "image_count(4); contour_count(7)",
                                 "surface_mesh_count(2); point_cloud_count(1)",
                                 "line_sample_count(3); table_count(0)",
                                 "transform3_count(1); rtplan_count(0)" };

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "all";

    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = CCWhitelistOpArgDoc();
    out.args.back().name = "ROISelection";
    out.args.back().default_val = "all";

    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The channel to consider for pixel value conditions."
                           " If negative, all channels are considered.";
    out.args.back().default_val = "-1";
    out.args.back().expected = true;
    out.args.back().examples = { "-1", "0", "1", "2" };

    return out;
}


bool TestConditions(Drover &DICOM_data,
                    const OperationArgPkg& OptArgs,
                    std::map<std::string, std::string>&,
                    const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ConditionsStr = OptArgs.getValueStr("Conditions").value();
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();
    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    //-----------------------------------------------------------------------------------------------------------------

    // Parse the conditions.
    const auto pfs = parse_functions(ConditionsStr);
    if(pfs.empty()){
        throw std::invalid_argument("No conditions specified");
    }

    // Condition name regexes use a fuzzy-abbreviation pattern: only the first few letters of each
    // word are required, and separators (underscores, dashes, spaces) are optional.
    const auto regex_ia_count    = Compile_Regex("^imag?e?[ _-]?arr?a?y?[ _-]?cou?n?t?$");
    const auto regex_cc_count    = Compile_Regex("^conto?u?r?[ _-]?colle?c?t?i?o?n?[ _-]?cou?n?t?$");
    const auto regex_pc_count    = Compile_Regex("^poin?t?[ _-]?clo?u?d?[ _-]?cou?n?t?$");
    const auto regex_smesh_count = Compile_Regex("^surf?a?c?e?[ _-]?mes?h?[ _-]?cou?n?t?$");
    const auto regex_rtp_count   = Compile_Regex("^rtpl?a?n?[ _-]?cou?n?t?$");
    const auto regex_lsamp_count = Compile_Regex("^lin?e?[ _-]?sam?p?l?e?[ _-]?cou?n?t?$");
    const auto regex_trans_count = Compile_Regex("^tran?s?f?o?r?m?3?[ _-]?cou?n?t?$");
    const auto regex_table_count = Compile_Regex("^tabl?e?[ _-]?cou?n?t?$");
    const auto regex_img_count   = Compile_Regex("^imag?e?[ _-]?cou?n?t?$");
    const auto regex_cnt_count   = Compile_Regex("^conto?u?r?[ _-]?cou?n?t?$");
    const auto regex_pix_minmax  = Compile_Regex("^pixe?l?[ _-]?min[ _-]?max$");

    // Tolerance for floating-point pixel value comparisons.
    const double default_tolerance = 0.01;

    const auto parse_optional_numeric = [](const std::string &raw) -> std::optional<double> {
        try{
            size_t n = 0;
            const auto v = std::stod(raw, &n);
            if(n == raw.size()){
                return v;
            }
            return std::nullopt;
        }catch(const std::exception &){
            return std::nullopt;
        }
    };

    for(const auto &pf : pfs){
        if(!pf.children.empty()){
            throw std::invalid_argument("Children functions are not accepted for condition '"
                                        + pf.name + "'");
        }

        // --- Drover-level count conditions ---

        if(std::regex_match(pf.name, regex_ia_count)){
            if(pf.parameters.size() != 1){
                throw std::invalid_argument("image_array_count requires exactly 1 argument (expected count)");
            }
            const auto expected = static_cast<int64_t>(std::stoll(pf.parameters.at(0).raw));
            const auto actual = static_cast<int64_t>(DICOM_data.image_data.size());
            if(actual != expected){
                throw std::runtime_error("Condition failed: image_array_count expected "
                                         + std::to_string(expected) + " but found "
                                         + std::to_string(actual));
            }
            YLOGINFO("Condition passed: image_array_count(" << expected << ")");

        }else if(std::regex_match(pf.name, regex_cc_count)){
            if(pf.parameters.size() != 1){
                throw std::invalid_argument("contour_collection_count requires exactly 1 argument (expected count)");
            }
            const auto expected = static_cast<int64_t>(std::stoll(pf.parameters.at(0).raw));
            int64_t actual = 0;
            if(DICOM_data.contour_data != nullptr){
                actual = static_cast<int64_t>(DICOM_data.contour_data->ccs.size());
            }
            if(actual != expected){
                throw std::runtime_error("Condition failed: contour_collection_count expected "
                                         + std::to_string(expected) + " but found "
                                         + std::to_string(actual));
            }
            YLOGINFO("Condition passed: contour_collection_count(" << expected << ")");

        }else if(std::regex_match(pf.name, regex_pc_count)){
            if(pf.parameters.size() != 1){
                throw std::invalid_argument("point_cloud_count requires exactly 1 argument (expected count)");
            }
            const auto expected = static_cast<int64_t>(std::stoll(pf.parameters.at(0).raw));
            const auto actual = static_cast<int64_t>(DICOM_data.point_data.size());
            if(actual != expected){
                throw std::runtime_error("Condition failed: point_cloud_count expected "
                                         + std::to_string(expected) + " but found "
                                         + std::to_string(actual));
            }
            YLOGINFO("Condition passed: point_cloud_count(" << expected << ")");

        }else if(std::regex_match(pf.name, regex_smesh_count)){
            if(pf.parameters.size() != 1){
                throw std::invalid_argument("surface_mesh_count requires exactly 1 argument (expected count)");
            }
            const auto expected = static_cast<int64_t>(std::stoll(pf.parameters.at(0).raw));
            const auto actual = static_cast<int64_t>(DICOM_data.smesh_data.size());
            if(actual != expected){
                throw std::runtime_error("Condition failed: surface_mesh_count expected "
                                         + std::to_string(expected) + " but found "
                                         + std::to_string(actual));
            }
            YLOGINFO("Condition passed: surface_mesh_count(" << expected << ")");

        }else if(std::regex_match(pf.name, regex_rtp_count)){
            if(pf.parameters.size() != 1){
                throw std::invalid_argument("rtplan_count requires exactly 1 argument (expected count)");
            }
            const auto expected = static_cast<int64_t>(std::stoll(pf.parameters.at(0).raw));
            const auto actual = static_cast<int64_t>(DICOM_data.rtplan_data.size());
            if(actual != expected){
                throw std::runtime_error("Condition failed: rtplan_count expected "
                                         + std::to_string(expected) + " but found "
                                         + std::to_string(actual));
            }
            YLOGINFO("Condition passed: rtplan_count(" << expected << ")");

        }else if(std::regex_match(pf.name, regex_lsamp_count)){
            if(pf.parameters.size() != 1){
                throw std::invalid_argument("line_sample_count requires exactly 1 argument (expected count)");
            }
            const auto expected = static_cast<int64_t>(std::stoll(pf.parameters.at(0).raw));
            const auto actual = static_cast<int64_t>(DICOM_data.lsamp_data.size());
            if(actual != expected){
                throw std::runtime_error("Condition failed: line_sample_count expected "
                                         + std::to_string(expected) + " but found "
                                         + std::to_string(actual));
            }
            YLOGINFO("Condition passed: line_sample_count(" << expected << ")");

        }else if(std::regex_match(pf.name, regex_trans_count)){
            if(pf.parameters.size() != 1){
                throw std::invalid_argument("transform3_count requires exactly 1 argument (expected count)");
            }
            const auto expected = static_cast<int64_t>(std::stoll(pf.parameters.at(0).raw));
            const auto actual = static_cast<int64_t>(DICOM_data.trans_data.size());
            if(actual != expected){
                throw std::runtime_error("Condition failed: transform3_count expected "
                                         + std::to_string(expected) + " but found "
                                         + std::to_string(actual));
            }
            YLOGINFO("Condition passed: transform3_count(" << expected << ")");

        }else if(std::regex_match(pf.name, regex_table_count)){
            if(pf.parameters.size() != 1){
                throw std::invalid_argument("table_count requires exactly 1 argument (expected count)");
            }
            const auto expected = static_cast<int64_t>(std::stoll(pf.parameters.at(0).raw));
            const auto actual = static_cast<int64_t>(DICOM_data.table_data.size());
            if(actual != expected){
                throw std::runtime_error("Condition failed: table_count expected "
                                         + std::to_string(expected) + " but found "
                                         + std::to_string(actual));
            }
            YLOGINFO("Condition passed: table_count(" << expected << ")");

        // --- Per-object count conditions ---

        }else if(std::regex_match(pf.name, regex_img_count)){
            if(pf.parameters.size() != 1){
                throw std::invalid_argument("image_count requires exactly 1 argument (expected count)");
            }
            const auto expected = static_cast<int64_t>(std::stoll(pf.parameters.at(0).raw));

            auto IAs_all = All_IAs( DICOM_data );
            auto IAs = Whitelist( IAs_all, ImageSelectionStr );

            int64_t actual = 0;
            for(auto &iap_it : IAs){
                if(*iap_it != nullptr){
                    actual += static_cast<int64_t>((*iap_it)->imagecoll.images.size());
                }
            }
            if(actual != expected){
                throw std::runtime_error("Condition failed: image_count expected "
                                         + std::to_string(expected) + " but found "
                                         + std::to_string(actual));
            }
            YLOGINFO("Condition passed: image_count(" << expected << ")");

        }else if(std::regex_match(pf.name, regex_cnt_count)){
            if(pf.parameters.size() != 1){
                throw std::invalid_argument("contour_count requires exactly 1 argument (expected count)");
            }
            const auto expected = static_cast<int64_t>(std::stoll(pf.parameters.at(0).raw));

            auto cc_all = All_CCs( DICOM_data );
            auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );

            int64_t actual = 0;
            for(auto &cc_refw : cc_ROIs){
                actual += static_cast<int64_t>(cc_refw.get().contours.size());
            }
            if(actual != expected){
                throw std::runtime_error("Condition failed: contour_count expected "
                                         + std::to_string(expected) + " but found "
                                         + std::to_string(actual));
            }
            YLOGINFO("Condition passed: contour_count(" << expected << ")");

        // --- Pixel value conditions ---

        }else if(std::regex_match(pf.name, regex_pix_minmax)){
            if(pf.parameters.size() < 2 || pf.parameters.size() > 3){
                throw std::invalid_argument("pixel_minmax requires 2 or 3 arguments"
                                            " (min, max, optional tolerance)");
            }
            const auto expected_min = parse_optional_numeric(pf.parameters.at(0).raw);
            const auto expected_max = parse_optional_numeric(pf.parameters.at(1).raw);
            if(!expected_min && !expected_max){
                throw std::invalid_argument("pixel_minmax requires at least one numeric"
                                            " bound (min or max)");
            }
            const auto tolerance = (pf.parameters.size() == 3) ? std::stod(pf.parameters.at(2).raw)
                                                                : default_tolerance;

            auto IAs_all = All_IAs( DICOM_data );
            auto IAs = Whitelist( IAs_all, ImageSelectionStr );
            if(IAs.empty()){
                throw std::invalid_argument("No image arrays selected for pixel value condition");
            }

            double actual_min = std::numeric_limits<double>::infinity();
            double actual_max = -std::numeric_limits<double>::infinity();
            bool pixels_examined = false;
            for(auto &iap_it : IAs){
                if(*iap_it == nullptr) continue;
                for(auto &img : (*iap_it)->imagecoll.images){
                    if(Channel < 0){
                        // All channels: use the built-in minmax for efficiency.
                        const auto mm = img.minmax();
                        const auto mm_min = static_cast<double>(mm.first);
                        const auto mm_max = static_cast<double>(mm.second);
                        if(mm_min < actual_min) actual_min = mm_min;
                        if(mm_max > actual_max) actual_max = mm_max;
                        pixels_examined = true;
                    }else{
                        if(Channel >= img.channels){
                            throw std::invalid_argument("Channel " + std::to_string(Channel)
                                                        + " exceeds image channel count "
                                                        + std::to_string(img.channels));
                        }
                        for(int64_t row = 0; row < img.rows; ++row){
                            for(int64_t col = 0; col < img.columns; ++col){
                                const auto val = static_cast<double>(img.value(row, col, Channel));
                                if(val < actual_min) actual_min = val;
                                if(val > actual_max) actual_max = val;
                                pixels_examined = true;
                            }
                        }
                    }
                }
            }
            if(!pixels_examined){
                throw std::invalid_argument("No images with pixel data found for '"
                                            + pf.name + "' condition");
            }

            if(expected_min){
                if(std::abs(actual_min - *expected_min) > tolerance){
                    throw std::runtime_error("Condition failed: pixel_min expected "
                                             + std::to_string(*expected_min) + " (tolerance "
                                             + std::to_string(tolerance) + ") but found "
                                             + std::to_string(actual_min));
                }
                YLOGINFO("Condition passed: pixel_min(" << *expected_min << ") actual=" << actual_min);
            }
            if(expected_max){
                if(std::abs(actual_max - *expected_max) > tolerance){
                    throw std::runtime_error("Condition failed: pixel_max expected "
                                             + std::to_string(*expected_max) + " (tolerance "
                                             + std::to_string(tolerance) + ") but found "
                                             + std::to_string(actual_max));
                }
                YLOGINFO("Condition passed: pixel_max(" << *expected_max << ") actual=" << actual_max);
            }

        }else{
            throw std::invalid_argument("Unrecognized condition: '" + pf.name + "'");
        }
    }

    return true;
}
