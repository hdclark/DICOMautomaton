// Clicker.cc - A part of DICOMautomaton 2026. Written by hal clark.
// A clicker game where players must click buttons before their timers expire.

#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"

#include <SDL.h>
#include "../imgui20210904/imgui.h"

#include "Clicker.h"

namespace {
    constexpr double pi = 3.14159265358979323846;
    
    // Clamp a value between min and max
    template<typename T>
    T clamp_val(T val, T min_val, T max_val) {
        return std::max(min_val, std::min(max_val, val));
    }
}

ClickerGame::ClickerGame() {
    rng.seed(std::random_device()());
    Reset();
}

void ClickerGame::Reset() {
    buttons.clear();
    total_successful_clicks = 0;
    buttons_spawned = 1;
    game_over = false;
    score = 0;
    t_updated = std::chrono::steady_clock::now();
    
    // Spawn the first button
    SpawnButton(1);
}

ClickerGame::button_personality_t ClickerGame::GetPersonalityForButton(int button_number) const {
    switch(button_number) {
        case 1:
        case 2:
            return button_personality_t::Plain;
        case 3:
        case 4:
            return button_personality_t::Wanderer;
        case 5:
        case 6:
            return button_personality_t::Evader;
        case 7:
        case 8:
            return button_personality_t::Rubber;
        case 9:
        case 10:
            return button_personality_t::MultiClick;
        case 11:
            return button_personality_t::RightClick;
        default:
            return button_personality_t::Unhinged;
    }
}

void ClickerGame::SpawnButton(int button_number) {
    button_t btn;
    btn.id = button_number;
    btn.personality = GetPersonalityForButton(button_number);
    
    // Set timer based on button number
    if(button_number == 1) {
        btn.max_time = 10.0;
    } else {
        // Random time between 30-90 seconds for subsequent buttons
        std::uniform_real_distribution<double> time_dist(30.0, 90.0);
        btn.max_time = time_dist(rng);
    }
    btn.current_time = btn.max_time;
    btn.click_window = 3.0;
    
    // Set size
    btn.base_size = vec2<double>(button_width, button_height);
    btn.current_size = btn.base_size;
    
    // Set personality-specific properties
    switch(btn.personality) {
        case button_personality_t::Plain:
            btn.label = "Button " + std::to_string(button_number);
            btn.color = button_normal_color;
            break;
            
        case button_personality_t::Wanderer:
            btn.label = "Wanderer " + std::to_string(button_number);
            btn.color = IM_COL32(100, 120, 150, 255);
            {
                std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * pi);
                std::uniform_real_distribution<double> speed_dist(wander_speed_min, wander_speed_max);
                btn.wander_angle = angle_dist(rng);
                btn.wander_speed = speed_dist(rng);
            }
            break;
            
        case button_personality_t::Evader:
            btn.label = "Shy " + std::to_string(button_number);
            btn.color = IM_COL32(150, 100, 150, 255);
            btn.evade_timer = 0.0;
            btn.tired_timer = 0.0;
            btn.is_tired = false;
            btn.breath_phase = 0.0;
            break;
            
        case button_personality_t::Rubber:
            btn.label = "Bouncy " + std::to_string(button_number);
            btn.color = IM_COL32(200, 150, 100, 255);
            btn.rubber_timer = 0.0;
            btn.rubber_cooldown = 0.0;
            btn.rubber_offset = vec2<double>(0.0, 0.0);
            break;
            
        case button_personality_t::MultiClick:
            btn.label = "Triple " + std::to_string(button_number);
            btn.color = IM_COL32(100, 150, 200, 255);
            btn.clicks_required = 3;
            btn.clicks_received = 0;
            break;
            
        case button_personality_t::RightClick:
            btn.label = "Right-Click " + std::to_string(button_number);
            btn.color = IM_COL32(180, 100, 180, 255);
            btn.clicks_required = 3;
            btn.clicks_received = 0;
            btn.requires_right_click = true;
            break;
            
        case button_personality_t::Unhinged:
            btn.unhinged_level = static_cast<double>(button_number - 11);
            btn.label = "???";
            btn.color = IM_COL32(150, 50, 50, 255);
            btn.shake_intensity = 1.0 + btn.unhinged_level * 0.5;
            btn.mood_text = GenerateUnhingedMood(static_cast<int>(btn.unhinged_level));
            btn.mood_timer = 2.0;
            btn.eye_blink_timer = 0.0;
            btn.eyes_open = true;
            // Unhinged buttons may need multiple clicks
            if(button_number >= 13) {
                btn.clicks_required = 2 + (button_number - 12) / 2;
                btn.clicks_required = std::min(btn.clicks_required, 5);
            }
            break;
    }
    
    // Find non-overlapping position
    btn.pos = FindNonOverlappingPosition(btn.base_size, btn.id);
    btn.wander_target = btn.pos;
    
    buttons.push_back(btn);
}

