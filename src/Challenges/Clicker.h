// Clicker.h - A clicker minigame for DICOMautomaton.

#pragma once

#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>

#include "YgorMath.h"
#include "YgorMisc.h"

#include "../imgui20210904/imgui.h"

// A clicker game where players must click buttons before their timers expire.
//
// Rules:
//   - Start with a single button with a 10 second countdown timer
//   - Clicking before 3 seconds remaining does nothing
//   - Clicking between 3-0 seconds resets the timer
//   - If timer drops below zero, game over
//   - Every 3 successful clicks, add a new button (30-90 second timers)
//
// Button personalities (by button number):
//   1: Plain button near bottom of window
//   3: Slowly wanders around the window imperceptibly
//   5: Avoids mouse cursor, gets tired, animated breathing
//   7: Bends/flexes like rubber when clicked (temporary)
//   9: Requires 3 clicks
//   11: Requires 3 right-clicks
//   12+: Increasingly unhinged and personified
//
// Controls:
//   - Left click: click buttons
//   - Right click: for button 11+
//   - R key: reset/restart the game

class ClickerGame {
  public:
    ClickerGame();
    bool Display(bool &enabled);
    void Reset();

  private:
    // Button personality types
    enum class button_personality_t {
        Plain = 0,           // Button 1: stationary, normal
        Wanderer = 1,        // Button 3: slowly wanders
        Evader = 2,          // Button 5: avoids mouse, gets tired, breathes
        Rubber = 3,          // Button 7: bends/flexes when clicked
        MultiClick = 4,      // Button 9: requires 3 left clicks
        RightClick = 5,      // Button 11: requires 3 right clicks
        Unhinged = 6         // Button 12+: increasingly chaotic
    };

    // Button state
    struct button_t {
        int id = 0;                          // Button number (1-indexed)
        button_personality_t personality = button_personality_t::Plain;
        
        // Position and size
        vec2<double> pos;                    // Center position
        vec2<double> base_size;              // Base size (width, height)
        vec2<double> current_size;           // Current size (for animations)
        
        // Timer state
        double max_time = 10.0;              // Total countdown time
        double current_time = 10.0;          // Current time remaining
        double click_window = 3.0;           // Time window where clicks count
        
        // Click tracking
        int clicks_required = 1;             // Number of clicks needed
        int clicks_received = 0;             // Current click count
        bool requires_right_click = false;   // If true, needs right-click
        
        // Animation state
        double wander_angle = 0.0;           // For wanderer: direction
        double wander_speed = 0.0;           // For wanderer: speed
        vec2<double> wander_target;          // For wanderer: target position
        
        double evade_timer = 0.0;            // For evader: how long evading
        double tired_timer = 0.0;            // For evader: tiredness
        bool is_tired = false;               // For evader: currently tired
        double breath_phase = 0.0;           // For evader: breathing animation
        
        double rubber_timer = 0.0;           // For rubber: bend animation timer
        double rubber_cooldown = 0.0;        // For rubber: cooldown after bending
        vec2<double> rubber_offset;          // For rubber: current bend offset
        vec2<double> click_pos;              // For rubber: where click occurred
        
        // Unhinged state (for buttons 12+)
        double unhinged_level = 0.0;         // How unhinged (increases with button #)
        double shake_intensity = 0.0;        // Random shaking
        double color_phase = 0.0;            // Color cycling
        std::string mood_text;               // Current "mood" text
        double mood_timer = 0.0;             // Time until mood changes
        bool is_screaming = false;           // Visual effect
        double eye_blink_timer = 0.0;        // Eye blink animation
        bool eyes_open = true;
        
        // Visual style
        ImU32 color = IM_COL32(100, 100, 200, 255);
        ImU32 text_color = IM_COL32(255, 255, 255, 255);
        std::string label;
    };

    // Game state
    std::vector<button_t> buttons;
    int total_successful_clicks = 0;
    int buttons_spawned = 1;
    bool game_over = false;
    int score = 0;
    
    // Timing
    std::chrono::time_point<std::chrono::steady_clock> t_updated;
    std::mt19937 rng;
    
    // Display parameters
    static constexpr float window_width = 800.0f;
    static constexpr float window_height = 600.0f;
    static constexpr float button_width = 120.0f;
    static constexpr float button_height = 50.0f;
    static constexpr float button_margin = 10.0f;
    static constexpr float timer_bar_height = 8.0f;
    
    // Animation parameters
    static constexpr double wander_speed_min = 2.0;
    static constexpr double wander_speed_max = 5.0;
    static constexpr double evade_distance = 150.0;
    static constexpr double evade_speed = 200.0;
    static constexpr double tired_threshold = 3.0;      // Seconds of chasing before tired
    static constexpr double tired_duration = 2.0;       // Seconds of being tired
    static constexpr double breath_rate = 2.0;          // Breaths per second when tired
    static constexpr double rubber_bend_duration = 0.5; // Seconds of bending
    static constexpr double rubber_cooldown_duration = 1.0;
    
    // Colors
    static constexpr ImU32 background_color = IM_COL32(30, 35, 45, 255);
    static constexpr ImU32 button_normal_color = IM_COL32(70, 90, 120, 255);
    static constexpr ImU32 button_hover_color = IM_COL32(90, 110, 140, 255);
    static constexpr ImU32 button_clickable_color = IM_COL32(70, 150, 70, 255);
    static constexpr ImU32 button_warning_color = IM_COL32(200, 150, 50, 255);
    static constexpr ImU32 button_danger_color = IM_COL32(200, 70, 70, 255);
    static constexpr ImU32 timer_bar_bg_color = IM_COL32(50, 50, 50, 255);
    static constexpr ImU32 timer_bar_fill_color = IM_COL32(100, 180, 100, 255);
    static constexpr ImU32 timer_bar_warning_color = IM_COL32(200, 180, 50, 255);
    static constexpr ImU32 timer_bar_danger_color = IM_COL32(200, 80, 80, 255);
    static constexpr ImU32 text_color = IM_COL32(255, 255, 255, 255);
    static constexpr ImU32 game_over_color = IM_COL32(255, 100, 100, 255);
    
    // Helper functions
    void SpawnButton(int button_number);
    void UpdateButton(button_t& button, double dt, ImVec2 mouse_pos);
    void DrawButton(ImDrawList* draw_list, const button_t& button, ImVec2 window_pos, ImVec2 mouse_pos, bool mouse_over);
    bool HandleButtonClick(button_t& button, bool is_left_click, ImVec2 click_pos);
    bool CheckButtonOverlap(const vec2<double>& pos, const vec2<double>& size, int exclude_id) const;
    vec2<double> FindNonOverlappingPosition(const vec2<double>& size, int exclude_id);
    button_personality_t GetPersonalityForButton(int button_number) const;
    ImU32 GetButtonColor(const button_t& button) const;
    std::string GenerateUnhingedMood(int unhinged_level);
    void DrawBreathingEffect(ImDrawList* draw_list, const button_t& button, ImVec2 window_pos);
    void DrawRubberEffect(ImDrawList* draw_list, const button_t& button, ImVec2 window_pos);
    void DrawUnhingedEffects(ImDrawList* draw_list, const button_t& button, ImVec2 window_pos);
};
