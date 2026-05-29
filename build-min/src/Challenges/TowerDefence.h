// TowerDefence.h.

#pragma once

#include <vector>
#include <set>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>

#include "YgorMath.h"
#include "YgorMisc.h"

#include "../imgui20210904/imgui.h"

class TowerDefenceGame {
  public:
    TowerDefenceGame();
    bool Display(bool &enabled);
    void Reset();

  private:
    struct td_enemy_t {
        double path_progress = 0.0;  // 0.0 = start, 1.0 = end of path.
        double hp = 25.0;           // Hit points.
        double max_hp = 25.0;       // Maximum hit points.
        double speed_multiplier = 1.0; // Speed multiplier for this enemy.
    };

    enum class td_tower_type_t {
        Basic = 0,      // Standard tower.
        RapidFire = 1,  // Lower damage, higher fire rate.
        Boom = 2        // Explosive projectiles with AOE damage.
    };

    struct td_tower_t {
        int64_t grid_x = 0;
        int64_t grid_y = 0;
        double range = 90.0;     // Attack range in pixels.
        double damage = 15.0;    // Damage per shot.
        double fire_rate = 2.0;  // Shots per second.
        double cooldown = 0.0;   // Time until next shot.
        int64_t level = 1;       // Tower level (1 = base, higher = upgraded).
        td_tower_type_t tower_type = td_tower_type_t::Basic;  // Tower type.
        double total_damage_dealt = 0.0;  // Cumulative lifetime damage dealt.
    };

    struct td_projectile_t {
        vec2<double> pos;
        vec2<double> target_pos;
        size_t target_enemy_idx;
        double speed = 600.0;
        double damage = 15.0;
        double explosion_radius = 0.0;  // For Boom towers: radius of AOE explosion (0 = single target).
        size_t source_tower_idx = 0;    // Index of the tower that fired this projectile.
    };

    std::vector<td_enemy_t> td_enemies;
    std::vector<td_tower_t> td_towers;
    std::vector<td_projectile_t> td_projectiles;
    std::chrono::time_point<std::chrono::steady_clock> t_td_updated;

    struct td_game_t {
        int64_t grid_cols = 15;       // Number of columns in the grid.
        int64_t grid_rows = 10;       // Number of rows in the grid.
        double cell_size = 50.0;      // Size of each cell in pixels.

        int64_t credits = 5;          // Number of towers the player can buy.
        int64_t wave_number = 0;      // Current wave number.
        int64_t enemies_in_wave = 0;  // Enemies remaining to spawn in this wave.
        int64_t lives = 10;           // Player lives.
        bool wave_active = false;     // Is a wave currently in progress?
        double spawn_timer = 0.0;     // Time until next enemy spawn.
        double spawn_interval = 1.0;  // Time between enemy spawns in seconds.
        double enemy_speed = 0.01;    // How fast enemies move (path progress per second).

        // Path waypoints define the white tiles path.
        // These are grid coordinates that form a path from top-left to bottom-right.
        std::vector<std::pair<int64_t, int64_t>> path_waypoints;

        // Set of path cells for O(1) lookup during rendering.
        std::set<std::pair<int64_t, int64_t>> path_cells_set;

        // Tower upgrade dialog state.
        bool show_upgrade_dialog = false;
        size_t upgrade_tower_idx = 0;  // Index of tower being upgraded.
    } td_game;

};
    