vec2<double> ClickerGame::FindNonOverlappingPosition(const vec2<double>& size, int exclude_id) {
    const double margin = button_margin;
    const double play_width = window_width - size.x - margin * 2;
    const double play_height = window_height - size.y - margin * 2 - 60; // Leave room for header
    
    // For button 1, place near bottom center
    if(exclude_id == 1) {
        return vec2<double>(window_width / 2.0, window_height - size.y / 2.0 - margin - 30);
    }
    
    // Try random positions
    std::uniform_real_distribution<double> x_dist(margin + size.x / 2.0, margin + size.x / 2.0 + play_width);
    std::uniform_real_distribution<double> y_dist(60 + margin + size.y / 2.0, 60 + margin + size.y / 2.0 + play_height);
    
    for(int attempt = 0; attempt < 100; ++attempt) {
        vec2<double> candidate(x_dist(rng), y_dist(rng));
        if(!CheckButtonOverlap(candidate, size, exclude_id)) {
            return candidate;
        }
    }
    
    // Fallback: place in a grid pattern
    int grid_cols = static_cast<int>(window_width / (size.x + margin * 2));
    int grid_rows = static_cast<int>((window_height - 60) / (size.y + margin * 2));
    int idx = exclude_id - 1;
    int col = idx % std::max(1, grid_cols);
    int row = idx / std::max(1, grid_cols);
    
    return vec2<double>(
        margin + size.x / 2.0 + col * (size.x + margin * 2),
        60 + margin + size.y / 2.0 + row * (size.y + margin * 2)
    );
}

bool ClickerGame::CheckButtonOverlap(const vec2<double>& pos, const vec2<double>& size, int exclude_id) const {
    const double margin = button_margin * 2;
    
    for(const auto& btn : buttons) {
        if(btn.id == exclude_id) continue;
        
        // Check AABB overlap
        double dx = std::abs(pos.x - btn.pos.x);
        double dy = std::abs(pos.y - btn.pos.y);
        double min_dx = (size.x + btn.current_size.x) / 2.0 + margin;
        double min_dy = (size.y + btn.current_size.y) / 2.0 + margin;
        
        if(dx < min_dx && dy < min_dy) {
            return true;
        }
    }
    return false;
}

std::string ClickerGame::GenerateUnhingedMood(int unhinged_level) {
    std::vector<std::string> moods;
    
    if(unhinged_level <= 2) {
        moods = {"anxious", "nervous", "jittery", "unsettled", "uneasy"};
    } else if(unhinged_level <= 5) {
        moods = {"PANICKING", "DESPERATE", "FRANTIC", "MANIC", "UNSTABLE"};
    } else {
        moods = {"*SCREAMING*", "WHY?!", "HELP ME", "I CAN'T", "AAAAA", "NO MORE", "END IT"};
    }
    
    std::uniform_int_distribution<size_t> dist(0, moods.size() - 1);
    return moods[dist(rng)];
}

