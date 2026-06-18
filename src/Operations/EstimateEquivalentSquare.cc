//EstimateEquivalentSquare.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <cmath>
#include <iomanip>
#include <limits>
#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>            //Needed for std::move.
#include <vector>

#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"

#include "../Regex_Selectors.h"
#include "../Structs.h"

#include "EstimateEquivalentSquare.h"

#include "../doctest20251212/doctest.h"



namespace {
constexpr double contour_eps = 1.0e-8;

struct point2 {
    double x = 0.0;
    double y = 0.0;
};

std::vector<point2> contour_points_2d(const contour_of_points<double> &c){
    std::vector<point2> out;
    out.reserve(c.points.size());
    for(const auto &p : c.points) out.push_back({p.x, p.y});
    return out;
}

double orient(const point2 &a, const point2 &b, const point2 &c){
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}


bool point_on_segment(const point2 &p, const point2 &a, const point2 &b){
    if(std::abs(orient(a, b, p)) > contour_eps) return false;
    return (std::min(a.x, b.x) - contour_eps <= p.x) && (p.x <= std::max(a.x, b.x) + contour_eps)
        && (std::min(a.y, b.y) - contour_eps <= p.y) && (p.y <= std::max(a.y, b.y) + contour_eps);
}

bool segments_intersect_or_touch(const point2 &a, const point2 &b, const point2 &c, const point2 &d){
    const auto o1 = orient(a, b, c);
    const auto o2 = orient(a, b, d);
    const auto o3 = orient(c, d, a);
    const auto o4 = orient(c, d, b);
    if(((o1 > contour_eps && o2 < -contour_eps) || (o1 < -contour_eps && o2 > contour_eps))
    && ((o3 > contour_eps && o4 < -contour_eps) || (o3 < -contour_eps && o4 > contour_eps))){
        return true;
    }
    return point_on_segment(c, a, b) || point_on_segment(d, a, b)
        || point_on_segment(a, c, d) || point_on_segment(b, c, d);
}

bool point_in_contour(const point2 &p, const contour_of_points<double> &c){
    const auto pts = contour_points_2d(c);
    bool inside = false;
    for(size_t i = 0U, j = pts.size() - 1U; i < pts.size(); j = i++){
        if(point_on_segment(p, pts[j], pts[i])) return true;
        const bool crosses = ((pts[i].y > p.y) != (pts[j].y > p.y))
            && (p.x < (pts[j].x - pts[i].x) * (p.y - pts[i].y) / (pts[j].y - pts[i].y) + pts[i].x);
        if(crosses) inside = !inside;
    }
    return inside;
}
}

bool ContourHasSelfIntersection(const contour_of_points<double> &c){
    const auto pts = contour_points_2d(c);
    if(pts.size() < 3U) return true;
    for(size_t i = 0U; i < pts.size(); ++i){
        const auto in = (i + 1U) % pts.size();
        for(size_t j = i + 1U; j < pts.size(); ++j){
            const auto jn = (j + 1U) % pts.size();
            const bool adjacent = (i == j) || (in == j) || (jn == i);
            if(adjacent) continue;
            if(segments_intersect_or_touch(pts[i], pts[in], pts[j], pts[jn])) return true;
        }
    }
    return false;
}

bool ContourCollectionsHaveIntersections(const contour_collection<double> &cc){
    for(auto a_it = cc.contours.begin(); a_it != cc.contours.end(); ++a_it){
        const auto a_pts = contour_points_2d(*a_it);
        auto b_it = a_it;
        for(++b_it; b_it != cc.contours.end(); ++b_it){
            const auto b_pts = contour_points_2d(*b_it);
            for(size_t i = 0U; i < a_pts.size(); ++i){
                const auto in = (i + 1U) % a_pts.size();
                for(size_t j = 0U; j < b_pts.size(); ++j){
                    const auto jn = (j + 1U) % b_pts.size();
                    if(segments_intersect_or_touch(a_pts[i], a_pts[in], b_pts[j], b_pts[jn])) return true;
                }
            }
        }
    }
    return false;
}

bool ContourCollectionIsSimpleNested(const contour_collection<double> &cc){
    for(const auto &c : cc.contours){
        if(!c.closed || (c.points.size() < 3U) || ContourHasSelfIntersection(c)) return false;
    }
    return !ContourCollectionsHaveIntersections(cc);
}

