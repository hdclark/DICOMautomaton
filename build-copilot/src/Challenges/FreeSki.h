// FreeSki.h.

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
//   - Left/Right arrow keys: move skier left/right
//   - Spacebar: jump over obstacles
//   - Double-tap spacebar (while in air): perform a flip for double points
//   - R key: reset the game
//
// Gameplay:
//   - The skier continuously moves downward (the world scrolls upward)
//   - Speed gradually increases over time until maximum is reached
//   - Avoid obstacles: trees, rocks, and other skiers
//   - Jump over rocks to score points (1 point normal, 2 points if flipping)
//   - Hit jumps while in the air to score points
//   - Hit jumps while on the ground for a speed boost
//   - Colliding with trees, rocks (when not jumping), or other skiers ends the game
//   - On game over, the screen twirls/jitters until reset
//   - After reset, a 3-second countdown gives the player time to prepare
//
// Visual elements:
//   - Player: red triangle (orange while jumping)
//   - Trees: green triangles
//   - Rocks: gray circles
//   - Jumps: brown/yellow ramps
//   - Other skiers: cyan triangles
//

class FreeSkiGame {
  public:
    FreeSkiGame();
    bool Display(bool &enabled);
    void Reset();

  private:
    enum class fs_obj_type_t {
        skier,     // The player
        tree,      // Obstacle - collision ends game
        rock,      // Obstacle - collision ends game, can be jumped over
        jump,      // Speed boost when hit, awards points when jumped
        other_skier // Obstacle - collision ends game
    };
    
    struct fs_game_obj_t {
        vec2<double> pos;      // position
        fs_obj_type_t type;
        double size;           // radius/size for rendering
    };
    
    std::vector< fs_game_obj_t > fs_game_objs;
    std::chrono::time_point<std::chrono::steady_clock> t_fs_updated;
    std::chrono::time_point<std::chrono::steady_clock> t_fs_started;
    std::chrono::time_point<std::chrono::steady_clock> t_fs_last_spacebar;
    
    struct fs_game_t {
        double box_width  = 800.0;    // World bounds width
        double box_height = 600.0;    // World bounds height (visible area)
        
        double skier_x = 400.0;       // Skier x position (can move left/right)
        double skier_y = 100.0;       // Skier y position (fixed on screen, world scrolls)
        double skier_size = 12.0;     // Skier triangle size
        
        double scroll_speed = 50.0;   // Initial downward scroll speed (pixels/sec)
        double max_scroll_speed = 1500.0; // Maximum scroll speed
        double speed_increase_rate = 3.0; // Speed increase per second
        
        double world_scroll_y = 0.0;  // How far we've scrolled down
        
        bool is_jumping = false;
        double jump_height = 0.0;     // Current height above ground
        double jump_velocity = 0.0;   // Vertical velocity during jump
        double jump_max_height = 80.0;
        double jump_speed = 200.0;    // Initial upward velocity
        
        bool did_flip = false;        // Whether player did a flip during this jump
        bool can_double_tap = false;  // Whether player can still double-tap
        
        bool game_over = false;
        double game_over_time = 0.0;  // Time since game over
        
        bool countdown_active = true;
        double countdown_remaining = 3.0; // Seconds until game starts
        
        int64_t score = 0;
        
        double tree_spawn_rate = 1.53;          // Average trees per second
        double rock_spawn_rate = 1.05;          // Average rocks per second  
        double jump_spawn_rate = 0.31;         // Average jumps per second
        double other_skier_spawn_rate = 0.213; // Average other skiers per second
        
        double last_tree_spawn = 0.0;
        double last_rock_spawn = 0.0;
        double last_jump_spawn = 0.0;
        double last_other_skier_spawn = 0.0;
        
        std::mt19937 re;
    } fs_game;

};
    
