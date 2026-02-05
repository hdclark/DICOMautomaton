// GuitarFret.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>

#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"

#include <SDL.h>
#include "../imgui20210904/imgui.h"

#include "GuitarFret.h"

GuitarFretGame::GuitarFretGame(){
    gf_game.re.seed( std::random_device()() );
    Reset();
}

void GuitarFretGame::Reset(){
    gf_game.notes.clear();
    gf_game.score = 0;
    gf_game.streak = 0;
    gf_game.paused = false;
    gf_game.next_note_time = 0.5;
    gf_game.elapsed_time = 0.0;
    gf_lane_pressed = {false, false, false, false};

    // Adjust difficulty settings
    switch(gf_game.difficulty){
        case 0: // Easy
            gf_game.note_speed = 0.25;
            gf_game.hit_window_perfect = 0.08;
            gf_game.hit_window_ok = 0.15;
            break;
        case 1: // Medium
            gf_game.note_speed = 0.40;
            gf_game.hit_window_perfect = 0.05;
            gf_game.hit_window_ok = 0.10;
            break;
        case 2: // Hard
            gf_game.note_speed = 0.60;
            gf_game.hit_window_perfect = 0.03;
            gf_game.hit_window_ok = 0.07;
            break;
        case 3: // Expert
            gf_game.note_speed = 0.80;
            gf_game.hit_window_perfect = 0.02;
            gf_game.hit_window_ok = 0.05;
            break;
        case 4: // Ultimate
            gf_game.note_speed = 1.20;
            gf_game.hit_window_perfect = 0.02;
            gf_game.hit_window_ok = 0.04;
            break;
        default:
            break;
    }

    const auto t_now = std::chrono::steady_clock::now();
    t_gf_updated = t_now;

    for(auto &l : gf_lane_scored){
        l.active = false;
        l.t_lane_scored = t_now;
        l.score = 0;
    }
    return;
}


