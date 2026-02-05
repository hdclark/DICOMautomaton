// TowerDefence.cc - A part of DICOMautomaton 2026. Written by hal clark.

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

#include "TowerDefence.h"

TowerDefenceGame::TowerDefenceGame(){

    //fs_game.re.seed( std::random_device()() );
    Reset();
}

void TowerDefenceGame::Reset(){
    td_enemies.clear();
    td_towers.clear();
    td_projectiles.clear();

    td_game.credits = 5;
    td_game.wave_number = 0;
    td_game.enemies_in_wave = 0;
    td_game.lives = 10;
    td_game.wave_active = false;
    td_game.spawn_timer = 0.0;
    td_game.show_upgrade_dialog = false;
    td_game.upgrade_tower_idx = 0;

    // Define path waypoints: starts at top-left, goes right, then down in a snake pattern.
    td_game.path_waypoints.clear();
    td_game.path_cells_set.clear();

    // Create a winding path for enemies to follow. Any square not on the path can be built upon.
    // Path goes: right across row 0, down to row 1, left across row 1, down to row 2, etc.
    for(int64_t row = 0; row < td_game.grid_rows; ++row){
        //const bool even_row = (row % 2 == 0);
        const bool odd_row = (row % 2 == 1);
        if(odd_row){
            // Connect the path to the row above.
            const auto &lw = td_game.path_waypoints.back();
            const auto prev_col = lw.first;
            td_game.path_waypoints.emplace_back(prev_col, row);
            td_game.path_cells_set.emplace(prev_col, row);

        }else{
            const bool going_right = ((row/2) % 2 == 0);
            if(going_right){
                for(int64_t col = 2; col < (td_game.grid_cols - 2); ++col){
                    td_game.path_waypoints.emplace_back(col, row);
                    td_game.path_cells_set.emplace(col, row);
                }
            }else{
                for(int64_t col = td_game.grid_cols - 3; col >= 2; --col){
                    td_game.path_waypoints.emplace_back(col, row);
                    td_game.path_cells_set.emplace(col, row);
                }
            }
        }
    }

    const auto t_now = std::chrono::steady_clock::now();
    t_td_updated = t_now;
    return;
}



