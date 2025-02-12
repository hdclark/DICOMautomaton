// Rotating_Cube.h.
#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <tuple>

struct rc_cell_t {
    int64_t colour = 0;
};

enum class rc_direction : int64_t {
    left         = 0,
    right        = 1,
    down         = 2,
    up           = 3,

    rotate_left  = 4,
    rotate_right = 5,

    highest      = 6,
};


struct rc_game_t {
    int64_t N = 3; // Number of cells along a cartesian direction.
    std::vector<rc_cell_t> cells;

    using coords_t = std::tuple<int64_t,   // face number in [0,5].
                                int64_t,   // cell 'x' in [0,N-1].
                                int64_t>;  // cell 'y' in [0,N-1].


    rc_game_t();
    void reset(int64_t N);

    int64_t index(const coords_t &c) const;
    void assert_index_valid(int64_t index) const;
    bool confirm_index_valid(int64_t index) const;

    coords_t coords(int64_t index) const;

    const rc_cell_t& get_const_cell(int64_t index) const;
    rc_cell_t& get_cell(int64_t index);

    int64_t get_N() const; // Use reset() to set N.

    std::array<float,4> colour_to_rgba(int64_t colour) const;

    std::tuple<int64_t, rc_direction> get_neighbour_face(int64_t, rc_direction) const;

    std::tuple<rc_game_t::coords_t, rc_direction>
    get_neighbour_cell(std::tuple<rc_game_t::coords_t, rc_direction>) const;

    void move(std::tuple<rc_game_t::coords_t, rc_direction>);
    void implement_primitive_shift(std::tuple<rc_game_t::coords_t, rc_direction>);
    void implement_primitive_face_rotate(std::tuple<rc_game_t::coords_t, rc_direction>);

};

