// GuitarFret.h.

#pragma once

#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>

#include "YgorMath.h"
#include "YgorMisc.h"

#include "../imgui20210904/imgui.h"


class GuitarFretGame {
  public:
    GuitarFretGame();
    bool Display(bool &enabled);
    void Reset();

  private:
    struct gf_note_t {
        int64_t lane;         // 0-3 for F1-F4
        double y_pos;         // Current vertical position (0.0 = top, 1.0 = hit zone)
        bool active;          // Whether note is still in play
        bool hit;             // Whether note was successfully hit
    };
    struct gf_game_t {
        std::vector<gf_note_t> notes;
        double note_speed = 0.4;            // Units per second (1.0 = full screen height)
        double hit_zone_norm = 0.9;         // Normalized hit zone position (0.0 = top, 1.0 = bottom)
        double hit_window_perfect = 0.05;   // Perfect hit if within this range of hit_zone_norm
        double hit_window_ok = 0.10;        // OK hit if within this range of hit_zone_norm
        double hit_window_distant = 0.20;   // Beyond this distance, the note can be ignored.
        int64_t score = 0;
        int64_t high_score = 0;
        int64_t low_score = 0;
        bool low_score_initialized = false; // Track if low_score has been set
        int64_t streak = 0;
        int64_t best_streak = 0;
        bool paused = false;
        int64_t difficulty = 1;             // 0 = easy, 1 = medium, 2 = hard, 3 = expert, 4 = ultimate
        double next_note_time = 0.0;        // Time until next note spawns
        double elapsed_time = 0.0;          // Total game time
        double score_linger_time = 250;     // Amount of time (ms) score changes linger on-screen.
        std::mt19937 re;

        double box_width = 400.0;
        double box_height = 600.0;
    } gf_game;
    std::chrono::time_point<std::chrono::steady_clock> t_gf_updated;
    std::array<bool, 4> gf_lane_pressed = {false, false, false, false};
    
    struct gf_lane_score_t {
        bool active = false;
        std::chrono::time_point<std::chrono::steady_clock> t_lane_scored;
        int score = 0;
    };
    std::array< gf_lane_score_t, 4 > gf_lane_scored;
};
    