bool TowerDefenceGame::Display(bool &enabled){
    if( !enabled ) return true;

    const auto box_width  = td_game.grid_cols * td_game.cell_size;
    const auto box_height = td_game.grid_rows * td_game.cell_size;
    const auto win_width  = static_cast<int>(box_width) + 220;
    const auto win_height = static_cast<int>(box_height) + 80;

    const auto wn = static_cast<double>(td_game.wave_number);

    auto flags = ImGuiWindowFlags_AlwaysAutoResize
               | ImGuiWindowFlags_NoScrollWithMouse
               | ImGuiWindowFlags_NoNavInputs
               | ImGuiWindowFlags_NoScrollbar ;
    ImGui::SetNextWindowSize(ImVec2(win_width, win_height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::Begin("Tower Defence", &enabled, flags );

    const auto f = ImGui::IsWindowFocused();

    // Reset the game before any game state is used.
    if( f && ImGui::IsKeyPressed(SDL_SCANCODE_R) ){
        Reset();
    }

    // Helper to check if a grid cell is on the path (O(1) lookup using set).
    const auto is_path_cell = [&](int64_t col, int64_t row) -> bool {
        return td_game.path_cells_set.count(std::make_pair(col, row)) > 0;
    };

    // Helper to check if a tower exists at grid cell.
    const auto has_tower_at = [&](int64_t col, int64_t row) -> bool {
        for(const auto& tower : td_towers){
            if(tower.grid_x == col && tower.grid_y == row) return true;
        }
        return false;
    };

    // Helper to get the index of a tower at a grid cell (returns td_towers.size() if none).
    const auto get_tower_idx_at = [&](int64_t col, int64_t row) -> size_t {
        for(size_t i = 0; i < td_towers.size(); ++i){
            if(td_towers[i].grid_x == col && td_towers[i].grid_y == row) return i;
        }
        return td_towers.size();
    };

    // Helper to compute the upgrade cost for a given level.
    // Level 1->2 costs 5, Level 2->3 costs 8, Level 3->4 costs 12, etc.
    const auto get_upgrade_cost = [](int64_t current_level) -> int64_t {
        return 5 + (current_level - 1) * 3;
    };

    // Helper to compute the type upgrade cost (switching to RapidFire or Boom).
    const auto get_type_upgrade_cost = [](td_tower_type_t new_type) -> int64_t {
        switch(new_type){
            case td_tower_type_t::RapidFire: return 8;
            case td_tower_type_t::Boom: return 12;
            default: return 0;
        }
    };

    // Helper to get a tower type name string.
    const auto get_tower_type_name = [](td_tower_type_t t) -> const char* {
        switch(t){
            case td_tower_type_t::Basic: return "Basic";
            case td_tower_type_t::RapidFire: return "Rapid Fire";
            case td_tower_type_t::Boom: return "Boom";
            default: return "Unknown";
        }
    };

    // Helper to compute boom radius for projectiles.
    const auto compute_boom_radius = [](double cell_size, int64_t tower_level) -> double {
        return cell_size * (0.8 + 0.2 * tower_level);
    };

    // Helper to get world position from path progress.
    const auto get_path_position = [&](double progress) -> vec2<double> {
        if(td_game.path_waypoints.empty()) return vec2<double>(0.0, 0.0);
        const auto total_segments = static_cast<double>(td_game.path_waypoints.size() - 1);
        if(total_segments <= 0.0){
            const auto& wp = td_game.path_waypoints[0];
            return vec2<double>(
                (static_cast<double>(wp.first) + 0.5) * td_game.cell_size,
                (static_cast<double>(wp.second) + 0.5) * td_game.cell_size
            );
        }
        const auto segment_progress = progress * total_segments;
        const auto segment_idx = static_cast<size_t>(std::floor(segment_progress));
        const auto t = segment_progress - static_cast<double>(segment_idx);
        
        size_t idx1 = std::min(segment_idx, td_game.path_waypoints.size() - 1);
        size_t idx2 = std::min(segment_idx + 1, td_game.path_waypoints.size() - 1);
        
        const auto& wp1 = td_game.path_waypoints[idx1];
        const auto& wp2 = td_game.path_waypoints[idx2];
        
        const double x1 = (static_cast<double>(wp1.first) + 0.5) * td_game.cell_size;
        const double y1 = (static_cast<double>(wp1.second) + 0.5) * td_game.cell_size;
        const double x2 = (static_cast<double>(wp2.first) + 0.5) * td_game.cell_size;
        const double y2 = (static_cast<double>(wp2.second) + 0.5) * td_game.cell_size;
        
        return vec2<double>(x1 + t * (x2 - x1), y1 + t * (y2 - y1));
    };

    // Time update.
    const auto t_now = std::chrono::steady_clock::now();
    auto t_updated_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_td_updated).count();
    if(50 < t_updated_diff) t_updated_diff = 50;  // Cap time step.
    const double dt = static_cast<double>(t_updated_diff) / 1000.0;
    t_td_updated = t_now;

    // Display game info panel.
    ImGui::BeginChild("GameInfo", ImVec2(200, box_height), true);
    ImGui::Text("Wave: %ld", static_cast<long>(td_game.wave_number));
    ImGui::Text("Lives: %ld", static_cast<long>(td_game.lives));
    ImGui::Text("Credits: %ld", static_cast<long>(td_game.credits));
    ImGui::Text("Towers: %zu", td_towers.size());
    ImGui::Text("Enemies: %zu", td_enemies.size());
    ImGui::Separator();

    if(td_game.lives <= 0){
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "GAME OVER!");
        if(ImGui::Button("Restart", ImVec2(-1, 0))){
            Reset();
        }
    }else if(!td_game.wave_active){
        if(ImGui::Button("Launch Wave", ImVec2(-1, 0))){
            td_game.wave_number += 1;
            td_game.enemies_in_wave = td_game.wave_number;  // Wave N has N enemies.
            td_game.wave_active = true;
            td_game.spawn_timer = 0.0;
        }
    }else{
        ImGui::Text("Wave in progress...");
        ImGui::Text("Spawning: %ld left", static_cast<long>(td_game.enemies_in_wave));
    }

    ImGui::Separator();
    ImGui::TextWrapped("Click gray tiles to place towers (%ld credits).", static_cast<long>(td_game.credits));
    ImGui::TextWrapped("Click existing towers to upgrade.");
    ImGui::TextWrapped("Press R to reset.");

    ImGui::EndChild();
    ImGui::SameLine();

    // Game grid display.
    ImVec2 grid_origin = ImGui::GetCursorScreenPos();
    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    // Draw grid cells.
    for(int64_t row = 0; row < td_game.grid_rows; ++row){
        for(int64_t col = 0; col < td_game.grid_cols; ++col){
            const float x0 = grid_origin.x + static_cast<float>(col * td_game.cell_size);
            const float y0 = grid_origin.y + static_cast<float>(row * td_game.cell_size);
            const float x1 = x0 + static_cast<float>(td_game.cell_size);
            const float y1 = y0 + static_cast<float>(td_game.cell_size);

            ImU32 fill_color;
            if(is_path_cell(col, row)){
                fill_color = ImColor(1.0f, 1.0f, 1.0f, 1.0f);  // White for path.
            }else{
                fill_color = ImColor(0.6f, 0.6f, 0.6f, 1.0f);  // Gray for buildable.
            }
            draw_list->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), fill_color);
            draw_list->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), ImColor(0.3f, 0.3f, 0.3f, 1.0f));
        }
    }

    // Handle tower placement and upgrade on mouse click.
    if( f
    &&  ImGui::IsMouseClicked(0)
    &&  (td_game.lives > 0)
    &&  !td_game.show_upgrade_dialog ){
        ImVec2 mouse_pos = ImGui::GetMousePos();
        const float rel_x = mouse_pos.x - grid_origin.x;
        const float rel_y = mouse_pos.y - grid_origin.y;
        if(rel_x >= 0 && rel_y >= 0){
            const int64_t col = static_cast<int64_t>(rel_x / td_game.cell_size);
            const int64_t row = static_cast<int64_t>(rel_y / td_game.cell_size);
            if(col >= 0 && col < td_game.grid_cols && row >= 0 && row < td_game.grid_rows){
                const size_t tower_idx = get_tower_idx_at(col, row);
                if(tower_idx < td_towers.size()){
                    // Clicked on an existing tower - open upgrade dialog.
                    td_game.show_upgrade_dialog = true;
                    td_game.upgrade_tower_idx = tower_idx;
                }else if(!is_path_cell(col, row) && td_game.credits > 0){
                    // Clicked on a buildable cell - place a new tower.
                    td_tower_t new_tower;
                    new_tower.grid_x = col;
                    new_tower.grid_y = row;
                    td_towers.push_back(new_tower);
                    td_game.credits -= 1;
                }
            }
        }
    }

    // Draw towers with visual variations based on level and type.
    for(size_t tower_idx = 0; tower_idx < td_towers.size(); ++tower_idx){
        const auto& tower = td_towers[tower_idx];
        const float cx = grid_origin.x + static_cast<float>((tower.grid_x + 0.5) * td_game.cell_size);
        const float cy = grid_origin.y + static_cast<float>((tower.grid_y + 0.5) * td_game.cell_size);
        // Scale size slightly based on level: higher levels are larger and more menacing.
        const float level_scale = 1.0f + 0.08f * static_cast<float>(tower.level - 1);
        const float half_size = static_cast<float>(td_game.cell_size * 0.35) * level_scale;

        // Determine tower colors based on type and level.
        ImU32 fill_color, border_color;
        switch(tower.tower_type){
            case td_tower_type_t::RapidFire:
                // Green-cyan for rapid fire.
                fill_color = ImColor(0.1f + 0.1f * (tower.level - 1), 0.7f, 0.3f + 0.1f * (tower.level - 1), 1.0f);
                border_color = ImColor(0.05f, 0.5f, 0.2f, 1.0f);
                break;
            case td_tower_type_t::Boom:
                // Orange-red for boom (explosive).
                fill_color = ImColor(0.9f, 0.3f + 0.1f * (tower.level - 1), 0.1f, 1.0f);
                border_color = ImColor(0.6f, 0.2f, 0.05f, 1.0f);
                break;
            default:
                // Blue for basic tower, getting darker/more intense at higher levels.
                fill_color = ImColor(0.2f, 0.3f + 0.1f * (tower.level - 1), 0.9f, 1.0f);
                border_color = ImColor(0.1f, 0.1f, 0.5f + 0.1f * (tower.level - 1), 1.0f);
                break;
        }

        // Draw tower based on type.
        constexpr float PI = 3.14159265f;
        if(tower.tower_type == td_tower_type_t::RapidFire){
            // RapidFire: Draw as a hexagon shape for distinction.
            const int num_sides = 6;
            ImVec2 hex_pts[6];
            for(int i = 0; i < num_sides; ++i){
                const float angle = static_cast<float>(i) * 2.0f * PI / static_cast<float>(num_sides) - PI / 6.0f;
                hex_pts[i] = ImVec2(cx + half_size * std::cos(angle), cy + half_size * std::sin(angle));
            }
            draw_list->AddConvexPolyFilled(hex_pts, num_sides, fill_color);
            for(int i = 0; i < num_sides; ++i){
                draw_list->AddLine(hex_pts[i], hex_pts[(i + 1) % num_sides], border_color, 2.0f);
            }
        }else if(tower.tower_type == td_tower_type_t::Boom){
            // Boom: Draw as an octagon (bigger, more menacing).
            const int num_sides = 8;
            ImVec2 oct_pts[8];
            for(int i = 0; i < num_sides; ++i){
                const float angle = static_cast<float>(i) * 2.0f * PI / static_cast<float>(num_sides);
                oct_pts[i] = ImVec2(cx + half_size * std::cos(angle), cy + half_size * std::sin(angle));
            }
            draw_list->AddConvexPolyFilled(oct_pts, num_sides, fill_color);
            for(int i = 0; i < num_sides; ++i){
                draw_list->AddLine(oct_pts[i], oct_pts[(i + 1) % num_sides], border_color, 2.0f);
            }
            // Add an inner circle to indicate explosive nature.
            draw_list->AddCircle(ImVec2(cx, cy), half_size * 0.5f, border_color, 16, 2.0f);
        }else{
            // Basic: Draw as a square with spikes for higher levels.
            draw_list->AddRectFilled(ImVec2(cx - half_size, cy - half_size),
                                     ImVec2(cx + half_size, cy + half_size),
                                     fill_color);
            draw_list->AddRect(ImVec2(cx - half_size, cy - half_size),
                               ImVec2(cx + half_size, cy + half_size),
                               border_color, 0.0f, 0, 2.0f);

            // Add corner spikes for higher levels to look more menacing.
            if(tower.level >= 2){
                const float spike_len = half_size * 0.3f;
                // Top-left spike.
                draw_list->AddTriangleFilled(
                    ImVec2(cx - half_size, cy - half_size),
                    ImVec2(cx - half_size - spike_len, cy - half_size - spike_len),
                    ImVec2(cx - half_size + spike_len * 0.5f, cy - half_size),
                    fill_color);
                // Top-right spike.
                draw_list->AddTriangleFilled(
                    ImVec2(cx + half_size, cy - half_size),
                    ImVec2(cx + half_size + spike_len, cy - half_size - spike_len),
                    ImVec2(cx + half_size - spike_len * 0.5f, cy - half_size),
                    fill_color);
                // Bottom-left spike.
                draw_list->AddTriangleFilled(
                    ImVec2(cx - half_size, cy + half_size),
                    ImVec2(cx - half_size - spike_len, cy + half_size + spike_len),
                    ImVec2(cx - half_size + spike_len * 0.5f, cy + half_size),
                    fill_color);
                // Bottom-right spike.
                draw_list->AddTriangleFilled(
                    ImVec2(cx + half_size, cy + half_size),
                    ImVec2(cx + half_size + spike_len, cy + half_size + spike_len),
                    ImVec2(cx + half_size - spike_len * 0.5f, cy + half_size),
                    fill_color);
            }
        }

        // Draw level indicator (small number or dots above tower).
        if(tower.level > 1){
            char level_str[30];
            std::snprintf(level_str, sizeof(level_str), "%ld", static_cast<long>(tower.level));
            //const ImVec2 text_pos(cx - 4.0f, cy - half_size - 12.0f);  // Hovering above the tower.
            const ImVec2 text_pos(cx - 3.0f, cy - 7.25f); // Centre of the tower.
            draw_list->AddText(text_pos, ImColor(0.0f, 0.0f, 0.0f, 1.0f), level_str);
        }

        // Handle tower tooltip on hover.
        ImVec2 mouse_pos = ImGui::GetMousePos();
        const float mouse_dist = std::sqrt((mouse_pos.x - cx) * (mouse_pos.x - cx) + (mouse_pos.y - cy) * (mouse_pos.y - cy));
        if( f
        && (mouse_dist < (half_size * 1.5f))
        &&  !td_game.show_upgrade_dialog){
            ImGui::BeginTooltip();
            ImGui::Text("Tower: %s", get_tower_type_name(tower.tower_type));
            ImGui::Text("Level: %ld", static_cast<long>(tower.level));
            ImGui::Text("Damage: %.1f", tower.damage);
            ImGui::Text("Range: %.1f", tower.range);
            if(tower.tower_type == td_tower_type_t::Boom){
                ImGui::Text("Blast radius: %.1f", compute_boom_radius(td_game.cell_size, tower.level));
            }
            ImGui::Text("Fire Rate: %.1f/s", tower.fire_rate);
            ImGui::Text("Total Damage: %.0f", tower.total_damage_dealt);
            ImGui::EndTooltip();

            // Draw range circle (subtle).
            ImU32 range_colour;
            switch(tower.tower_type){
                case td_tower_type_t::RapidFire:
                    range_colour = ImColor(0.1f, 0.8f, 0.3f, 0.6f);
                    break;
                case td_tower_type_t::Boom:
                    range_colour = ImColor(0.9f, 0.4f, 0.1f, 0.6f);
                    break;
                default:
                    range_colour = ImColor(0.2f, 0.2f, 0.8f, 0.5f);
                    break;
            }
            draw_list->AddCircle(ImVec2(cx, cy), static_cast<float>(tower.range),
                                 range_colour, 32, 1.0f);

            // Also draw the blast radius for boom towers.
            if(tower.tower_type == td_tower_type_t::Boom){
                ImU32 blast_colour = ImColor(1.0f, 0.1f, 0.1f, 0.6f);
                draw_list->AddCircle(ImVec2(cx, cy), static_cast<float>(compute_boom_radius(td_game.cell_size, tower.level)),
                                     blast_colour, 32, 1.0f);
            }
        }
    }

    // Update wave spawning.
    if(td_game.wave_active && td_game.enemies_in_wave > 0){
        td_game.spawn_timer -= dt;
        if(td_game.spawn_timer <= 0.0){
            // Spawn a new enemy.
            td_enemy_t new_enemy;
            new_enemy.path_progress = 0.0;
            //new_enemy.hp = 100.0 + (wn - 1.0) * 20.0;  // More HP for later waves.
            new_enemy.hp += (wn - 1.0) * 20.0;  // More HP for later waves.
            new_enemy.max_hp = new_enemy.hp;
            td_enemies.push_back(new_enemy);
            td_game.enemies_in_wave -= 1;
            td_game.spawn_timer = td_game.spawn_interval;
        }
    }

    // Check if wave is complete.
    if(td_game.wave_active && td_game.enemies_in_wave <= 0 && td_enemies.empty()){
        td_game.wave_active = false;
    }

    // Update enemies.
    for(auto& enemy : td_enemies){
        enemy.path_progress += (td_game.enemy_speed + wn/500.0) * dt;
    }

    // Check for enemies reaching the end.
    td_enemies.erase(
        std::remove_if(td_enemies.begin(), td_enemies.end(),
            [&](const td_enemy_t& enemy) -> bool {
                if(enemy.path_progress >= 1.0){
                    td_game.lives -= 1;
                    return true;
                }
                return false;
            }),
        td_enemies.end()
    );

    // Towers attack enemies.
    for(size_t tower_idx = 0; tower_idx < td_towers.size(); ++tower_idx){
        auto& tower = td_towers[tower_idx];
        tower.cooldown -= dt;
        if(tower.cooldown <= 0.0){
            const double tower_x = (static_cast<double>(tower.grid_x) + 0.5) * td_game.cell_size;
            const double tower_y = (static_cast<double>(tower.grid_y) + 0.5) * td_game.cell_size;
            const vec2<double> tower_pos(tower_x, tower_y);

            // Find closest enemy in range.
            double closest_dist = tower.range + 1.0;
            size_t closest_idx = td_enemies.size();
            for(size_t i = 0; i < td_enemies.size(); ++i){
                const auto enemy_pos = get_path_position(td_enemies[i].path_progress);
                const double dist = tower_pos.distance(enemy_pos);
                if(dist <= tower.range && dist < closest_dist){
                    closest_dist = dist;
                    closest_idx = i;
                }
            }

            if(closest_idx < td_enemies.size()){
                // Fire at the enemy.
                td_projectile_t proj;
                proj.pos = tower_pos;
                proj.target_pos = get_path_position(td_enemies[closest_idx].path_progress);
                proj.target_enemy_idx = closest_idx;  // May become stale, use position-based hit detection.
                proj.damage = tower.damage;
                proj.source_tower_idx = tower_idx;
                if(tower.tower_type == td_tower_type_t::Boom){
                    proj.explosion_radius = compute_boom_radius(td_game.cell_size, tower.level);
                }else{
                    proj.explosion_radius = 0.0;
                }
                td_projectiles.push_back(proj);
                tower.cooldown = 1.0 / tower.fire_rate;
            }
        }
    }

    // Update projectiles.
    for(auto& proj : td_projectiles){
        const vec2<double> dir = (proj.target_pos - proj.pos);
        const double dist = dir.length();
        if(dist > 0.0){
            const vec2<double> move = dir.unit() * proj.speed * dt;
            if(move.length() >= dist){
                proj.pos = proj.target_pos;
            }else{
                proj.pos = proj.pos + move;
            }
        }
    }

    // Check for projectile hits using position-based detection (not indices).
    // When a projectile reaches its target, damage enemies within hit radius (or explosion radius for Boom).
    const double hit_radius = td_game.cell_size * 0.4;
    td_projectiles.erase(
        std::remove_if(td_projectiles.begin(), td_projectiles.end(),
            [&](const td_projectile_t& proj) -> bool {
                const double dist_to_target = proj.pos.distance(proj.target_pos);
                if(dist_to_target < 5.0){
                    double total_damage_dealt = 0.0;
                    // Determine the damage radius: use explosion_radius for Boom, otherwise hit_radius.
                    const double damage_radius = (proj.explosion_radius > 0.0) ? proj.explosion_radius : hit_radius;

                    if(proj.explosion_radius > 0.0){
                        // AOE damage: hit all enemies within the explosion radius.
                        for(auto& enemy : td_enemies){
                            const auto enemy_pos = get_path_position(enemy.path_progress);
                            const double dist = proj.pos.distance(enemy_pos);
                            if(dist < damage_radius){
                                enemy.hp -= proj.damage;
                                total_damage_dealt += proj.damage;
                            }
                        }
                    }else{
                        // Single-target damage: only hit the closest enemy.
                        double closest_dist = damage_radius;
                        td_enemy_t* closest_enemy = nullptr;
                        for(auto& enemy : td_enemies){
                            const auto enemy_pos = get_path_position(enemy.path_progress);
                            const double dist = proj.pos.distance(enemy_pos);
                            if(dist < closest_dist){
                                closest_dist = dist;
                                closest_enemy = &enemy;
                            }
                        }
                        if(closest_enemy != nullptr){
                            closest_enemy->hp -= proj.damage;
                            total_damage_dealt += proj.damage;
                        }
                    }

                    // Track cumulative damage dealt by the source tower.
                    if(proj.source_tower_idx < td_towers.size()){
                        td_towers[proj.source_tower_idx].total_damage_dealt += total_damage_dealt;
                    }
                    return true;
                }
                return false;
            }),
        td_projectiles.end()
    );

    // Remove dead enemies and grant credits.
    size_t enemies_killed = 0;
    td_enemies.erase(
        std::remove_if(td_enemies.begin(), td_enemies.end(),
            [&](const td_enemy_t& enemy) -> bool {
                if(enemy.hp <= 0.0){
                    ++enemies_killed;
                    return true;
                }
                return false;
            }),
        td_enemies.end()
    );
    td_game.credits += static_cast<int64_t>(enemies_killed);

    // Draw enemies (red circles with health bars).
    for(const auto& enemy : td_enemies){
        const auto pos = get_path_position(enemy.path_progress);
        const float ex = grid_origin.x + static_cast<float>(pos.x);
        const float ey = grid_origin.y + static_cast<float>(pos.y);
        const float enemy_radius = static_cast<float>(td_game.cell_size * 0.3);

        // Draw enemy circle.
        draw_list->AddCircleFilled(ImVec2(ex, ey), enemy_radius,
                                   ImColor(0.9f, 0.2f, 0.2f, 1.0f));
        draw_list->AddCircle(ImVec2(ex, ey), enemy_radius,
                             ImColor(0.5f, 0.1f, 0.1f, 1.0f), 0, 2.0f);

        // Draw health bar above enemy.
        const float bar_width = td_game.cell_size * 0.6f;
        const float bar_height = 4.0f;
        const float bar_x = ex - bar_width / 2.0f;
        const float bar_y = ey - enemy_radius - 8.0f;
        const float hp_ratio = static_cast<float>(enemy.hp / enemy.max_hp);

        draw_list->AddRectFilled(ImVec2(bar_x, bar_y),
                                 ImVec2(bar_x + bar_width, bar_y + bar_height),
                                 ImColor(0.3f, 0.3f, 0.3f, 1.0f));
        draw_list->AddRectFilled(ImVec2(bar_x, bar_y),
                                 ImVec2(bar_x + bar_width * hp_ratio, bar_y + bar_height),
                                 ImColor(0.0f, 0.8f, 0.0f, 1.0f));

        // Handle enemy tooltip on hover.
        ImVec2 mouse_pos = ImGui::GetMousePos();
        const float mouse_dist = std::sqrt((mouse_pos.x - ex) * (mouse_pos.x - ex) + (mouse_pos.y - ey) * (mouse_pos.y - ey));
        if( f
        &&  (mouse_dist < (enemy_radius * 1.5f))
        &&  !td_game.show_upgrade_dialog ){
            // Calculate enemy's current speed (base speed + wave scaling).
            const double current_speed = (td_game.enemy_speed + wn/500.0) * enemy.speed_multiplier;
            ImGui::BeginTooltip();
            ImGui::Text("Enemy");
            ImGui::Text("Health: %.0f / %.0f", enemy.hp, enemy.max_hp);
            ImGui::Text("Speed: %.3f", current_speed);
            ImGui::EndTooltip();
        }
    }

    // Draw projectiles (yellow circles, or orange for explosive).
    for(const auto& proj : td_projectiles){
        const float px = grid_origin.x + static_cast<float>(proj.pos.x);
        const float py = grid_origin.y + static_cast<float>(proj.pos.y);
        if(proj.explosion_radius > 0.0){
            // Explosive projectile: draw as orange.
            draw_list->AddCircleFilled(ImVec2(px, py), 5.0f, ImColor(1.0f, 0.5f, 0.0f, 1.0f));
        }else{
            draw_list->AddCircleFilled(ImVec2(px, py), 4.0f, ImColor(1.0f, 1.0f, 0.0f, 1.0f));
        }
    }

    // Tower upgrade dialog.
    if( td_game.show_upgrade_dialog
    &&  (td_game.upgrade_tower_idx < td_towers.size()) ){
        auto& tower = td_towers[td_game.upgrade_tower_idx];
        const float cx = grid_origin.x + static_cast<float>((tower.grid_x + 0.5) * td_game.cell_size);
        const float cy = grid_origin.y + static_cast<float>((tower.grid_y + 0.5) * td_game.cell_size);

        ImGui::SetNextWindowPos(ImVec2(cx + 30.0f, cy - 30.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(180, 0), ImGuiCond_Always);
        bool dialog_open = true;
        if(ImGui::Begin("Upgrade Tower", &dialog_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)){
            ImGui::SetWindowFocus();
            ImGui::Text("Tower: %s", get_tower_type_name(tower.tower_type));
            ImGui::Text("Level: %ld", static_cast<long>(tower.level));
            ImGui::Text("Damage: %.1f", tower.damage);
            ImGui::Text("Range: %.1f", tower.range);
            if(tower.tower_type == td_tower_type_t::Boom){
                ImGui::Text("Blast radius: %.1f", compute_boom_radius(td_game.cell_size, tower.level));
            }
            ImGui::Text("Fire Rate: %.1f/s", tower.fire_rate);
            ImGui::Text("Total Damage: %.0f", tower.total_damage_dealt);
            ImGui::Separator();

            // Level upgrade option.
            const int64_t upgrade_cost = get_upgrade_cost(tower.level);
            char upgrade_btn_label[64];
            std::snprintf(upgrade_btn_label, sizeof(upgrade_btn_label), "Upgrade Lvl (%ld credits)", static_cast<long>(upgrade_cost));
            const bool can_afford_upgrade = (td_game.credits >= upgrade_cost);
            if(!can_afford_upgrade) ImGui::BeginDisabled();
            if(ImGui::Button(upgrade_btn_label, ImVec2(-1, 0))){
                // Upgrade the tower.
                tower.level += 1;
                tower.damage += 5.0;  // Increase damage.
                tower.range += 10.0;  // Slight range increase.
                if(tower.tower_type == td_tower_type_t::RapidFire){
                    tower.fire_rate += 0.5;  // RapidFire gets more fire rate per level.
                }else{
                    tower.fire_rate += 0.25;  // Others get some fire rate increase.
                }
                td_game.credits -= upgrade_cost;
                td_game.show_upgrade_dialog = false;
            }
            if(!can_afford_upgrade) ImGui::EndDisabled();

            // Tower type upgrades (only available for Basic towers).
            if(tower.tower_type == td_tower_type_t::Basic){
                ImGui::Separator();
                ImGui::Text("Convert to:");

                // Rapid Fire upgrade.
                const int64_t rapidfire_cost = get_type_upgrade_cost(td_tower_type_t::RapidFire);
                char rapidfire_label[64];
                std::snprintf(rapidfire_label, sizeof(rapidfire_label), "Rapid Fire (%ld)", static_cast<long>(rapidfire_cost));
                const bool can_afford_rapidfire = (td_game.credits >= rapidfire_cost);
                if(!can_afford_rapidfire) ImGui::BeginDisabled();
                if(ImGui::Button(rapidfire_label, ImVec2(-1, 0))){
                    tower.tower_type = td_tower_type_t::RapidFire;
                    tower.fire_rate = 5.0;  // Higher fire rate.
                    tower.damage = 8.0;     // Lower damage per shot.
                    td_game.credits -= rapidfire_cost;
                    td_game.show_upgrade_dialog = false;
                }
                if(!can_afford_rapidfire) ImGui::EndDisabled();

                // Boom upgrade.
                const int64_t boom_cost = get_type_upgrade_cost(td_tower_type_t::Boom);
                char boom_label[64];
                std::snprintf(boom_label, sizeof(boom_label), "Boom (%ld)", static_cast<long>(boom_cost));
                const bool can_afford_boom = (td_game.credits >= boom_cost);
                if(!can_afford_boom) ImGui::BeginDisabled();
                if(ImGui::Button(boom_label, ImVec2(-1, 0))){
                    tower.tower_type = td_tower_type_t::Boom;
                    tower.fire_rate = 1.0;  // Slower fire rate.
                    tower.damage = 20.0;    // Higher damage.
                    td_game.credits -= boom_cost;
                    td_game.show_upgrade_dialog = false;
                }
                if(!can_afford_boom) ImGui::EndDisabled();
            }

            if(ImGui::Button("Close", ImVec2(-1, 0))){
                td_game.show_upgrade_dialog = false;
            }
        }
        ImGui::End();
        if(!dialog_open){
            td_game.show_upgrade_dialog = false;
        }
    }

    // Create an invisible button to capture mouse input on the grid area.
    ImGui::SetCursorScreenPos(grid_origin);
    ImGui::InvisibleButton("td_grid", ImVec2(box_width, box_height));

    ImGui::End();

    return true;
}

