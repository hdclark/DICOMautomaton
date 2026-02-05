// TacticalStealth.h.

#pragma once

#include <vector>
#include <random>
#include <chrono>
#include <deque>
#include <algorithm>
#include <cmath>

#include "YgorMath.h"
#include "YgorMisc.h"

#include "../imgui20210904/imgui.h"

// Controls:
//   - Arrow keys: move the player circle through the maze
//   - Spacebar (hold): move slowly and quietly (enemies can't hear you)
//   - B key: don a cardboard box (once per round, hides for 5 seconds)
//   - R key: reset the game
//
// Gameplay:
//   - Avoid being spotted by enemy squares patrolling the maze
//   - Enemies have a visible light cone field of view
//   - Hide for the required time to advance to the next level
//   - If spotted, enemies will chase you at 2x speed
//   - If an enemy catches you (within 2x diameter), game over
//   - Enemies can also detect you by 'hearing' if you move too close
//   - Hold spacebar to move quietly and avoid being heard
//   - When detected, an exclamation mark appears above the enemy
//   - Press B to hide under a cardboard box - enemies cannot see, hear, or catch you
//   - The cardboard box can only be used once per round
//   - Hover over enemies to see their patrol path and status
//
// Level progression:
//   - Each level increases enemy speed and FOV by 20%
//   - Hide time increases by 5 seconds per level
//   - One additional enemy per level
//   - Maze layout changes each level
//

class TacticalStealthGame {
  public:
    TacticalStealthGame();
    bool Display(bool &enabled);
    void Reset();

  private:
    void GenerateMaze();
    void PlaceEnemies();
    void PlacePlayer();
    bool IsWall(double x, double y) const;
    bool IsBlocked(double x, double y, double radius) const;  // Check if position blocked by walls or bounds
    vec2<double> TryMoveWithSlide(const vec2<double> &pos, const vec2<double> &desired_move, double radius) const;
    bool LineOfSight(const vec2<double> &from, const vec2<double> &to) const;
    bool IsInFieldOfView(const vec2<double> &enemy_pos, double enemy_facing, 
                         double fov_angle, double fov_range, 
                         const vec2<double> &target) const;
    std::vector<vec2<double>> FindPath(const vec2<double> &from, const vec2<double> &to) const;  // BFS pathfinding

    enum class ts_enemy_state_t {
        Patrolling,     // Following patrol path
        Alerted,        // Heard something, looking around
        Pursuing,       // Chasing player
        Returning       // Returning to patrol after losing player
    };

    struct ts_enemy_t {
        vec2<double> pos;
        double facing = 0.0;  // Angle in radians
        std::vector<vec2<double>> patrol_path;
        size_t patrol_idx = 0;
        bool patrol_forward = true;
        ts_enemy_state_t state = ts_enemy_state_t::Patrolling;
        double alert_timer = 0.0;      // Time remaining in alerted state
        double pursuit_timer = 0.0;    // Time spent pursuing
        double look_timer = 0.0;       // Timer for look-around pause
        bool is_looking = false;       // Currently pausing to look
        double exclaim_timer = 0.0;    // Timer for exclamation mark display
        double walk_anim = 0.0;        // Walking animation phase
        vec2<double> last_known_player_pos;
    };

    struct ts_game_t {
        // Game area
        double box_width = 600.0;
        double box_height = 500.0;
        
        // Grid-based maze
        int64_t grid_cols = 21;
        int64_t grid_rows = 17;
        double cell_size = 30.0;
        std::deque<bool> walls; // true = wall, false = walkable
        
        // Player
        double player_radius = 8.0;
        double base_speed = 60.0;
        double quiet_speed_mult = 0.4;  // Speed multiplier when sneaking
        double walk_anim = 0.0;         // Walking animation phase
        
        // Enemy properties
        double enemy_size = 12.0;
        double base_fov_angle = 0.8;      // Base field of view half-angle in radians (~45 deg)
        double base_fov_range = 80.0;     // Base field of view range in pixels
        double hearing_range = 120.0;     // Base range to hear player
        double pursuit_speed_mult = 1.5;  // Speed multiplier when pursuing
        double pursuit_timeout = 15.0;    // Seconds before giving up pursuit
        double alert_duration = 3.0;      // Seconds to stay alerted
        double look_interval_min = 2.0;   // Min time between look pauses
        double look_interval_max = 5.0;   // Max time between look pauses
        double look_duration = 1.0;       // Duration of look pause
        double exclaim_duration = 1.0;    // Duration of exclamation display
        double catch_distance_mult = 2.0; // Catch distance as multiple of player diameter
        
        // Game state
        int64_t level = 1;
        int64_t score = 0;
        double hide_time_base = 10.0;      // Base hide time for level 1
        double hide_time_increment = 5.0;  // Additional hide time per level
        double current_hide_timer = 0.0;
        double level_complete_timer = 0.0;
        bool game_over = false;
        bool level_complete = false;
        bool countdown_active = true;
        double countdown_remaining = 3.0;
        
        // Level scaling
        double speed_scale_per_level = 0.10;  // 10% increase per level
        double fov_scale_per_level = 0.05;    // 5% increase per level
        int64_t base_enemies = 2;             // Starting number of enemies
        
        std::mt19937 re;
    } ts_game;

    vec2<double> player_pos;
    bool player_sneaking = false;
    std::vector<ts_enemy_t> enemies;

    // Cardboard box mechanic
    bool box_available = true;        // Can only use once per round
    bool box_active = false;          // Currently wearing box
    double box_timer = 0.0;           // Time remaining in box
    double box_anim_timer = 0.0;      // Animation timer for donning/doffing
    enum class box_state_t {
        Inactive,
        Donning,      // Putting on the box
        Active,       // Fully inside box
        Doffing       // Taking off the box
    } box_state = box_state_t::Inactive;
    static constexpr double box_duration = 5.0;      // Duration inside box
    static constexpr double box_anim_duration = 0.5; // Duration of don/doff animation

    std::chrono::time_point<std::chrono::steady_clock> t_ts_updated;
};

