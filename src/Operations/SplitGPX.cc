//SplitGPX.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <cstdint>

#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>

#include "Explicator.h"

#include "../GPX.h"
#include "../Regex_Selectors.h"
#include "../Structs.h"

#include "SplitGPX.h"

OperationDoc OpArgDocSplitGPX(){
    OperationDoc out;
    out.name = "SplitGPX";

    out.tags.emplace_back("category: contour processing");
    out.tags.emplace_back("category: file import");

    out.desc =
        "This operation splits GPX-style open contours into separate tracks when they cross one or more"
        " named closed boundary contours."
        " Repeated crossings can be debounced so that GPS jitter or lingering near a trailhead does not"
        " create a large number of tiny tracks.";

    out.notes.emplace_back(
        "This operation is intended for GPX contours loaded from XML/GPX files."
        " Only selected contour collections containing a single open contour are split."
        " Other selected contour collections are retained unchanged."
    );

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = CCWhitelistOpArgDoc();
    out.args.back().name = "ROISelection";
    out.args.back().default_val = "all";

    out.args.emplace_back();
    out.args.back().name = "BoundaryContours";
    out.args.back().desc =
        "Boundary contours specified in GPS coordinates."
        " When empty or set to 'lower-mainland-mtb', a predefined set of popular lower mainland riding areas"
        " is used."
        " Custom boundaries use a semicolon-separated function syntax such as"
        " 'boundary(Fromme, 49.338, -123.104, 49.338, -123.048, 49.377, -123.048, 49.377, -123.104);"
        "  boundary(Seymour, 49.340, -122.999, 49.340, -122.925, 49.392, -122.925, 49.392, -122.999)'.";
    out.args.back().default_val = "lower-mainland-mtb";
    out.args.back().expected = true;
    out.args.back().examples = {
        "lower-mainland-mtb",
        "boundary(Fromme, 49.338, -123.104, 49.338, -123.048, 49.377, -123.048, 49.377, -123.104)",
    };

    out.args.emplace_back();
    out.args.back().name = "DebounceDistanceMetres";
    out.args.back().desc =
        "The distance the track must travel after a split before another boundary crossing may trigger"
        " an additional split.";
    out.args.back().default_val = "50.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0", "25.0", "50.0", "100.0" };

    return out;
}

bool SplitGPX(Drover &DICOM_data,
              const OperationArgPkg& OptArgs,
              std::map<std::string, std::string>& /*InvocationMetadata*/,
              const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();

    const auto BoundaryContours = OptArgs.getValueStr("BoundaryContours").value();
    const auto DebounceDistanceMetres = std::stod( OptArgs.getValueStr("DebounceDistanceMetres").value() );

    //-----------------------------------------------------------------------------------------------------------------
    if(DebounceDistanceMetres < 0.0){
        throw std::invalid_argument("DebounceDistanceMetres must be non-negative");
    }

    DICOM_data.Ensure_Contour_Data_Allocated();

    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    const auto boundaries = dcma::gpx::parse_boundary_contours(BoundaryContours);
    if(boundaries.empty()){
        throw std::invalid_argument("No boundary contours were provided. Cannot continue.");
    }

    std::set<const contour_collection<double>*> selected_ccs;
    for(const auto &cc_refw : cc_ROIs){
        selected_ccs.insert(std::addressof(cc_refw.get()));
    }

    Explicator X(FilenameLex);
    std::map<std::string, int64_t> name_counts;
    std::list<contour_collection<double>> replacement_ccs;

    for(const auto &cc : DICOM_data.contour_data->ccs){
        const auto is_selected = (selected_ccs.count(std::addressof(cc)) != 0U);
        if(!is_selected){
            replacement_ccs.push_back(cc);
            continue;
        }

        if( (cc.contours.size() != 1)
        ||  cc.contours.front().closed ){
            replacement_ccs.push_back(cc);
            continue;
        }

        const auto base_roi_name = cc.get_dominant_value_for_key("ROIName").value_or("track");
        const auto split_segments = dcma::gpx::split_open_contour_on_boundary_crossings(cc.contours.front(),
                                                                                         boundaries,
                                                                                         DebounceDistanceMetres);

        for(const auto &segment : split_segments){
            auto new_cc = cc;
            new_cc.contours.clear();
            new_cc.contours.push_back(segment.contour);

            auto &new_contour = new_cc.contours.back();
            const auto output_label_root = base_roi_name + " - " + segment.location_name;
            const auto output_label = output_label_root + " - " + std::to_string(++name_counts[output_label_root]);

            new_contour.metadata["ROIName"] = output_label;
            new_contour.metadata["NormalizedROIName"] = X(output_label);
            new_contour.metadata["SplitGPXLocation"] = segment.location_name;
            new_contour.metadata["SplitGPXDebounceDistanceMetres"] = std::to_string(DebounceDistanceMetres);

            replacement_ccs.push_back(std::move(new_cc));
        }
    }

    DICOM_data.contour_data->ccs = std::move(replacement_ccs);
    return true;
}
