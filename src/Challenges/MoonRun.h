// MoonRun.h.

#pragma once

#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>

#include "YgorMath.h"
#include "YgorMisc.h"

#include "../imgui20210904/imgui.h"

// Controls:
//   - Space/Up arrow: jump
//   - Down arrow: crouch
//   - R key: reset the game
//
// Gameplay:
//   - The runner stays in place while the moon surface scrolls by
//   - Jump over craters and duck under aliens/projectiles
//   - Speed increases over time to raise the challenge
//   - Score increases for each obstacle cleared
//
// Visual elements:
//   - Rotating moon backdrop with craters
//   - Running stick figure with animated poses
//   - Ground debris and floating space rocks
//

class MoonRunGame {
  public:
    MoonRunGame();
    bool Display(bool &enabled);
    void Reset();

  private:
    enum class mr_obstacle_type_t {
        crater,
        aerial
    };

    enum class mr_aerial_variant_t {
        alien,
        projectile
    };

    struct mr_obstacle_t {
        vec2<double> pos;
        double width = 0.0;
        double height = 0.0;
        mr_obstacle_type_t type = mr_obstacle_type_t::crater;
        mr_aerial_variant_t variant = mr_aerial_variant_t::alien;
        bool scored = false;
    };

    struct mr_moon_feature_t {
        double angle = 0.0;
        double radius = 0.0;
        double size = 0.0;
        bool crater = true;
    };

    struct mr_debris_t {
        vec2<double> pos;
        vec2<double> vel;
        double size = 1.0;
    };

    struct mr_ground_rock_t {
        double x = 0.0;
        double size = 0.0;
        double height = 0.0;
    };

    std::vector<mr_obstacle_t> mr_obstacles;
    std::vector<mr_moon_feature_t> mr_moon_features;
    std::vector<mr_debris_t> mr_debris;
    std::vector<mr_ground_rock_t> mr_ground_rocks;
    std::chrono::time_point<std::chrono::steady_clock> t_mr_updated;
    std::chrono::time_point<std::chrono::steady_clock> t_mr_started;

    struct mr_game_t {
        double box_width = 820.0;
        double box_height = 420.0;
        double ground_height = 70.0;
        double ground_y = 0.0;

        double runner_x = 150.0;
        double runner_width = 26.0;
        double runner_height = 70.0;
        double runner_crouch_height = 42.0;

        bool is_jumping = false;
        bool is_crouching = false;
        double jump_height = 0.0;
        double jump_velocity = 0.0;
        double jump_speed = 320.0;
        double gravity = 900.0;

        double run_phase = 0.0;
        double run_phase_speed = 10.0;

        double scroll_speed = 220.0;
        double max_scroll_speed = 900.0;
        double speed_increase_rate = 20.0;

        double crater_spawn_timer = 0.0;
        double aerial_spawn_timer = 0.0;

        double moon_radius = 0.0;
        double moon_rotation = 0.0;
        double moon_rotation_speed = 0.12;

        bool game_over = false;
        double game_over_time = 0.0;
        int64_t score = 0;

        std::mt19937 re;
    } mr_game;
};