void ClickerGame::UpdateButton(button_t& button, double dt, ImVec2 mouse_pos) {
    // Update timer
    button.current_time -= dt;
    
    // Update personality-specific behavior
    switch(button.personality) {
        case button_personality_t::Plain:
            // No special behavior
            break;
            
        case button_personality_t::Wanderer:
            {
                // Slowly wander toward target
                vec2<double> to_target = button.wander_target - button.pos;
                double dist = to_target.length();
                
                if(dist < 5.0) {
                    // Pick new target
                    const double margin = button_margin + button.base_size.x / 2.0;
                    std::uniform_real_distribution<double> x_dist(margin, window_width - margin);
                    std::uniform_real_distribution<double> y_dist(60 + margin, window_height - margin);
                    button.wander_target = vec2<double>(x_dist(rng), y_dist(rng));
                } else {
                    // Move toward target very slowly
                    vec2<double> dir = to_target / dist;
                    vec2<double> new_pos = button.pos + dir * button.wander_speed * dt;
                    
                    // Check bounds
                    const double margin = button_margin + button.base_size.x / 2.0;
                    new_pos.x = clamp_val(new_pos.x, margin, window_width - margin);
                    new_pos.y = clamp_val(new_pos.y, 60.0 + margin, window_height - margin);
                    
                    // Check overlap
                    if(!CheckButtonOverlap(new_pos, button.base_size, button.id)) {
                        button.pos = new_pos;
                    }
                }
            }
            break;
            
        case button_personality_t::Evader:
            {
                vec2<double> mouse(mouse_pos.x, mouse_pos.y);
                vec2<double> to_mouse = mouse - button.pos;
                double dist_to_mouse = to_mouse.length();
                
                if(button.is_tired) {
                    // Tired: breathing animation, no movement
                    button.tired_timer += dt;
                    button.breath_phase += dt * breath_rate * 2.0 * pi;
                    
                    // Breathing affects size
                    double breath_scale = 1.0 + 0.15 * std::sin(button.breath_phase);
                    button.current_size = button.base_size * breath_scale;
                    
                    if(button.tired_timer >= tired_duration) {
                        button.is_tired = false;
                        button.tired_timer = 0.0;
                        button.evade_timer = 0.0;
                        button.current_size = button.base_size;
                    }
                } else if(dist_to_mouse < evade_distance) {
                    // Evade: move away from mouse
                    button.evade_timer += dt;
                    
                    if(button.evade_timer >= tired_threshold) {
                        button.is_tired = true;
                        button.tired_timer = 0.0;
                    } else {
                        // Calculate tired factor (slow down as we get more tired)
                        double tired_factor = 1.0 - (button.evade_timer / tired_threshold) * 0.7;
                        
                        vec2<double> away = button.pos - mouse;
                        if(away.length() > 0.001) {
                            away = away / away.length();
                        } else {
                            away = vec2<double>(1.0, 0.0);
                        }
                        
                        vec2<double> new_pos = button.pos + away * evade_speed * tired_factor * dt;
                        
                        // Check bounds
                        const double margin = button_margin + button.base_size.x / 2.0;
                        new_pos.x = clamp_val(new_pos.x, margin, window_width - margin);
                        new_pos.y = clamp_val(new_pos.y, 60.0 + margin, window_height - margin);
                        
                        // Check overlap
                        if(!CheckButtonOverlap(new_pos, button.base_size, button.id)) {
                            button.pos = new_pos;
                        }
                    }
                } else {
                    // Not being chased, slowly recover
                    button.evade_timer = std::max(0.0, button.evade_timer - dt * 0.5);
                }
            }
            break;
            
        case button_personality_t::Rubber:
            {
                if(button.rubber_timer > 0.0) {
                    button.rubber_timer -= dt;
                    
                    // Oscillating bend effect
                    double t = (rubber_bend_duration - button.rubber_timer) / rubber_bend_duration;
                    double bend = std::sin(t * pi * 4) * (1.0 - t) * 30.0;
                    
                    // Direction based on click position
                    vec2<double> dir = button.click_pos - button.pos;
                    if(dir.length() > 0.001) {
                        dir = dir / dir.length();
                    } else {
                        dir = vec2<double>(1.0, 0.0);
                    }
                    
                    button.rubber_offset = dir * bend;
                    
                    // Size oscillation
                    double size_mod = 1.0 + 0.2 * std::sin(t * pi * 6) * (1.0 - t);
                    button.current_size = button.base_size * size_mod;
                    
                    if(button.rubber_timer <= 0.0) {
                        button.rubber_offset = vec2<double>(0.0, 0.0);
                        button.current_size = button.base_size;
                        button.rubber_cooldown = rubber_cooldown_duration;
                    }
                } else if(button.rubber_cooldown > 0.0) {
                    button.rubber_cooldown -= dt;
                }
            }
            break;
            
        case button_personality_t::MultiClick:
        case button_personality_t::RightClick:
            // No special animation
            break;
            
        case button_personality_t::Unhinged:
            {
                // Color cycling
                button.color_phase += dt * (1.0 + button.unhinged_level * 0.3);
                
                // Random shaking
                std::uniform_real_distribution<double> shake_dist(-1.0, 1.0);
                double shake_x = shake_dist(rng) * button.shake_intensity;
                double shake_y = shake_dist(rng) * button.shake_intensity;
                button.rubber_offset = vec2<double>(shake_x, shake_y);
                
                // Mood changes
                button.mood_timer -= dt;
                if(button.mood_timer <= 0.0) {
                    button.mood_text = GenerateUnhingedMood(static_cast<int>(button.unhinged_level));
                    button.mood_timer = 1.0 + 1.0 / (1.0 + button.unhinged_level);
                    button.is_screaming = (button.unhinged_level >= 5) && (shake_dist(rng) > 0.0);
                }
                
                // Eye blinking
                button.eye_blink_timer -= dt;
                if(button.eye_blink_timer <= 0.0) {
                    button.eyes_open = !button.eyes_open;
                    if(button.eyes_open) {
                        std::uniform_real_distribution<double> blink_dist(1.0, 3.0);
                        button.eye_blink_timer = blink_dist(rng);
                    } else {
                        button.eye_blink_timer = 0.1;
                    }
                }
                
                // Size pulsing
                double pulse = 1.0 + 0.1 * std::sin(button.color_phase * 3.0);
                button.current_size = button.base_size * pulse;
            }
            break;
    }
}

