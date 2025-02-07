// Rotating_Cube.h.
#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <tuple>

struct rc_face_t {
    int64_t colour = 0;
};

enum class rc_direction {
    left,
    right,
    down,
    up,
    rotate_left,
    rotate_right,
};


struct rc_game_t {
    int64_t N = 3; // Number of faces along a cartesian direction.
    std::vector<rc_face_t> faces;

    using coords_t = std::tuple<int64_t,   // face number in [0,5].
                                int64_t,   // grid 'x' in [0,N-1].
                                int64_t>;  // grid 'y' in [0,N-1].


    rc_game_t();
    void reset(int64_t N);

    int64_t index(const coords_t &c) const;
    void assert_index_valid(int64_t index) const;
    bool confirm_index_valid(int64_t index) const;

    coords_t coords(int64_t index) const;

    const rc_face_t& get_const_face(int64_t index) const;
    rc_face_t& get_face(int64_t index);

    int64_t get_N() const; // Use reset() to set N.

    std::array<float,4> colour_to_rgba(int64_t colour) const;

    // get_neighbours ?

};

