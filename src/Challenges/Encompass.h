// Encompass_Game.h.

#pragma once

#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>

#include "YgorMath.h"
#include "YgorMisc.h"

#include "../imgui20210904/imgui.h"

class EncompassGame {
  public:
    EncompassGame();
    bool Display(bool &enabled);
    void Reset();

  private:

    struct en_game_obj_t {
        vec2<double> pos; // position.
        vec2<double> vel; // velocity.
        double rad;       // radius, which also implies mass since we assume constant mass density.
        bool player_controlled = false;
    };

    std::vector< en_game_obj_t > en_game_objs;
    std::chrono::time_point<std::chrono::steady_clock> t_en_updated;
    std::chrono::time_point<std::chrono::steady_clock> t_en_started;

    struct en_game_t {
        int64_t N_objs = 250L;        // Initial configuration target number of objects.

        double min_radius = 3.0;      // Objects cannot be smaller than this.
        double max_radius = 60.0;     // Only used for initial configuration.

        double box_width  = 1000.0;   // World bounds.
        double box_height = 800.0;

        double max_speed = 25.0;      // The maximum speed an object can attain.
                                      // It's possible that objects might temporarily be faster, so consider
                                      // the upper limit to be slightly higher in practice.

        double mutiny_period = 300.0; // Relates to how often some mass 'leaks' spontaneously.
        double mutiny_slope = 75.0;   // Relates to likelihood of mutiny due to area (logistic slope).
        double mutiny_mid = 100.0;    // Relates to likelihood of mutiny due to area (logistic midpoint).

        std::mt19937 re;
    } en_game;

};
