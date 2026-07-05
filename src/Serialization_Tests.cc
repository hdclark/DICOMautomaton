#include <list>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest20251212/doctest.h"

#include "YgorSerialize.h"

struct SampleState {
    int value = 0;
    std::string label;
    std::vector<int> counts;
    std::list<std::string> tags;
    std::map<std::string, int> metadata;
};


TEST_CASE("serialization round-trips nested state") {
    SampleState in;
    in.value = 7;
    in.label = "demo";
    in.counts = {1, 2, 3};
    in.tags = {"alpha", "beta"};
    in.metadata = {{"one", 1}, {"two", 2}};

    std::stringstream ss;
    {
        boost::archive::text_oarchive ar(ss);
        ar & boost::serialization::make_nvp("state.value", in.value)
           & boost::serialization::make_nvp("state.label", in.label)
           & boost::serialization::make_nvp("state.counts", in.counts)
           & boost::serialization::make_nvp("state.tags", in.tags)
           & boost::serialization::make_nvp("state.metadata", in.metadata);
    }

    SampleState out;
    ss.clear();
    ss.seekg(0);
    {
        boost::archive::text_iarchive ar(ss);
        ar & boost::serialization::make_nvp("state.value", out.value)
           & boost::serialization::make_nvp("state.label", out.label)
           & boost::serialization::make_nvp("state.counts", out.counts)
           & boost::serialization::make_nvp("state.tags", out.tags)
           & boost::serialization::make_nvp("state.metadata", out.metadata);
    }

    CHECK(out.value == 7);
    CHECK(out.label == "demo");
    CHECK(out.counts == std::vector<int>({1, 2, 3}));
    CHECK(out.tags == std::list<std::string>({"alpha", "beta"}));
    CHECK(out.metadata == std::map<std::string, int>({{"one", 1}, {"two", 2}}));
}
