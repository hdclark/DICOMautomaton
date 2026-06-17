//EstimateEquivalentSquare.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <cmath>
#include <iomanip>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>            //Needed for std::move.

#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"

#include "../Regex_Selectors.h"
#include "../Structs.h"

#include "EstimateEquivalentSquare.h"

#include "../doctest20251212/doctest.h"


double EstimateEquivalentSquareForContours(const contour_collection<double> &cc){
    double area = 0.0;
    double perimeter = 0.0;

    for(const auto &c : cc.contours){
        if(!c.closed){
            throw std::invalid_argument("All contours must be closed");
        }

        const auto c_area = std::abs(c.Get_Signed_Area(true));
        const auto c_perimeter = c.Perimeter();
        if(!std::isfinite(c_area) || !std::isfinite(c_perimeter)){
            throw std::invalid_argument("Encountered non-finite contour area or perimeter");
        }
        if(c_area <= 0.0 || c_perimeter <= 0.0){
            throw std::invalid_argument("Encountered degenerate contour with non-positive area or perimeter");
        }

        area += c_area;
        perimeter += c_perimeter;
    }

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

    const auto equivalent_square = EstimateEquivalentSquareForContours(cc_ROIs.front().get());

    std::ostringstream ss;
    ss << std::setprecision(std::numeric_limits<double>::max_digits10) << equivalent_square;
    InvocationMetadata[ParameterKey] = ss.str();

    YLOGINFO("Stored equivalent square '" << ss.str() << "' mm with key '" << ParameterKey << "'");

    return true;
}

namespace {
contour_of_points<double> make_rectangle(const double width, const double height, const double x_offset = 0.0){
    contour_of_points<double> c(std::list<vec3<double>>{
        vec3<double>(x_offset, 0.0, 0.0),
        vec3<double>(x_offset + width, 0.0, 0.0),
        vec3<double>(x_offset + width, height, 0.0),
        vec3<double>(x_offset, height, 0.0)
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