double EstimateEquivalentSquareForContours(const contour_collection<double> &cc,
                                           const EquivalentSquareHoleDetection hole_detection){
    if(!ContourCollectionIsSimpleNested(cc)){
        throw std::invalid_argument("Non-simple contour arrangement encountered; contours must be closed, non-self-intersecting, and non-intersecting");
    }

    double area = 0.0;
    double perimeter = 0.0;

    for(auto c_it = cc.contours.begin(); c_it != cc.contours.end(); ++c_it){
        const auto signed_area = c_it->Get_Signed_Area(true);
        const auto abs_area = std::abs(signed_area);
        const auto c_perimeter = c_it->Perimeter();
        if(!std::isfinite(signed_area) || !std::isfinite(c_perimeter)){
            throw std::invalid_argument("Encountered non-finite contour area or perimeter");
        }
        if(abs_area <= 0.0 || c_perimeter <= 0.0){
            throw std::invalid_argument("Encountered degenerate contour with non-positive area or perimeter");
        }

        if(hole_detection == EquivalentSquareHoleDetection::SignedContourOrientation){
            area += signed_area;
        }else{
            size_t containing_contours = 0U;
            const auto probe = contour_points_2d(*c_it).front();
            for(auto other_it = cc.contours.begin(); other_it != cc.contours.end(); ++other_it){
                if(other_it == c_it) continue;
                if(point_in_contour(probe, *other_it)) ++containing_contours;
            }
            area += ((containing_contours % 2U) == 0U) ? abs_area : -abs_area;
        }
        perimeter += c_perimeter;
    }

    area = std::abs(area);
    if(area <= 0.0 || perimeter <= 0.0){
        throw std::invalid_argument("No valid contour aperture was available");
    }

    return 4.0 * area / perimeter;
}

OperationDoc OpArgDocEstimateEquivalentSquare(){
    OperationDoc out;
    out.name = "EstimateEquivalentSquare";

    out.tags.emplace_back("category: contour processing");
    out.tags.emplace_back("category: parameter table");
    out.tags.emplace_back("category: radiotherapy treatment planning");

    out.desc =
        "Estimate the Sterling equivalent square for an ROI containing one or more closed planar contours."
        " The selected contours are interpreted as radiotherapy field apertures in millimetres, and the"
        " equivalent square is computed as 4*A/P, where A is the total aperture area and P is the total"
        " aperture perimeter. The result has scale units of millimetres and is stored in the global parameter table.";

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
    out.args.back().name = "ParameterKey";
    out.args.back().desc = "The key under which the equivalent square will be stored in the global parameter table.";
    out.args.back().default_val = "EquivalentSquare_mm";
    out.args.back().expected = true;
    out.args.back().examples = { "FieldEquivalentSquare", "eqsq_mm" };

    out.args.emplace_back();
    out.args.back().name = "HoleDetection";
    out.args.back().desc = "Controls how nested contours are interpreted as holes, islands, and islands with holes."
                           " 'signed' uses contour signedness/orientation. 'overlap' ignores orientation and alternates"
                           " aperture/hole status by nesting depth (even-odd-even-odd). Non-simple arrangements,"
                           " including self-intersections, contour intersections, and T-intersections, are rejected.";
    out.args.back().default_val = "signed";
    out.args.back().expected = true;
    out.args.back().examples = { "signed", "overlap" };

    return out;
}

bool EstimateEquivalentSquare(Drover& DICOM_data,
                              const OperationArgPkg& OptArgs,
                              std::map<std::string, std::string>& InvocationMetadata,
                              const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();
    const auto ParameterKey = OptArgs.getValueStr("ParameterKey").value();
    const auto HoleDetectionStr = OptArgs.getValueStr("HoleDetection").value();

    //-----------------------------------------------------------------------------------------------------------------
    if(ParameterKey.empty()){
        throw std::invalid_argument("ParameterKey must not be empty");
    }

    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    if(cc_ROIs.size() != 1UL){
        throw std::invalid_argument("Exactly one ROI must be selected for equivalent square estimation");
    }

    const std::regex signed_regex("signed|orientation|contour_signedness");
    const std::regex overlap_regex("overlap|simple_overlap|even_odd|evenodd");
    EquivalentSquareHoleDetection hole_detection = EquivalentSquareHoleDetection::SignedContourOrientation;
    if(std::regex_match(HoleDetectionStr, signed_regex)){
        hole_detection = EquivalentSquareHoleDetection::SignedContourOrientation;
    }else if(std::regex_match(HoleDetectionStr, overlap_regex)){
        hole_detection = EquivalentSquareHoleDetection::SimpleOverlap;
    }else{
        throw std::invalid_argument("HoleDetection must be 'signed' or 'overlap'");
    }

    try{
        const auto equivalent_square = EstimateEquivalentSquareForContours(cc_ROIs.front().get(), hole_detection);

        std::ostringstream ss;
        ss << std::setprecision(std::numeric_limits<double>::max_digits10) << equivalent_square;
        InvocationMetadata[ParameterKey] = ss.str();

        YLOGINFO("Stored equivalent square '" << ss.str() << "' mm with key '" << ParameterKey << "'");
        return true;
    }catch(const std::invalid_argument &e){
        YLOGWARN("Unable to estimate equivalent square: " << e.what());
        return false;
    }
}

