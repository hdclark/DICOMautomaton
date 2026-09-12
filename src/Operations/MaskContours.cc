// MaskContours.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Explicator.h"

#include "YgorMath.h"
#include "YgorString.h"

#include "../GIS.h"
#include "../Regex_Selectors.h"
#include "../String_Parsing.h"
#include "../Structs.h"
#include "../Contour_Mask.h"


OperationDoc OpArgDocMaskContours(){
    OperationDoc out;
    out.name = "MaskContours";
    out.tags.emplace_back("category: contour processing");
    out.desc =
        "Slices selected contours against one or more named polygonal regions. The original contours are retained; "
        "derived inside/outside path portions are appended and tagged with MaskContours procedure metadata. "
        "Optionally, the selected region boundaries can instead be emitted as contours for visualization.";

    out.notes.emplace_back(
        "Regions use the parsed_function syntax Region(name){Polygon(...); GeoPolygon(...); ...} or NamedRegion(name). "
        "A Region is the union "
        "of all of its polygon children. Polygon uses native contour x,y coordinates. Polygon3D accepts x,y,z triples "
        "but masking is performed in the x-y footprint. GeoPolygon accepts latitude,longitude pairs and applies the same "
        "Mercator projection used by the GPX loader. Polygon closure is implicit; repeating the first vertex is optional."
    );
    out.notes.emplace_back(
        "When EmitRegionContours is true, all selected region polygons are emitted into one contour collection and no "
        "source contours are masked. NamedRegion coordinates are already represented in the same Mercator projection "
        "used by the GPX loader, so the emitted boundaries can be overlaid directly with GPX tracks."
    );
    out.notes.emplace_back(
        "DebounceDistance is measured along the source contour in native contour coordinate units. A short inside/outside "
        "run is absorbed only when it is bracketed by the opposite state. This suppresses GPS boundary chatter and brief "
        "leave-and-return excursions without erasing short runs at the beginning or end of a trace."
    );
    out.notes.emplace_back(
        "All supplied regions form one mask union: a contour portion is inside when it is inside at least one region. "
        "MaskContoursRegion lists the supplied region names in input order, separated by semicolons. Derived contour "
        "collections are homogeneous in MaskContoursState for downstream metadata partitioning/filtering."
    );
    out.notes.emplace_back(
        "Predefined regions are trail-focused approximate footprints, not legal or authoritative park boundaries."
    );
    out.notes.emplace_back(
        "Available predefined regions are Fromme, Burke, Eagle Mountain, Cypress Mountain, Grouse Mountain Bike Park, "
        "Seymour Mountain, Burnaby Mountain, Delta Watershed, Bert Flinn Park, Thornhill, Woodlot 0007, Bear Mountain, "
        "Red Mountain, Sumas Mountain, Vedder Mountain, Ledgeview, Whistler Mountain Bike Park, Mount Washington Bike "
        "Park, Revelstoke Bike Park, Kicking Horse Bike Park, Sun Peaks Bike Park, Kamloops Bike Ranch, Winsport Bike "
        "Park, and Hinton Bike Park. Names are case-insensitive and spaces, hyphens, and punctuation are optional."
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
    out.args.back().name = "Regions";
    out.args.back().desc =
        "Semicolon-separated Region or NamedRegion functions. Each Region(name){...} contains one or more Polygon(x,y,...), "
        "Polygon3D(x,y,z,...), or GeoPolygon(latitude,longitude,...) child functions. NamedRegion(name) selects a "
        "predefined trail-focused geographic region.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = {
        "Region(test){Polygon(0,0, 10,0, 10,10, 0,10)}",
        "Region(CustomFromme){GeoPolygon(49.327,-123.104, 49.392,-123.104, 49.392,-123.036, 49.327,-123.036)};"
        "Region(CustomSeymour){GeoPolygon(49.315,-123.010, 49.390,-123.010, 49.390,-122.925, 49.315,-122.925)};"
        "Region(CustomEagleMountain){GeoPolygon(49.275,-122.875, 49.345,-122.875, 49.345,-122.790, 49.275,-122.790)}"
        "NamedRegion(Fromme)",
        "NamedRegion(Fromme);NamedRegion(Burke);NamedRegion(Eagle Mountain)"
    };

    out.args.emplace_back();
    out.args.back().name = "EmitRegionContours";
    out.args.back().desc =
        "If true, emit all selected region polygons as closed contours in a single contour collection and return without "
        "masking any source contours. If false, perform the normal MaskContours slicing behaviour.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "DebounceDistance";
    out.args.back().desc =
        "Maximum along-contour length of an interior run to absorb when it leaves one state and returns to that same "
        "state. Zero disables debouncing. For GPX data loaded by DICOMautomaton, coordinates are Mercator metres.";
    out.args.back().default_val = "0.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0", "5.0", "20.0", "100.0" };

    return out;
}

