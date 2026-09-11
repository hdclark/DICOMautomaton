// Contour_Mask_Tests.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include "doctest20251212/doctest.h"

#include "Contour_Mask.h"
#include "GIS.h"

using namespace dcma::mask_contours;

TEST_CASE("MaskContours parses named multi-polygon regions"){
    const auto rs = parse_regions(
        "Region(park){Polygon(0,0, 10,0, 10,10, 0,10); Polygon(20,20, 21,20, 21,21, 20,21)}"
    );
    REQUIRE(rs.size() == 1);
    CHECK(rs.front().name == "park");
    CHECK(rs.front().polygons.size() == 2);
    CHECK(point_in_region_xy(vec3<double>(5.0,5.0,0.0), rs.front()));
    CHECK(point_in_region_xy(vec3<double>(20.5,20.5,0.0), rs.front()));
    CHECK_FALSE(point_in_region_xy(vec3<double>(15.0,15.0,0.0), rs.front()));
}

TEST_CASE("MaskContours splits a path into exact inside and outside portions"){
    const auto r = parse_regions("Region(square){Polygon(-1,-1, 1,-1, 1,1, -1,1)}").front();
    const std::vector<vec3<double>> path = { vec3<double>(-2.0,0.0,0.0), vec3<double>(2.0,0.0,0.0) };
    const auto atoms = atomize_path(path, false, r);
    REQUIRE(atoms.size() == 3);
    CHECK_FALSE(atoms.at(0).inside);
    CHECK(atoms.at(1).inside);
    CHECK_FALSE(atoms.at(2).inside);
    CHECK(atoms.at(0).length() == doctest::Approx(1.0));
    CHECK(atoms.at(1).length() == doctest::Approx(2.0));
    CHECK(atoms.at(2).length() == doctest::Approx(1.0));
}

TEST_CASE("MaskContours tags every derived piece for downstream filtering"){
    const auto r = parse_regions("Region(square){Polygon(-1,-1, 1,-1, 1,1, -1,1)}").front();
    contour_of_points<double> source;
    source.closed = false;
    source.metadata["SourceTag"] = "preserved";
    source.points.emplace_back(vec3<double>(-2.0,0.0,0.0));
    source.points.emplace_back(vec3<double>(2.0,0.0,0.0));

    const auto pieces = slice_contour(source, r, 0.0);
    REQUIRE(pieces.size() == 3);
    for(const auto &piece : pieces){
        CHECK(piece.metadata.at("MaskContoursRegion") == "square");
        CHECK((piece.metadata.at("MaskContoursState") == "inside" || piece.metadata.at("MaskContoursState") == "outside"));
        CHECK(piece.metadata.at("SourceTag") == "preserved");
    }
}

TEST_CASE("MaskContours debounce absorbs a short leave-and-return excursion"){
    const auto r = parse_regions("Region(square){Polygon(-1,-1, 1,-1, 1,1, -1,1)}").front();
    const std::vector<vec3<double>> path = {
        vec3<double>(0.0,0.0,0.0), vec3<double>(1.10,0.0,0.0), vec3<double>(0.0,0.0,0.0)
    };
    auto atoms = atomize_path(path, false, r);
    REQUIRE(atoms.size() >= 3);
    bool had_outside = false;
    for(const auto &a : atoms) had_outside = had_outside || !a.inside;
    REQUIRE(had_outside);

    debounce_atoms(atoms, 0.25);
    for(const auto &a : atoms) CHECK(a.inside);
}

TEST_CASE("MaskContours accepts illustrative lower-mainland BC riding regions"){
    const auto rs = parse_regions(
        "Region(Fromme){GeoPolygon(49.327,-123.104, 49.392,-123.104, 49.392,-123.036, 49.327,-123.036)};"
        "Region(Seymour){GeoPolygon(49.315,-123.010, 49.390,-123.010, 49.390,-122.925, 49.315,-122.925)};"
        "Region(EagleMountain){GeoPolygon(49.275,-122.875, 49.345,-122.875, 49.345,-122.790, 49.275,-122.790)}"
    );
    REQUIRE(rs.size() == 3);
    CHECK(rs.at(0).name == "Fromme");
    CHECK(rs.at(1).name == "Seymour");
    CHECK(rs.at(2).name == "EagleMountain");
}

TEST_CASE("MaskContours treats multiple polygons as a union"){
    const auto rs = parse_regions(
        "Region(west){Polygon(-4,-1, -2,-1, -2,1, -4,1)};"
        "Region(east){Polygon(2,-1, 4,-1, 4,1, 2,1)}"
    );
    mask_region combined;
    combined.name = rs.at(0).name + ";" + rs.at(1).name;
    combined.polygons = rs.at(0).polygons;
    combined.polygons.insert(combined.polygons.end(), rs.at(1).polygons.begin(), rs.at(1).polygons.end());

    const std::vector<vec3<double>> path = { vec3<double>(-5.0,0.0,0.0), vec3<double>(5.0,0.0,0.0) };
    const auto atoms = atomize_path(path, false, combined);
    REQUIRE(atoms.size() == 5);
    CHECK_FALSE(atoms.at(0).inside);
    CHECK(atoms.at(1).inside);
    CHECK_FALSE(atoms.at(2).inside);
    CHECK(atoms.at(3).inside);
    CHECK_FALSE(atoms.at(4).inside);
}

TEST_CASE("MaskContours resolves predefined trail regions by canonical name"){
    const auto rs = parse_regions(
        "NamedRegion(Fromme);NamedRegion(EagleMountain);NamedRegion(whistler-mountain-bike-park)"
    );
    REQUIRE(rs.size() == 3);
    CHECK(rs.at(0).name == "Fromme");
    CHECK(rs.at(1).name == "Eagle Mountain");
    CHECK(rs.at(2).name == "Whistler Mountain Bike Park");
    for(const auto &region : rs){
        REQUIRE(region.polygons.size() == 1);
        CHECK(region.polygons.front().vertices.size() > 4);
    }

    const auto fromme_xy = dcma::gis::project_mercator(49.355, -123.075);
    CHECK(point_in_region_xy(vec3<double>(fromme_xy.first, fromme_xy.second, 0.0), rs.front()));
    CHECK(parse_regions("NamedRegion(Seymour)").front().name == "Seymour Mountain");
    CHECK_THROWS_AS(parse_regions("NamedRegion(not-a-real-place)"), std::invalid_argument);
}

TEST_CASE("MaskContours provides all documented predefined trail regions"){
    const auto rs = parse_regions(
        "NamedRegion(Fromme);NamedRegion(Burke);NamedRegion(Eagle Mountain);"
        "NamedRegion(Cypress Mountain);NamedRegion(Grouse Mountain Bike Park);NamedRegion(Seymour Mountain);"
        "NamedRegion(Burnaby Mountain);NamedRegion(Delta Watershed);NamedRegion(Bert Flinn Park);"
        "NamedRegion(Thornhill);NamedRegion(Woodlot 0007);NamedRegion(Bear Mountain);NamedRegion(Red Mountain);"
        "NamedRegion(Sumas Mountain);NamedRegion(Vedder Mountain);NamedRegion(Ledgeview);"
        "NamedRegion(Whistler Mountain Bike Park);NamedRegion(Mount Washington Bike Park);"
        "NamedRegion(Revelstoke Bike Park);NamedRegion(Kicking Horse Bike Park);NamedRegion(Sun Peaks Bike Park);"
        "NamedRegion(Kamloops Bike Ranch);NamedRegion(Winsport Bike Park);NamedRegion(Hinton Bike Park)"
    );
    CHECK(rs.size() == 24);
}