namespace {
contour_of_points<double> make_rectangle(const double width,
                                          const double height,
                                          const double x_offset = 0.0,
                                          const double y_offset = 0.0,
                                          const bool clockwise = false){
    std::list<vec3<double>> points{
        vec3<double>(x_offset, y_offset, 0.0),
        vec3<double>(x_offset + width, y_offset, 0.0),
        vec3<double>(x_offset + width, y_offset + height, 0.0),
        vec3<double>(x_offset, y_offset + height, 0.0)
    };
    if(clockwise) points.reverse();
    contour_of_points<double> c(points);
    c.closed = true;
    return c;
}

contour_of_points<double> make_bowtie(){
    contour_of_points<double> c(std::list<vec3<double>>{
        vec3<double>(0.0, 0.0, 0.0),
        vec3<double>(10.0, 10.0, 0.0),
        vec3<double>(0.0, 10.0, 0.0),
        vec3<double>(10.0, 0.0, 0.0)
    });
    c.closed = true;
    return c;
}

contour_collection<double> make_collection(std::list<contour_of_points<double>> contours){
    contour_collection<double> cc;
    cc.contours = std::move(contours);
    return cc;
}
}

TEST_CASE("EstimateEquivalentSquareForContours handles square apertures"){
    const auto cc = make_collection({ make_rectangle(10.0, 10.0) });
    CHECK(EstimateEquivalentSquareForContours(cc) == doctest::Approx(10.0));
}

TEST_CASE("EstimateEquivalentSquareForContours handles rectangular apertures"){
    const auto cc = make_collection({ make_rectangle(10.0, 20.0) });
    CHECK(EstimateEquivalentSquareForContours(cc) == doctest::Approx(40.0 / 3.0));
}

TEST_CASE("EstimateEquivalentSquareForContours handles multiple apertures"){
    const auto cc = make_collection({ make_rectangle(10.0, 10.0), make_rectangle(5.0, 5.0, 20.0) });
    CHECK(EstimateEquivalentSquareForContours(cc) == doctest::Approx(25.0 / 3.0));
}


TEST_CASE("EstimateEquivalentSquareForContours handles signed holes"){
    const auto cc = make_collection({ make_rectangle(10.0, 10.0), make_rectangle(5.0, 5.0, 2.5, 2.5, true) });
    CHECK(EstimateEquivalentSquareForContours(cc, EquivalentSquareHoleDetection::SignedContourOrientation) == doctest::Approx(5.0));
}

TEST_CASE("EstimateEquivalentSquareForContours handles simple-overlap holes"){
    const auto cc = make_collection({ make_rectangle(10.0, 10.0), make_rectangle(5.0, 5.0, 2.5, 2.5) });
    CHECK(EstimateEquivalentSquareForContours(cc, EquivalentSquareHoleDetection::SimpleOverlap) == doctest::Approx(5.0));
}

TEST_CASE("ContourHasSelfIntersection rejects bowtie contours"){
    CHECK(ContourHasSelfIntersection(make_bowtie()));
    CHECK_THROWS_AS(EstimateEquivalentSquareForContours(make_collection({ make_bowtie() })), std::invalid_argument);
}

TEST_CASE("ContourCollectionsHaveIntersections rejects crossing contours"){
    const auto cc = make_collection({ make_rectangle(10.0, 10.0), make_rectangle(10.0, 10.0, 5.0, 5.0) });
    CHECK(ContourCollectionsHaveIntersections(cc));
    CHECK_THROWS_AS(EstimateEquivalentSquareForContours(cc), std::invalid_argument);
}

TEST_CASE("ContourCollectionsHaveIntersections rejects T-intersections"){
    const auto cc = make_collection({ make_rectangle(10.0, 10.0), make_rectangle(5.0, 5.0, 5.0, 2.5) });
    CHECK(ContourCollectionsHaveIntersections(cc));
    CHECK_THROWS_AS(EstimateEquivalentSquareForContours(cc), std::invalid_argument);
}