bool ClickerGame::HandleButtonClick(button_t& button, bool is_left_click, ImVec2 click_pos) {
    // Check if in click window
    if(button.current_time > button.click_window) {
        return false; // Too early
    }
    
    // Check click type
    if(button.requires_right_click && is_left_click) {
        return false; // Wrong button
    }
    if(!button.requires_right_click && !is_left_click) {
        return false; // Wrong button
    }
    
    // Rubber button: trigger bend animation and reject click
    if(button.personality == button_personality_t::Rubber && button.rubber_cooldown <= 0.0) {
        if(button.rubber_timer <= 0.0) {
            button.rubber_timer = rubber_bend_duration;
            button.click_pos = vec2<double>(click_pos.x, click_pos.y);
            return false;
        }
    }
    
    // Evader button: if not tired, can't click
    if(button.personality == button_personality_t::Evader && !button.is_tired) {
        return false;
    }
    
    // Multi-click buttons: increment counter
    button.clicks_received++;
    if(button.clicks_received < button.clicks_required) {
        return false; // Need more clicks
    }
    
    // Reset button
    button.current_time = button.max_time;
    button.clicks_received = 0;
    return true;
}

ImU32 ClickerGame::GetButtonColor(const button_t& button) const {
    double time_ratio = button.current_time / button.max_time;
    bool in_window = button.current_time <= button.click_window;
    
    if(button.personality == button_personality_t::Unhinged) {
        // Color cycling for unhinged buttons
        float r = static_cast<float>(0.5 + 0.5 * std::sin(button.color_phase));
        float g = static_cast<float>(0.5 + 0.5 * std::sin(button.color_phase + 2.0));
        float b = static_cast<float>(0.5 + 0.5 * std::sin(button.color_phase + 4.0));
        
        if(in_window) {
            return IM_COL32(
                static_cast<int>(100 + 155 * r),
                static_cast<int>(150 + 105 * g),
                static_cast<int>(100 + 155 * b),
                255
            );
        } else {
            return IM_COL32(
                static_cast<int>(80 + 80 * r),
                static_cast<int>(50 + 80 * g),
                static_cast<int>(80 + 80 * b),
                255
            );
        }
    }
    
    if(in_window) {
        if(time_ratio < 0.1) {
            return button_danger_color;
        } else if(time_ratio < 0.2) {
            return button_warning_color;
        } else {
            return button_clickable_color;
        }
    }
    
    return button.color;
}

