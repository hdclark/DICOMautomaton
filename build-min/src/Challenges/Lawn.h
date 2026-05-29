// Lawn.h.

#pragma once

#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>

#include "YgorMath.h"
#include "YgorMisc.h"

#include "../imgui20210904/imgui.h"

constexpr double lg_default_tower_hp = 300.0;
constexpr double lg_default_enemy_hp = 80.0;
constexpr double lg_default_projectile_damage = 12.0;

class LawnGame {
  public:
    LawnGame();
    bool Display(bool &enabled);
    void Reset();

  private:
    enum class lg_tower_type_t {
        Sun = 0,
        Shooter = 1,
        Blocker = 2
    };

    struct lg_tower_t {
        int64_t lane = 0;
        int64_t col = 0;
        lg_tower_type_t type = lg_tower_type_t::Sun;
        double hp = lg_default_tower_hp;
        double max_hp = lg_default_tower_hp;
        double cooldown = 0.0;
        double sun_production_time = 10.0; // Randomized production time for sun towers (8-15 seconds)
        double attack_animation_timer = 0.0; // Timer for attack animation
        double damage_animation_timer = 0.0; // Timer for being-attacked animation
    };

    struct lg_enemy_t {
        int64_t lane = 0;
        double x = 0.0;
        double hp = lg_default_enemy_hp;
        double max_hp = lg_default_enemy_hp;
        double attack_cooldown = 0.0;
        double speed_multiplier = 1.0; // Speed scaling based on spawn count
        double attack_animation_timer = 0.0; // Timer for attack animation
        double damage_animation_timer = 0.0; // Timer for being-attacked animation
    };

    struct lg_projectile_t {
        int64_t lane = 0;
        double x = 0.0;
        double damage = lg_default_projectile_damage;
        double speed = 200.0;
    };

    struct lg_token_t {
        vec2<double> pos;
        double radius = 10.0;
        std::chrono::time_point<std::chrono::steady_clock> created;
    };

    std::vector<lg_tower_t> lg_towers;
    std::vector<lg_enemy_t> lg_enemies;
    std::vector<lg_projectile_t> lg_projectiles;
    std::vector<lg_token_t> lg_tokens;
    std::chrono::time_point<std::chrono::steady_clock> t_lg_updated;

    struct lg_game_t {
        int64_t lanes = 5;
        int64_t cols = 9;
        double lane_height = 70.0;
        double cell_width = 70.0;
        double board_width = 0.0;
        double board_height = 0.0;

        int64_t tokens = 5;
        int64_t lives = 10;
        double spawn_timer = 0.0;
        double spawn_interval = 10.0;
        double enemy_speed = 15.0;
        int64_t enemy_spawn_count = 0; // Track total enemies spawned for difficulty scaling

        lg_tower_type_t selected_tower = lg_tower_type_t::Sun;

        std::mt19937 re;
    } lg_game;

};
    
