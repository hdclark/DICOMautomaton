// MazeExplorer.h.

#pragma once

#include <vector>
#include <deque>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>

#include "YgorMath.h"
#include "YgorMisc.h"

#include "../imgui20210904/imgui.h"

// Controls:
//   - W/A/S/D: move forward/left/back/right
//   - Arrow keys: rotate view
//   - Spacebar: jump
//   - E/Q: climb stairs up/down (when standing on a stair tile)
//   - R key: regenerate the maze
//
// Gameplay:
//   - Explore a multi-floor maze rendered in a 2.5D raycast style
//   - Find the floating, pulsating relic to complete the level
//   - The timer tracks how long it takes to reach the relic
//   - No enemies or game over state; explore at your own pace
//
// Visual elements:
//   - Doom-like vertical wall slices with distance shading
//   - Camera bobbing and a tiny player figure animate while walking/jumping
//   - The relic glows, floats, and pulses when visible
//

class MazeExplorerGame {
  public:
    MazeExplorerGame();
    bool Display(bool &enabled);
    void Reset();

  private:
    struct cell_t {
        bool wall = true;
        bool stair_up = false;
        bool stair_down = false;
    };

    struct floor_t {
        std::vector<cell_t> cells;
    };

    struct maze_coord_t {
        int64_t floor = 0;
        int64_t x = 0;
        int64_t y = 0;
    };

    struct maze_game_t {
        double box_width = 720.0;
        double box_height = 420.0;

        int64_t grid_cols = 23;
        int64_t grid_rows = 17;
        int64_t num_floors = 3;

        double fov = 1.05; // ~60 degrees in radians.
        double max_view_distance = 18.0;

        double move_speed = 2.5;
        double rot_speed = 1.8;

        double jump_speed = 4.5;
        double gravity = 9.5;

        bool level_complete = false;
        double completion_time = 0.0;

        bool is_jumping = false;
        double jump_height = 0.0;
        double jump_velocity = 0.0;

        double walk_phase = 0.0;

        std::mt19937 re;
    } maze_game;

    std::vector<floor_t> floors;
    maze_coord_t start_cell;
    maze_coord_t goal_cell;
    vec2<double> player_pos;
    int64_t player_floor = 0;
    double player_angle = 0.0;

    std::chrono::time_point<std::chrono::steady_clock> t_updated;
    std::chrono::time_point<std::chrono::steady_clock> t_started;

    void GenerateLevel();
    void GenerateMazeFloor(int64_t floor_idx);
    maze_coord_t RandomWalkableCell(int64_t floor_idx);
    std::vector<int64_t> ComputeDistances(const maze_coord_t &start) const;

    bool InBounds(int64_t x, int64_t y) const;
    bool IsWall(int64_t floor_idx, int64_t x, int64_t y) const;
    bool IsStairUp(int64_t floor_idx, int64_t x, int64_t y) const;
    bool IsStairDown(int64_t floor_idx, int64_t x, int64_t y) const;

    size_t CellIndex(int64_t x, int64_t y) const;
    size_t FlatIndex(const maze_coord_t &coord) const;

    double CastRay(const vec2<double> &pos, double angle, int64_t floor_idx, int &hit_side) const;
    double NormalizeAngle(double angle) const;
    double AngleDelta(double angle) const;
};