void ClickerGame::DrawButton(ImDrawList* draw_list, const button_t& button, ImVec2 window_pos, ImVec2 mouse_pos, bool mouse_over) {
    // Calculate position with offset
    vec2<double> draw_pos = button.pos + button.rubber_offset;
    ImVec2 center(window_pos.x + static_cast<float>(draw_pos.x),
                  window_pos.y + static_cast<float>(draw_pos.y));
    
    ImVec2 half_size(static_cast<float>(button.current_size.x / 2.0),
                     static_cast<float>(button.current_size.y / 2.0));
    
    ImVec2 min_pos(center.x - half_size.x, center.y - half_size.y);
    ImVec2 max_pos(center.x + half_size.x, center.y + half_size.y);
    
    // Get button color
    ImU32 color = GetButtonColor(button);
    if(mouse_over && button.current_time <= button.click_window) {
        color = button_hover_color;
    }
    
    // Draw button rectangle
    draw_list->AddRectFilled(min_pos, max_pos, color, 5.0f);
    draw_list->AddRect(min_pos, max_pos, IM_COL32(200, 200, 200, 255), 5.0f, 0, 2.0f);
    
    // Draw timer bar
    float timer_width = static_cast<float>(button.current_size.x - 10);
    float timer_fill = static_cast<float>(button.current_time / button.max_time);
    timer_fill = clamp_val(timer_fill, 0.0f, 1.0f);
    
    ImVec2 timer_min(center.x - timer_width / 2, max_pos.y - timer_bar_height - 5);
    ImVec2 timer_max(center.x + timer_width / 2, max_pos.y - 5);
    ImVec2 timer_fill_max(timer_min.x + timer_width * timer_fill, timer_max.y);
    
    ImU32 timer_color = timer_bar_fill_color;
    if(timer_fill < 0.3f) timer_color = timer_bar_danger_color;
    else if(timer_fill < 0.5f) timer_color = timer_bar_warning_color;
    
    draw_list->AddRectFilled(timer_min, timer_max, timer_bar_bg_color, 2.0f);
    if(timer_fill > 0.0f) {
        draw_list->AddRectFilled(timer_min, timer_fill_max, timer_color, 2.0f);
    }
    
    // Draw label
    std::string display_label = button.label;
    if(button.personality == button_personality_t::MultiClick || 
       button.personality == button_personality_t::RightClick ||
       (button.personality == button_personality_t::Unhinged && button.clicks_required > 1)) {
        display_label += " (" + std::to_string(button.clicks_received) + "/" + 
                         std::to_string(button.clicks_required) + ")";
    }
    
    ImVec2 text_size = ImGui::CalcTextSize(display_label.c_str());
    ImVec2 text_pos(center.x - text_size.x / 2, center.y - text_size.y / 2 - 5);
    draw_list->AddText(text_pos, text_color, display_label.c_str());
    
    // Draw timer text
    std::ostringstream timer_ss;
    timer_ss << std::fixed << std::setprecision(1) << std::max(0.0, button.current_time) << "s";
    std::string timer_str = timer_ss.str();
    ImVec2 timer_text_size = ImGui::CalcTextSize(timer_str.c_str());
    ImVec2 timer_text_pos(center.x - timer_text_size.x / 2, min_pos.y + 3);
    
    ImU32 timer_text_color = text_color;
    if(button.current_time <= button.click_window) {
        timer_text_color = button.current_time < 1.0 ? IM_COL32(255, 100, 100, 255) : IM_COL32(255, 255, 100, 255);
    }
    draw_list->AddText(timer_text_pos, timer_text_color, timer_str.c_str());
    
    // Draw personality-specific effects
    if(button.personality == button_personality_t::Evader && button.is_tired) {
        DrawBreathingEffect(draw_list, button, window_pos);
    }
    
    if(button.personality == button_personality_t::Unhinged) {
        DrawUnhingedEffects(draw_list, button, window_pos);
    }
}

void ClickerGame::DrawBreathingEffect(ImDrawList* draw_list, const button_t& button, ImVec2 window_pos) {
    // Draw "tired" indicator and breathing puffs
    vec2<double> draw_pos = button.pos + button.rubber_offset;
    ImVec2 center(window_pos.x + static_cast<float>(draw_pos.x),
                  window_pos.y + static_cast<float>(draw_pos.y));
    
    // Draw "ZZZ" or puff clouds
    double breath = std::sin(button.breath_phase);
    float offset_y = static_cast<float>(button.current_size.y / 2.0 + 10 + breath * 5);
    
    ImVec2 puff_pos(center.x + static_cast<float>(button.current_size.x / 2.0) + 15,
                    center.y - offset_y);
    
    // Draw small circles as "breath puffs"
    float puff_alpha = static_cast<float>(0.3 + 0.4 * (breath + 1.0) / 2.0);
    float puff_size = 5.0f + 5.0f * static_cast<float>((breath + 1.0) / 2.0);
    
    draw_list->AddCircleFilled(puff_pos, puff_size, 
        IM_COL32(200, 200, 220, static_cast<int>(puff_alpha * 255)));
    
    puff_pos.x += puff_size * 1.5f;
    puff_pos.y -= puff_size * 0.5f;
    draw_list->AddCircleFilled(puff_pos, puff_size * 0.7f,
        IM_COL32(200, 200, 220, static_cast<int>(puff_alpha * 200)));
    
    // Draw "tired" text
    ImVec2 tired_pos(center.x - 20, center.y - static_cast<float>(button.current_size.y / 2.0) - 20);
    draw_list->AddText(tired_pos, IM_COL32(255, 200, 100, 200), "*huff*");
}