bool MaskContours(Drover &DICOM_data,
                  const OperationArgPkg& OptArgs,
                  std::map<std::string, std::string>& /*InvocationMetadata*/,
                  const std::string& FilenameLex){
    const auto RegionSpec = OptArgs.getValueStr("Regions").value();
    const auto EmitRegionContoursStr = OptArgs.getValueStr("EmitRegionContours").value();

    const auto regex_true = Compile_Regex("^tr?u?e?$");
    const bool EmitRegionContours = std::regex_match(EmitRegionContoursStr, regex_true);

    const auto regions = dcma::mask_contours::parse_regions(RegionSpec);

    dcma::mask_contours::mask_region combined_region;
    for(const auto &region : regions){
        if(!combined_region.name.empty()) combined_region.name += ";";
        combined_region.name += region.name;
        combined_region.polygons.insert(combined_region.polygons.end(), region.polygons.begin(), region.polygons.end());
    }

    if(EmitRegionContours){
        Explicator X(FilenameLex);
        const auto RegionContourLabel = "MaskContours regions: "_s + combined_region.name;

        auto contour_storage = std::make_shared<Contour_Data>();
        auto region_cc = dcma::mask_contours::region_boundaries_as_contours(
            regions, RegionContourLabel, X(RegionContourLabel)
        );
        contour_storage->ccs.emplace_back(std::move(region_cc));
        DICOM_data.Consume(contour_storage);
        return true;
    }

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();
    const auto DebounceDistance = std::stod(OptArgs.getValueStr("DebounceDistance").value());

    if(!std::isfinite(DebounceDistance) || (DebounceDistance < 0.0)){
        throw std::invalid_argument("MaskContours: DebounceDistance must be finite and non-negative");
    }

    auto cc_all = All_CCs(DICOM_data);
    auto cc_selected = Whitelist(cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection);
    if(cc_selected.empty()){
        throw std::invalid_argument("MaskContours: no contours selected");
    }

    // Build all derived data separately so appending it cannot invalidate references in the selection list.
    // Keep each appended collection homogeneous by state, which makes the added metadata useful to collection-level
    // selectors and partitioning operations.
    auto contour_storage = std::make_shared<Contour_Data>();
    for(const auto &cc_refw : cc_selected){
        const auto &src_cc = cc_refw.get();
        contour_collection<double> inside_cc;
        contour_collection<double> outside_cc;

        for(const auto &source : src_cc.contours){
            auto pieces = dcma::mask_contours::slice_contour(source, combined_region, DebounceDistance);
            for(auto &piece : pieces){
                const auto state_it = piece.metadata.find("MaskContoursState");
                if(state_it == piece.metadata.end()){
                    throw std::logic_error("MaskContours: derived contour is missing MaskContoursState metadata");
                }
                if(state_it->second == "inside"){
                    inside_cc.contours.emplace_back(std::move(piece));
                }else if(state_it->second == "outside"){
                    outside_cc.contours.emplace_back(std::move(piece));
                }else{
                    throw std::logic_error("MaskContours: derived contour has an invalid MaskContoursState value");
                }
            }
        }

        if(!inside_cc.contours.empty()) contour_storage->ccs.emplace_back(std::move(inside_cc));
        if(!outside_cc.contours.empty()) contour_storage->ccs.emplace_back(std::move(outside_cc));
    }

    if(!contour_storage->ccs.empty()) DICOM_data.Consume(contour_storage);
    return true;
}
