// RotatingCubeGame.h.

#pragma once

#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>

#include "YgorMath.h"
#include "YgorMisc.h"

#include "../imgui20210904/imgui.h"
#include "../Rotating_Cube.h"


// Wraps the imgui SliderInt() function to better handle int64_t and other non-'int' inputs.
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
    