void ClickerGame::DrawUnhingedEffects(ImDrawList* draw_list, const button_t& button, ImVec2 window_pos) {
    vec2<double> draw_pos = button.pos + button.rubber_offset;
    ImVec2 center(window_pos.x + static_cast<float>(draw_pos.x),
                  window_pos.y + static_cast<float>(draw_pos.y));
    
    // Draw eyes
    float eye_y = center.y - 5;
    float eye_spacing = 15.0f;
    float eye_size = 4.0f;
    
    ImVec2 left_eye(center.x - eye_spacing, eye_y);
    ImVec2 right_eye(center.x + eye_spacing, eye_y);
    
    if(button.eyes_open) {
        // Draw open eyes
        draw_list->AddCircleFilled(left_eye, eye_size, IM_COL32(255, 255, 255, 255));
        draw_list->AddCircleFilled(right_eye, eye_size, IM_COL32(255, 255, 255, 255));
        
        // Pupils (looking at... something)
        float pupil_offset = button.is_screaming ? 0.0f : 1.5f;
        draw_list->AddCircleFilled(ImVec2(left_eye.x + pupil_offset, left_eye.y), 2.0f, IM_COL32(0, 0, 0, 255));
        draw_list->AddCircleFilled(ImVec2(right_eye.x + pupil_offset, right_eye.y), 2.0f, IM_COL32(0, 0, 0, 255));
    } else {
        // Draw closed eyes (lines)
        draw_list->AddLine(
            ImVec2(left_eye.x - eye_size, left_eye.y),
            ImVec2(left_eye.x + eye_size, left_eye.y),
            IM_COL32(0, 0, 0, 255), 2.0f);
        draw_list->AddLine(
            ImVec2(right_eye.x - eye_size, right_eye.y),
            ImVec2(right_eye.x + eye_size, right_eye.y),
            IM_COL32(0, 0, 0, 255), 2.0f);
    }
    
    // Draw mood text above button
    ImVec2 mood_pos(center.x - ImGui::CalcTextSize(button.mood_text.c_str()).x / 2,
                    center.y - static_cast<float>(button.current_size.y / 2.0) - 25);
    
    ImU32 mood_color = button.is_screaming ? IM_COL32(255, 50, 50, 255) : IM_COL32(255, 200, 100, 255);
    draw_list->AddText(mood_pos, mood_color, button.mood_text.c_str());
    
    // Draw mouth
    float mouth_y = center.y + 10;
    if(button.is_screaming) {
        // Screaming mouth (open circle)
        draw_list->AddCircleFilled(ImVec2(center.x, mouth_y), 8.0f, IM_COL32(50, 50, 50, 255));
    } else {
        // Worried mouth (wavy line)
        float wave = static_cast<float>(std::sin(button.color_phase * 5.0)) * 3.0f;
        draw_list->AddBezierQuadratic(
            ImVec2(center.x - 10, mouth_y),
            ImVec2(center.x, mouth_y + wave),
            ImVec2(center.x + 10, mouth_y),
            IM_COL32(50, 50, 50, 255), 2.0f);
    }
}

