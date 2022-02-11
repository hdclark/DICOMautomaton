// SDL_query.h -- dialogs for user interaction.

#pragma once

#include <map>
#include <string>
#include <vector>
#include <initializer_list>
#include <functional>
#include <chrono>
#include <filesystem>
#include <any>

enum class user_input_t {
    string,
    real,
    integer,
//    image,
//    mesh,
//    ...
};

struct user_query_packet_t {
    bool answered;
    std::string key;
    std::string query;
    user_input_t val_type;
    std::any val;
};

std::vector<user_query_packet_t>
interactive_query(std::vector<user_query_packet_t> qv);