bool GuitarFretGame::Display(bool &enabled){
    if( !enabled ) return true;

    const auto t_now = std::chrono::steady_clock::now();
    auto t_diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_gf_updated).count();
    if(t_diff_ms > 50) t_diff_ms = 50; // Cap delta time


    // Lane colors: F1=Red, F2=Yellow, F3=Blue, F4=Green
    const std::array<ImColor, 4> lane_colors = {
        ImColor(1.0f, 0.2f, 0.2f, 1.0f),  // Red for F1
        ImColor(1.0f, 1.0f, 0.2f, 1.0f),  // Yellow for F2
        ImColor(0.2f, 0.5f, 1.0f, 1.0f),  // Blue for F3
        ImColor(0.2f, 1.0f, 0.2f, 1.0f)   // Green for F4
    };
    const std::array<ImColor, 4> lane_colors_dim = {
        ImColor(0.5f, 0.1f, 0.1f, 1.0f),
        ImColor(0.5f, 0.5f, 0.1f, 1.0f),
        ImColor(0.1f, 0.25f, 0.5f, 1.0f),
        ImColor(0.1f, 0.5f, 0.1f, 1.0f)
    };

    const auto win_width  = static_cast<int>( std::ceil(gf_game.box_width) ) + 15;
    const auto win_height = static_cast<int>( std::ceil(gf_game.box_height) ) + 120;
    auto flags = ImGuiWindowFlags_AlwaysAutoResize
               | ImGuiWindowFlags_NoScrollWithMouse
               | ImGuiWindowFlags_NoNavInputs
               | ImGuiWindowFlags_NoScrollbar;
    ImGui::SetNextWindowSize(ImVec2(win_width, win_height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::Begin("Guitar Fret", &enabled, flags);

    const auto gh_game_window_focused = ImGui::IsWindowFocused();

    // Handle keyboard input only when window is focused
    if(gh_game_window_focused){
        // Reset game
        if( ImGui::IsKeyPressed(SDL_SCANCODE_R) ){
            Reset();
        }

        // Pause/unpause with spacebar
        if( ImGui::IsKeyPressed(SDL_SCANCODE_SPACE) ){
            gf_game.paused = !gf_game.paused;
        }

        // Track F1-F4 key presses for this frame
        gf_lane_pressed[0] = ImGui::IsKeyPressed(SDL_SCANCODE_F1);
        gf_lane_pressed[1] = ImGui::IsKeyPressed(SDL_SCANCODE_F2);
        gf_lane_pressed[2] = ImGui::IsKeyPressed(SDL_SCANCODE_F3);
        gf_lane_pressed[3] = ImGui::IsKeyPressed(SDL_SCANCODE_F4);
    }else{
        gf_lane_pressed = {false, false, false, false};
    }

    // Display score and stats
    ImGui::Text("Score: %" PRId64, gf_game.score);
    ImGui::SameLine();
    ImGui::Text("  Streak: %" PRId64, gf_game.streak);
    ImGui::SameLine();
    ImGui::Text("  Best: %" PRId64, gf_game.best_streak);
    ImGui::Text("High: %" PRId64 "  Low: %" PRId64, gf_game.high_score, gf_game.low_score);

    // Difficulty selector
    ImGui::SameLine();
    ImGui::Text("   ");
    ImGui::SameLine();
    const char* diff_names[] = { "Easy", "Medium", "Hard", "Expert", "Ultimate" };
    int diff = static_cast<int>(gf_game.difficulty);
    ImGui::SetNextItemWidth(80);
    if(ImGui::Combo("##Difficulty", &diff, diff_names, IM_ARRAYSIZE(diff_names))){
        gf_game.difficulty = static_cast<int64_t>(diff);
        Reset();
    }

    if(gf_game.paused){
        ImGui::SameLine();
        ImGui::Text("  PAUSED");
    }

    ImGui::Text("Controls: F1-F4 = Notes, Space = Pause, R = Reset");

    // Get draw list and current position
    ImVec2 curr_pos = ImGui::GetCursorScreenPos();
    ImDrawList *window_draw_list = ImGui::GetWindowDrawList();

    const float lane_width = gf_game.box_width / 4.0f;
    const float hit_zone_y = static_cast<float>(gf_game.box_height * gf_game.hit_zone_norm);
    const float note_radius = lane_width * 0.35f;

    // Draw background
    {
        const auto c = ImColor(0.1f, 0.1f, 0.15f, 1.0f);
        window_draw_list->AddRectFilled(curr_pos, 
            ImVec2(curr_pos.x + gf_game.box_width, curr_pos.y + gf_game.box_height), c);
    }

    // Draw lane dividers
    for(int i = 1; i < 4; ++i){
        float x = curr_pos.x + i * lane_width;
        window_draw_list->AddLine(
            ImVec2(x, curr_pos.y),
            ImVec2(x, curr_pos.y + gf_game.box_height),
            ImColor(0.3f, 0.3f, 0.35f, 1.0f), 1.0f);
    }

    // Draw hit zone with colored buttons
    for(int i = 0; i < 4; ++i){
        float lane_center = curr_pos.x + (i + 0.5f) * lane_width;
        float hit_y = curr_pos.y + hit_zone_y;

        // Draw hit zone marker (circle outline)
        bool key_down = false;
        if(gh_game_window_focused){
            switch(i){
                case 0: key_down = ImGui::IsKeyDown(SDL_SCANCODE_F1); break;
                case 1: key_down = ImGui::IsKeyDown(SDL_SCANCODE_F2); break;
                case 2: key_down = ImGui::IsKeyDown(SDL_SCANCODE_F3); break;
                case 3: key_down = ImGui::IsKeyDown(SDL_SCANCODE_F4); break;
            }
        }
        if(key_down){
            window_draw_list->AddCircleFilled(ImVec2(lane_center, hit_y), note_radius, lane_colors[i]);
        }else{
            window_draw_list->AddCircleFilled(ImVec2(lane_center, hit_y), note_radius, lane_colors_dim[i]);
        }

        const auto c_normal  = ImColor(0.8f, 0.8f, 0.8f, 0.8f);
        const auto c_hit     = ImColor(1.0f, 0.7f, 0.3f, 1.0f);
        const auto c_hit2    = ImColor(0.3f, 1.0f, 0.3f, 1.0f);
        const auto c_miss    = ImColor(1.0f, 0.3f, 0.3f, 1.0f);
        ImColor c = c_normal;
        float r = note_radius + 2.0f;
        float thick = 2.0f;

        if(gf_lane_scored.at(i).active){
            auto &l = gf_lane_scored.at(i);
            const auto dt_score = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - l.t_lane_scored).count();
            if(dt_score > gf_game.score_linger_time){
                l.active = false;
            }else if(l.score < 0){
                c = c_miss;
                thick += 3.0;
            }else if(l.score == 1){
                c = c_hit;
                thick += 3.0;
            }else if(l.score == 2){
                c = c_hit2;
                thick += 5.0;
            }
        }

        // Draw hit zone marker (circle outline)
        window_draw_list->AddCircle(ImVec2(lane_center, hit_y), r, c, 0, thick);

        // Draw lane label
        const char* labels[] = {"F1", "F2", "F3", "F4"};
        ImVec2 text_size = ImGui::CalcTextSize(labels[i]);
        window_draw_list->AddText(
            ImVec2(lane_center - text_size.x * 0.5f, curr_pos.y + gf_game.box_height + 5),
            ImColor(0.9f, 0.9f, 0.9f, 1.0f), labels[i]);
    }


    // Update game logic
    if(!gf_game.paused){
        double dt = t_diff_ms / 1000.0;
        gf_game.elapsed_time += dt;

        // Spawn new notes
        gf_game.next_note_time -= dt;
        if(gf_game.next_note_time <= 0.0){
            std::uniform_int_distribution<int64_t> lane_dist(0, 3);
            std::uniform_real_distribution<double> time_dist(0.5, 1.5);
            
            // Adjust spawn rate based on difficulty
            double spawn_mult = 1.0;
            switch(gf_game.difficulty){
                case 0: spawn_mult = 1.5; break;
                case 1: spawn_mult = 1.0; break;
                case 2: spawn_mult = 0.7; break;
                case 3: spawn_mult = 0.4; break;
                case 4: spawn_mult = 0.2; break;
            }

            gf_note_t new_note;
            new_note.lane = lane_dist(gf_game.re);
            new_note.y_pos = 0.0;
            new_note.active = true;
            new_note.hit = false;
            gf_game.notes.push_back(new_note);

            gf_game.next_note_time = time_dist(gf_game.re) * spawn_mult;
        }

        // Update note positions and identify the lowest active note for each lane.
        std::array< gf_note_t*, 4> lowest_notes;
        lowest_notes.fill(nullptr);

        for(auto& note : gf_game.notes){
            if(!note.active) continue;

            note.y_pos += gf_game.note_speed * dt;

            if( (lowest_notes.at(note.lane) == nullptr)
            ||  ( lowest_notes.at(note.lane)->y_pos < note.y_pos ) ){
                lowest_notes.at(note.lane) = &note;
            }
        }

        // Check the lowest notes for hits/misses
        for(auto& note_ptr : lowest_notes){
            if(note_ptr == nullptr) continue;

            // Check if key was pressed for this lane
            if(gf_lane_pressed[note_ptr->lane] && !note_ptr->hit){
                double distance_from_hit = std::abs(note_ptr->y_pos - gf_game.hit_zone_norm);
                
                //YLOGINFO("distance_from_hit = " << distance_from_hit << " perfect: " << gf_game.hit_window_perfect << " ok: " << gf_game.hit_window_ok);
                if(distance_from_hit > gf_game.hit_window_distant){
                    // Disable the score visual display, but do not alter any notes (too far away).
                    gf_lane_scored.at(note_ptr->lane).active = false;

                }else if(distance_from_hit <= gf_game.hit_window_perfect){
                    // Perfect hit
                    gf_game.score += 2;
                    gf_game.streak++;
                    note_ptr->hit = true;
                    note_ptr->active = false;

                    gf_lane_scored.at(note_ptr->lane).active = true;
                    gf_lane_scored.at(note_ptr->lane).score = 2;
                    gf_lane_scored.at(note_ptr->lane).t_lane_scored = t_now;

                }else if(distance_from_hit <= gf_game.hit_window_ok){
                    // OK hit
                    gf_game.score += 1;
                    gf_game.streak++;
                    note_ptr->hit = true;
                    note_ptr->active = false;

                    gf_lane_scored.at(note_ptr->lane).active = true;
                    gf_lane_scored.at(note_ptr->lane).score = 1;
                    gf_lane_scored.at(note_ptr->lane).t_lane_scored = t_now;
                }else if(note_ptr->y_pos < gf_game.hit_zone_norm - gf_game.hit_window_ok){
                    // Too early - penalty
                    gf_game.score -= 1;
                    gf_game.streak = 0;
                    note_ptr->hit = true;
                    note_ptr->active = false;

                    gf_lane_scored.at(note_ptr->lane).active = true;
                    gf_lane_scored.at(note_ptr->lane).score = -1;
                    gf_lane_scored.at(note_ptr->lane).t_lane_scored = t_now;
                }
            }

            // Note passed the hit zone without being hit
            if(note_ptr->y_pos > gf_game.hit_zone_norm + gf_game.hit_window_ok && !note_ptr->hit){
                // Too late - penalty
                gf_game.score -= 1;
                gf_game.streak = 0;
                note_ptr->active = false;

                gf_lane_scored.at(note_ptr->lane).active = true;
                gf_lane_scored.at(note_ptr->lane).score = -1;
                gf_lane_scored.at(note_ptr->lane).t_lane_scored = t_now;
            }
        }
        lowest_notes.fill(nullptr); // Don't leave dangling references.


        // Update best streak
        if(gf_game.streak > gf_game.best_streak){
            gf_game.best_streak = gf_game.streak;
        }

        // Update high/low scores
        if(gf_game.score > gf_game.high_score){
            gf_game.high_score = gf_game.score;
        }
        if(!gf_game.low_score_initialized || gf_game.score < gf_game.low_score){
            gf_game.low_score = gf_game.score;
            gf_game.low_score_initialized = true;
        }

        // Remove inactive notes
        gf_game.notes.erase(
            std::remove_if(gf_game.notes.begin(), gf_game.notes.end(),
                [](const gf_note_t& n){ return !n.active; }),
            gf_game.notes.end());
    }

    t_gf_updated = t_now;

    // Draw falling notes
    for(const auto& note : gf_game.notes){
        if(!note.active) continue;

        float lane_center = curr_pos.x + (note.lane + 0.5f) * lane_width;
        float note_y = curr_pos.y + note.y_pos * gf_game.box_height;

        // Only draw if in visible area
        if(note_y >= curr_pos.y - note_radius && note_y <= curr_pos.y + gf_game.box_height + note_radius){
            window_draw_list->AddCircleFilled(
                ImVec2(lane_center, note_y), note_radius, lane_colors[note.lane]);
            window_draw_list->AddCircle(
                ImVec2(lane_center, note_y), note_radius, 
                ImColor(1.0f, 1.0f, 1.0f, 0.6f), 0, 2.0f);
        }
    }

    // Draw border
    {
        const auto c = ImColor(0.5f, 0.5f, 0.6f, 1.0f);
        window_draw_list->AddRect(curr_pos, 
            ImVec2(curr_pos.x + gf_game.box_width, curr_pos.y + gf_game.box_height), c, 0.0f, 0, 2.0f);
    }

    ImGui::Dummy(ImVec2(gf_game.box_width, gf_game.box_height + 25));
    ImGui::End();

    return true;
}

