// RotatingCubeGame.h.

#pragma once

#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <tuple>

#include "YgorMath.h"
#include "YgorMisc.h"

#include "../imgui20210904/imgui.h"


// Helper that wraps the imgui SliderInt() function to better handle int64_t and other non-'int' inputs.
//
// Also enforces the lower and upper bounds, which the user can otherwise overcome via CTRL+click
// on the slider widget.
//
// Invokes the user functor, if valid, and returns true when the number changes.
template <class T>
bool
imgui_slider_int_wrapper( const std::string &desc,
                          T &i,
                          const T lower_bound_inclusive,
                          const T upper_bound_inclusive,
                          const std::function<void(void)> &f = std::function<void(void)>() ){
    auto int_i = static_cast<int>(i);
    const auto orig_int_i = int_i;

    auto int_lb = static_cast<int>(lower_bound_inclusive);
    auto int_ub = static_cast<int>(upper_bound_inclusive);

    ImGui::SliderInt(desc.c_str(), &int_i, int_lb, int_ub);
    int_i = std::clamp(int_i, int_lb, int_ub);
    const bool has_changed = (int_i != orig_int_i);

    i = static_cast<T>(int_i);
    if(has_changed && f) f();
    return has_changed;
}


// ------------------------------------------------------------------------------------
// Cube representation.
// ------------------------------------------------------------------------------------
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

    using move_t = std::tuple<rc_game_t::coords_t, rc_direction>;

    //-----

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

    move_t get_neighbour_cell(move_t) const;

    void move(move_t);
    void implement_primitive_shift(move_t);
    void implement_primitive_face_rotate(move_t);

    std::vector<move_t> generate_random_moves(int64_t) const;

};


// ------------------------------------------------------------------------------------
// Game built around the representation.
// ------------------------------------------------------------------------------------
class RotatingCubeGame {
  public:
    RotatingCubeGame();
    bool Display(bool &enabled);
    void Reset();

    void append_history(rc_game_t::move_t x);
    void jump_to_history(int64_t n);

  private:
    std::chrono::time_point<std::chrono::steady_clock> t_cube_updated;

    int64_t rc_game_size = 4L;
    rc_game_t rc_game;
    double rc_game_anim_dt = 350.0; // in ms.

    std::list<rc_game_t::move_t> rc_game_move_history;
    int64_t rc_game_move_history_current = 0L; // Zero = implicit game reset (i.e., no moves yet).

    std::map<int64_t, int64_t> rc_game_colour_map; // index to colour number.

};
    
