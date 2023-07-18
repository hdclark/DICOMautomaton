//Standard_Guides.h.

#pragma once

#include <istream>
#include <ostream>
#include <list>
#include <set>
#include <vector>
#include <filesystem>

#include "Structs.h"


// Some 'standard' guides.
struct standard_guide_t {
    std::string category;
    std::string name;
    std::string text;
};
std::list<standard_guide_t> standard_guides();

std::list<standard_guide_t> standard_guides_with_category(const std::string &category);

std::set<std::string> standard_guide_categories();

// A single stage/step of a guide.
struct guide_stage_t {
    std::string message;
    std::list<std::string> base64_images;
};

std::list<guide_stage_t> parse_guide(const std::string &text);