bool ClickerGame::Display(bool &enabled) {
    if(!enabled) return true;
    
    // Calculate delta time
    auto t_now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(t_now - t_updated).count();
    t_updated = t_now;
    
    // Clamp dt to avoid huge jumps
    dt = std::min(dt, 0.1);
    
    // Set fixed window size
    ImGui::SetNextWindowSize(ImVec2(window_width, window_height), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | 
                                    ImGuiWindowFlags_NoScrollbar |
                                    ImGuiWindowFlags_NoScrollWithMouse;
    
    if(ImGui::Begin("Clicker Game", &enabled, window_flags)) {
        ImVec2 window_pos = ImGui::GetWindowPos();
        ImVec2 content_pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        
        // Draw background
        ImVec2 bg_min = content_pos;
        ImVec2 bg_max(content_pos.x + window_width - 20, content_pos.y + window_height - 60);
        draw_list->AddRectFilled(bg_min, bg_max, background_color, 5.0f);
        
        // Offset for content area
        ImVec2 play_area_offset(content_pos.x - window_pos.x, content_pos.y - window_pos.y);
        
        // Get mouse position relative to window
        ImVec2 mouse_pos = ImGui::GetMousePos();
        ImVec2 local_mouse(mouse_pos.x - window_pos.x, mouse_pos.y - window_pos.y);
        
        // Handle keyboard input
        if(ImGui::IsWindowFocused()) {
            if(ImGui::IsKeyPressed(ImGuiKey_R)) {
                Reset();
            }
        }
        
        if(!game_over) {
            // Update all buttons
            for(auto& button : buttons) {
                UpdateButton(button, dt, local_mouse);
                
                // Check for game over
                if(button.current_time < 0.0) {
                    game_over = true;
                    break;
                }
            }
            
            // Handle mouse clicks
            bool left_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
            bool right_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
            
            if(left_clicked || right_clicked) {
                for(auto& button : buttons) {
                    // Check if click is on button
                    vec2<double> draw_pos = button.pos + button.rubber_offset;
                    double half_w = button.current_size.x / 2.0;
                    double half_h = button.current_size.y / 2.0;
                    
                    if(local_mouse.x >= draw_pos.x - half_w && local_mouse.x <= draw_pos.x + half_w &&
                       local_mouse.y >= draw_pos.y - half_h && local_mouse.y <= draw_pos.y + half_h) {
                        
                        if(HandleButtonClick(button, left_clicked, local_mouse)) {
                            total_successful_clicks++;
                            score += 10 * button.id;
                            
                            // Spawn new button every 3 successful clicks
                            if(total_successful_clicks % 3 == 0) {
                                buttons_spawned++;
                                SpawnButton(buttons_spawned);
                            }
                        }
                        break;
                    }
                }
            }
            
            // Draw all buttons
            for(const auto& button : buttons) {
                // Check if mouse is over button
                vec2<double> draw_pos = button.pos + button.rubber_offset;
                double half_w = button.current_size.x / 2.0;
                double half_h = button.current_size.y / 2.0;
                
                bool mouse_over = (local_mouse.x >= draw_pos.x - half_w && 
                                   local_mouse.x <= draw_pos.x + half_w &&
                                   local_mouse.y >= draw_pos.y - half_h && 
                                   local_mouse.y <= draw_pos.y + half_h);
                
                DrawButton(draw_list, button, window_pos, local_mouse, mouse_over);
            }
        }
        
        // Draw header
        ImGui::SetCursorPos(ImVec2(10, 25));
        ImGui::Text("Score: %d", score);
        ImGui::SameLine(150);
        ImGui::Text("Buttons: %d", static_cast<int>(buttons.size()));
        ImGui::SameLine(300);
        ImGui::Text("Clicks: %d", total_successful_clicks);
        ImGui::SameLine(window_width - 200);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Press R to restart");
        
        // Draw game over screen
        if(game_over) {
            ImVec2 center(content_pos.x + (window_width - 20) / 2,
                          content_pos.y + (window_height - 60) / 2);
            
            // Darken background
            draw_list->AddRectFilled(bg_min, bg_max, IM_COL32(0, 0, 0, 180), 5.0f);
            
            // Game over text
            const char* game_over_text = "GAME OVER";
            ImVec2 text_size = ImGui::CalcTextSize(game_over_text);
            ImVec2 text_pos(center.x - text_size.x / 2, center.y - text_size.y / 2 - 30);
            
            // Draw with shadow
            draw_list->AddText(ImVec2(text_pos.x + 2, text_pos.y + 2), IM_COL32(0, 0, 0, 255), game_over_text);
            draw_list->AddText(text_pos, game_over_color, game_over_text);
            
            // Final score
            std::string score_text = "Final Score: " + std::to_string(score);
            ImVec2 score_size = ImGui::CalcTextSize(score_text.c_str());
            ImVec2 score_pos(center.x - score_size.x / 2, center.y + 10);
            draw_list->AddText(score_pos, text_color, score_text.c_str());
            
            // Restart hint
            const char* restart_text = "Press R to play again";
            ImVec2 restart_size = ImGui::CalcTextSize(restart_text);
            ImVec2 restart_pos(center.x - restart_size.x / 2, center.y + 50);
            draw_list->AddText(restart_pos, IM_COL32(200, 200, 200, 255), restart_text);
        }
    }
    ImGui::End();
    
    return true;
}
