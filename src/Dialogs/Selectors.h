// Selectors.h -- dialogs for user input.

#pragma once

#include <map>
#include <string>
#include <vector>
#include <initializer_list>
#include <functional>
#include <chrono>
#include <filesystem>
#include <any>

std::string
select_directory(std::string query_text);

