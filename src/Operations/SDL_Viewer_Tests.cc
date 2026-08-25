// SDL_Viewer_Tests.cc.

#include "../doctest20251212/doctest.h"

#include <stdexcept>

#include "SDL_Viewer.h"

TEST_CASE("SDL_Viewer documents and parses configurable keyword colours"){
    const auto doc = OpArgDocSDL_Viewer();
    const OperationArgDoc *keyword_arg = nullptr;
    for(const auto &arg : doc.args) if(arg.name == "KeywordColours") keyword_arg = &arg;
    REQUIRE(keyword_arg != nullptr);
    CHECK(keyword_arg->expected);

    const auto defaults = ParseSDLViewerKeywordColours(keyword_arg->default_val);
    REQUIRE(defaults.size() == 4U);
    CHECK(defaults.at("pass")[0] == doctest::Approx(0.175));
    CHECK(defaults.at("true")[1] == doctest::Approx(0.500));
    CHECK(defaults.at("fail")[0] == doctest::Approx(0.600));
    CHECK(defaults.at("false")[3] == doctest::Approx(1.0));

    const auto custom = ParseSDLViewerKeywordColours("keyword('Onsite staff',0.1,0.2,0.3,0.4);keyword(Remote,1,0,0,1)");
    REQUIRE(custom.size() == 2U);
    CHECK(custom.at("Onsite staff")[2] == doctest::Approx(0.3));
    CHECK(ParseSDLViewerKeywordColours("").empty());
}

TEST_CASE("SDL_Viewer rejects malformed keyword colours"){
    CHECK_THROWS_AS(ParseSDLViewerKeywordColours("colour(red,1,0,0,1)"), std::invalid_argument);
    CHECK_THROWS_AS(ParseSDLViewerKeywordColours("keyword(red,1,0,0)"), std::invalid_argument);
    CHECK_THROWS_AS(ParseSDLViewerKeywordColours("keyword('',1,0,0,1)"), std::invalid_argument);
    CHECK_THROWS_AS(ParseSDLViewerKeywordColours("keyword(red,1.1,0,0,1)"), std::invalid_argument);
    CHECK_THROWS_AS(ParseSDLViewerKeywordColours("keyword(red,nope,0,0,1)"), std::invalid_argument);
    CHECK_THROWS_AS(ParseSDLViewerKeywordColours("keyword(red,1,0,0,1);keyword(red,0,1,0,1)"), std::invalid_argument);
}
