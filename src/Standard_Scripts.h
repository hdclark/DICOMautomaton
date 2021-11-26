//Standard_Scripts.h.

#pragma once

#include <istream>
#include <ostream>
#include <list>
#include <set>
#include <filesystem>

#include "Structs.h"

// Some 'standard' scripts.
struct standard_script_t {
    std::string category;
    std::string name;
    std::string text;
};
std::list<standard_script_t> standard_scripts();

std::list<standard_script_t> standard_scripts_with_category(const std::string &category);

std::set<std::string> standard_script_categories();


