// Lawn.cc - A part of DICOMautomaton 2026. Written by hal clark.

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

#include "Lawn.h"

LawnGame::LawnGame(){
    Reset();
}

void LawnGame::Reset(){
    lg_towers.clear();
    lg_enemies.clear();
    lg_projectiles.clear();
    lg_tokens.clear();

    lg_game.tokens = 5;
    lg_game.lives = 10;
    lg_game.spawn_timer = 0.0;
    lg_game.selected_tower = lg_tower_type_t::Sun;
    lg_game.enemy_spawn_count = 0;

    lg_game.board_width = lg_game.cols * lg_game.cell_width;
    lg_game.board_height = lg_game.lanes * lg_game.lane_height;

    const auto t_now = std::chrono::steady_clock::now();
    t_lg_updated = t_now;

    lg_game.re.seed(std::random_device()());
    return;
}


bool LawnGame::Display(bool &enabled){
    if( !enabled ) return true;

    const auto win_width = static_cast<int>(lg_game.board_width + 260.0);
    const auto win_height = static_cast<int>(lg_game.board_height + 90.0);

    auto flags = ImGuiWindowFlags_AlwaysAutoResize
               | ImGuiWindowFlags_NoScrollWithMouse
               | ImGuiWindowFlags_NoNavInputs
               | ImGuiWindowFlags_NoScrollbar;
    ImGui::SetNextWindowSize(ImVec2(win_width, win_height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(150, 150), ImGuiCond_FirstUseEver);
    ImGui::Begin("Lawn Game", &enabled, flags);

    const auto f = ImGui::IsWindowFocused();
    if(f && ImGui::IsKeyPressed(SDL_SCANCODE_R)){
        Reset();
    }

    // Time update.
    const auto t_now = std::chrono::steady_clock::now();
    auto t_diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_lg_updated).count();
    if(t_diff_ms > 50) t_diff_ms = 50;
    const double dt = static_cast<double>(t_diff_ms) / 1000.0;
    t_lg_updated = t_now;
    constexpr double pi = 3.14159265358979323846;
    constexpr double sun_initial_cooldown = 2.0;
    constexpr double sun_token_cooldown_min = 8.0;
    constexpr double sun_token_cooldown_max = 25.0;
    constexpr double token_click_tolerance = 4.0;
    constexpr double enemy_attack_distance = 0.25; // 25% of cell width
    constexpr double enemy_spawn_offset = 15.0;
    constexpr double projectile_hit_radius = 12.0;
    constexpr double shooter_projectile_damage = lg_default_projectile_damage;
    constexpr double enemy_attack_damage = 12.0;
    constexpr double default_enemy_hp = lg_default_enemy_hp;
    constexpr double default_tower_hp = lg_default_tower_hp;
    constexpr double hp_regen_rate = 0.5; // HP regeneration per second (slow)
    constexpr double attack_animation_duration = 0.3; // Duration of attack animation
    constexpr double damage_animation_duration = 0.2; // Duration of damage animation
    
    std::uniform_real_distribution<double> sun_dist(sun_token_cooldown_min, sun_token_cooldown_max);

    // Control panel.
    ImGui::BeginChild("LawnInfo", ImVec2(240, lg_game.board_height), true);
    ImGui::Text("Lives: %ld", static_cast<long>(lg_game.lives));
    ImGui::Text("Tokens: %ld", static_cast<long>(lg_game.tokens));
    ImGui::Separator();
    ImGui::Text("Build:");
    const auto tower_button = [&](const char* label, lg_tower_type_t type, int64_t cost){
        if(lg_game.tokens < cost) ImGui::BeginDisabled();
        bool pressed = ImGui::Selectable(label, lg_game.selected_tower == type);
        if(pressed) lg_game.selected_tower = type;
        if(lg_game.tokens < cost) ImGui::EndDisabled();
    };
    tower_button("Sun (1)", lg_tower_type_t::Sun, 1);
    tower_button("Shooter (2)", lg_tower_type_t::Shooter, 2);
    tower_button("Blocker (3)", lg_tower_type_t::Blocker, 3);
    ImGui::Separator();
    ImGui::TextWrapped("Click a lane tile to place the selected tower.");
    ImGui::TextWrapped("Click tokens to collect. R to reset.");
    if(lg_game.lives <= 0){
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "GAME OVER");
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImVec2 board_origin = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const bool mouse_clicked = ImGui::IsMouseClicked(0);
    const ImVec2 mouse_pos = ImGui::GetMousePos();

    // Draw board background.
    draw_list->AddRectFilled(board_origin,
                             ImVec2(board_origin.x + lg_game.board_width,
                                    board_origin.y + lg_game.board_height),
                             ImColor(0.18f, 0.35f, 0.18f, 1.0f));
    draw_list->AddRect(board_origin,
                       ImVec2(board_origin.x + lg_game.board_width,
                              board_origin.y + lg_game.board_height),
                       ImColor(0.1f, 0.2f, 0.1f, 1.0f), 0.0f, 0, 2.0f);

    // Lane lines.
    for(int64_t lane_idx = 1; lane_idx < lg_game.lanes; ++lane_idx){
        const float y = board_origin.y + static_cast<float>(lane_idx * lg_game.lane_height);
        draw_list->AddLine(ImVec2(board_origin.x, y),
                           ImVec2(board_origin.x + lg_game.board_width, y),
                           ImColor(0.12f, 0.25f, 0.12f, 1.0f), 1.0f);
    }

    // Column lines.
    for(int64_t col_idx = 1; col_idx < lg_game.cols; ++col_idx){
        const float x = board_origin.x + static_cast<float>(col_idx * lg_game.cell_width);
        draw_list->AddLine(ImVec2(x, board_origin.y),
                           ImVec2(x, board_origin.y + lg_game.board_height),
                           ImColor(0.15f, 0.3f, 0.15f, 1.0f), 1.0f);
    }

    bool clicked_token = false;
    if(f && mouse_clicked){
        for(const auto& token : lg_tokens){
            const ImVec2 token_pos(board_origin.x + static_cast<float>(token.pos.x),
                                   board_origin.y + static_cast<float>(token.pos.y));
            const float dx = mouse_pos.x - token_pos.x;
            const float dy = mouse_pos.y - token_pos.y;
            if(std::sqrt(dx * dx + dy * dy) < token.radius + token_click_tolerance){
                clicked_token = true;
                break;
            }
        }
    }

    // Placement interaction.
    if(f && mouse_clicked && !clicked_token && lg_game.lives > 0){
        const float rel_x = mouse_pos.x - board_origin.x;
        const float rel_y = mouse_pos.y - board_origin.y;
        if(rel_x >= 0.0f && rel_y >= 0.0f && rel_x < static_cast<float>(lg_game.board_width) && rel_y < static_cast<float>(lg_game.board_height)){
            const int64_t col = static_cast<int64_t>(rel_x / lg_game.cell_width);
            const int64_t lane = static_cast<int64_t>(rel_y / lg_game.lane_height);
            const auto has_tower = [&](int64_t lane_check, int64_t col_check){
                for(const auto& tower : lg_towers){
                    if(tower.lane == lane_check && tower.col == col_check) return true;
                }
                return false;
            };
            if(!has_tower(lane, col)){
                int64_t cost = 0;
                switch(lg_game.selected_tower){
                    case lg_tower_type_t::Sun: cost = 1; break;
                    case lg_tower_type_t::Shooter: cost = 2; break;
                    case lg_tower_type_t::Blocker: cost = 3; break;
                }
                if(lg_game.tokens >= cost){
                    lg_tower_t tower;
                    tower.lane = lane;
                    tower.col = col;
                    tower.type = lg_game.selected_tower;
                    switch(tower.type){
                        case lg_tower_type_t::Sun:
                            tower.hp = default_tower_hp;
                            tower.max_hp = default_tower_hp;
                            tower.cooldown = sun_initial_cooldown;
                            // Randomize sun production time between 8-15 seconds
                            tower.sun_production_time = sun_dist(lg_game.re);
                            break;
                        case lg_tower_type_t::Shooter:
                            tower.hp = default_tower_hp;
                            tower.max_hp = default_tower_hp;
                            tower.cooldown = 1.0;
                            break;
                        case lg_tower_type_t::Blocker:
                            tower.hp = default_tower_hp * 3.0;
                            tower.max_hp = default_tower_hp * 3.0;
                            tower.cooldown = 0.0;
                            break;
                    }
                    lg_towers.push_back(tower);
                    lg_game.tokens -= cost;
                }
            }
        }
    }

    // Update tokens from sun towers.
    if(lg_game.lives > 0){
        for(auto& tower : lg_towers){
            if(tower.type != lg_tower_type_t::Sun) continue;
            tower.cooldown -= dt;
            if(tower.cooldown <= 0.0){
                lg_token_t token;
                const double cx = (static_cast<double>(tower.col) + 0.5) * lg_game.cell_width;
                const double cy = (static_cast<double>(tower.lane) + 0.5) * lg_game.lane_height;
                token.pos = vec2<double>(cx, cy - 12.0);
                token.created = t_now;
                lg_tokens.push_back(token);
                tower.cooldown = tower.sun_production_time; // Use randomized production time
            }
        }
    }

    // Spawn enemies.
    if(lg_game.lives > 0){
        lg_game.spawn_timer -= dt;
        if(lg_game.spawn_timer <= 0.0){
            std::uniform_int_distribution<int64_t> lane_dist(0, lg_game.lanes - 1);
            lg_enemy_t enemy;
            enemy.lane = lane_dist(lg_game.re);
            enemy.x = lg_game.board_width - enemy_spawn_offset;
            
            // Scale difficulty based on spawn count
            const double difficulty_factor = 1.0 + (static_cast<double>(lg_game.enemy_spawn_count) * 0.05); // 5% increase per spawn
            enemy.hp = default_enemy_hp * difficulty_factor;
            enemy.max_hp = default_enemy_hp * difficulty_factor;
            enemy.speed_multiplier = 1.0 + (static_cast<double>(lg_game.enemy_spawn_count) * 0.02); // 2% speed increase per spawn
            enemy.attack_cooldown = 0.0;
            
            lg_enemies.push_back(enemy);
            lg_game.spawn_timer = lg_game.spawn_interval;
            lg_game.enemy_spawn_count += 1; // Increment spawn count
        }
    }

    const auto lane_y_center = [&](int64_t lane){
        return (static_cast<double>(lane) + 0.5) * lg_game.lane_height;
    };

    const auto tower_x_center = [&](int64_t col){
        return (static_cast<double>(col) + 0.5) * lg_game.cell_width;
    };

    // Update enemies and attacks.
    if(lg_game.lives > 0){
        for(auto& enemy : lg_enemies){
            int64_t blocking_col = -1;
            lg_tower_t* target_tower = nullptr;
            double min_distance = std::numeric_limits<double>::max();
            
            for(auto& tower : lg_towers){
                if(tower.lane != enemy.lane) continue;
                const double tower_x = tower_x_center(tower.col);
                const double distance = enemy.x - tower_x;
                
                // Enemy is past or at the tower
                if(distance >= -5.0 && tower_x <= enemy.x){
                    if(blocking_col < 0 || tower.col > blocking_col){
                        blocking_col = tower.col;
                        target_tower = &tower;
                        min_distance = distance;
                    }
                }
            }
            
            // Check if enemy is within attack range (25% of cell width)
            const double attack_range = lg_game.cell_width * enemy_attack_distance;
            bool is_in_attack_range = (target_tower != nullptr) && (min_distance <= attack_range);
            
            if(is_in_attack_range && target_tower != nullptr){
                // Enemy attacks tower
                enemy.attack_cooldown -= dt;
                if(enemy.attack_cooldown <= 0.0){
                    target_tower->hp -= enemy_attack_damage;
                    target_tower->damage_animation_timer = damage_animation_duration; // Trigger damage animation
                    enemy.attack_animation_timer = attack_animation_duration; // Trigger attack animation
                    enemy.attack_cooldown = 0.8;
                }
            }else{
                // Enemy moves
                enemy.x -= lg_game.enemy_speed * enemy.speed_multiplier * dt;
            }
            
            // Slow HP regeneration for enemies (only when not at max HP)
            if(enemy.hp < enemy.max_hp){
                enemy.hp += hp_regen_rate * dt;
                if(enemy.hp > enemy.max_hp) enemy.hp = enemy.max_hp;
            }
            
            // Update animation timers
            if(enemy.attack_animation_timer > 0.0){
                enemy.attack_animation_timer -= dt;
            }
            if(enemy.damage_animation_timer > 0.0){
                enemy.damage_animation_timer -= dt;
            }
        }
    }

    // Update tower HP regeneration and animation timers.
    if(lg_game.lives > 0){
        for(auto& tower : lg_towers){
            // Slow HP regeneration for towers (only when not at max HP)
            if(tower.hp < tower.max_hp){
                tower.hp += hp_regen_rate * dt;
                if(tower.hp > tower.max_hp) tower.hp = tower.max_hp;
            }
            
            // Update animation timers
            if(tower.attack_animation_timer > 0.0){
                tower.attack_animation_timer -= dt;
            }
            if(tower.damage_animation_timer > 0.0){
                tower.damage_animation_timer -= dt;
            }
        }
    }

    // Remove dead towers.
    lg_towers.erase(
        std::remove_if(lg_towers.begin(), lg_towers.end(),
            [&](const lg_tower_t& tower) -> bool {
                return tower.hp <= 0.0;
            }),
        lg_towers.end()
    );

    // Shooter towers fire projectiles.
    if(lg_game.lives > 0){
        for(auto& tower : lg_towers){
            if(tower.type != lg_tower_type_t::Shooter) continue;
            tower.cooldown -= dt;
            if(tower.cooldown <= 0.0){
                bool enemy_in_lane = false;
                for(const auto& enemy : lg_enemies){
                    if(enemy.lane == tower.lane && enemy.x >= tower_x_center(tower.col)){
                        enemy_in_lane = true;
                        break;
                    }
                }
                if(enemy_in_lane){
                    lg_projectile_t proj;
                    proj.lane = tower.lane;
                    proj.x = tower_x_center(tower.col) + 10.0;
                    proj.damage = shooter_projectile_damage;
                    lg_projectiles.push_back(proj);
                    tower.cooldown = 1.0;
                    tower.attack_animation_timer = attack_animation_duration; // Trigger attack animation
                }
            }
        }
    }

    // Update projectiles and handle hits.
    for(auto& proj : lg_projectiles){
        proj.x += proj.speed * dt;
    }
    lg_projectiles.erase(
        std::remove_if(lg_projectiles.begin(), lg_projectiles.end(),
            [&](const lg_projectile_t& proj) -> bool {
                bool remove = false;
                for(auto& enemy : lg_enemies){
                    if(enemy.lane != proj.lane) continue;
                    if(std::abs(enemy.x - proj.x) < projectile_hit_radius){
                        enemy.hp -= proj.damage;
                        enemy.damage_animation_timer = damage_animation_duration; // Trigger damage animation
                        remove = true;
                        break;
                    }
                }
                if(proj.x > lg_game.board_width + 20.0) remove = true;
                return remove;
            }),
        lg_projectiles.end()
    );

    // Remove dead enemies.
    lg_enemies.erase(
        std::remove_if(lg_enemies.begin(), lg_enemies.end(),
            [&](const lg_enemy_t& enemy) -> bool {
                return enemy.hp <= 0.0;
            }),
        lg_enemies.end()
    );

    // Check for enemies reaching the left edge.
    lg_enemies.erase(
        std::remove_if(lg_enemies.begin(), lg_enemies.end(),
            [&](const lg_enemy_t& enemy) -> bool {
                if(enemy.x <= 0.0){
                    lg_game.lives -= 1;
                    return true;
                }
                return false;
            }),
        lg_enemies.end()
    );

    // Token collection.
    lg_tokens.erase(
        std::remove_if(lg_tokens.begin(), lg_tokens.end(),
            [&](const lg_token_t& token) -> bool {
                const auto age_seconds = std::chrono::duration_cast<std::chrono::seconds>(t_now - token.created).count();
                if(age_seconds > 8) return true;
                const ImVec2 token_pos(board_origin.x + static_cast<float>(token.pos.x),
                                       board_origin.y + static_cast<float>(token.pos.y));
                const float dx = mouse_pos.x - token_pos.x;
                const float dy = mouse_pos.y - token_pos.y;
                if(mouse_clicked && std::sqrt(dx * dx + dy * dy) < token.radius + token_click_tolerance){
                    lg_game.tokens += 1;
                    return true;
                }
                return false;
            }),
        lg_tokens.end()
    );

    // Draw towers.
    for(const auto& tower : lg_towers){
        const float cx = board_origin.x + static_cast<float>(tower_x_center(tower.col));
        const float cy = board_origin.y + static_cast<float>(lane_y_center(tower.lane));
        float radius = static_cast<float>(lg_game.cell_width * 0.25);
        
        // Apply damage animation (shake/pulse effect)
        float cx_anim = cx;
        float cy_anim = cy;
        if(tower.damage_animation_timer > 0.0){
            const float shake = 3.0f * static_cast<float>(tower.damage_animation_timer / damage_animation_duration);
            cx_anim += shake * std::sin(static_cast<float>(tower.damage_animation_timer) * 50.0f);
        }
        
        // Apply attack animation (slightly enlarge)
        if(tower.attack_animation_timer > 0.0){
            const float scale = 1.0f + 0.2f * static_cast<float>(tower.attack_animation_timer / attack_animation_duration);
            radius *= scale;
        }
        
        ImU32 fill_col = 0;
        ImU32 border_col = 0;
        ImU32 detail_col = 0;
        char label = '?';
        if(tower.type == lg_tower_type_t::Sun){
            fill_col = ImColor(0.95f, 0.85f, 0.2f, 1.0f);
            border_col = ImColor(0.8f, 0.65f, 0.1f, 1.0f);
            detail_col = ImColor(0.98f, 0.95f, 0.5f, 1.0f);
            label = 'S';
            
            // Draw sun with rays
            draw_list->AddCircleFilled(ImVec2(cx_anim, cy_anim), radius, fill_col);
            draw_list->AddCircle(ImVec2(cx_anim, cy_anim), radius, border_col, 0, 2.5f);
            
            // Draw sun rays
            const int num_rays = 8;
            const float pi_f = static_cast<float>(pi);
            for(int i = 0; i < num_rays; ++i){
                const float angle = static_cast<float>(i) * 2.0f * pi_f / static_cast<float>(num_rays);
                const float ray_start = radius;
                const float ray_end = radius * 1.3f;
                const float sx = cx_anim + ray_start * std::cos(angle);
                const float sy = cy_anim + ray_start * std::sin(angle);
                const float ex = cx_anim + ray_end * std::cos(angle);
                const float ey = cy_anim + ray_end * std::sin(angle);
                draw_list->AddLine(ImVec2(sx, sy), ImVec2(ex, ey), detail_col, 2.0f);
            }
            
            // Draw inner circle for detail
            draw_list->AddCircleFilled(ImVec2(cx_anim, cy_anim), radius * 0.5f, detail_col);
        }else if(tower.type == lg_tower_type_t::Shooter){
            fill_col = ImColor(0.2f, 0.7f, 0.3f, 1.0f);
            border_col = ImColor(0.1f, 0.4f, 0.15f, 1.0f);
            detail_col = ImColor(0.3f, 0.9f, 0.4f, 1.0f);
            label = 'P';
            
            // Draw shooter as a cannon/turret
            // Base (hexagon)
            const int base_sides = 6;
            ImVec2 base_pts[base_sides];
            const float pi_f = static_cast<float>(pi);
            for(int i = 0; i < base_sides; ++i){
                const float angle = static_cast<float>(i) * 2.0f * pi_f / static_cast<float>(base_sides);
                base_pts[i] = ImVec2(cx_anim + radius * 0.8f * std::cos(angle), 
                                    cy_anim + radius * 0.8f * std::sin(angle));
            }
            draw_list->AddConvexPolyFilled(base_pts, base_sides, fill_col);
            for(int i = 0; i < base_sides; ++i){
                draw_list->AddLine(base_pts[i], base_pts[(i + 1) % base_sides], border_col, 2.0f);
            }
            
            // Cannon barrel (rectangle pointing right)
            const float barrel_width = radius * 0.3f;
            const float barrel_length = radius * 0.9f;
            ImVec2 barrel[4] = {
                ImVec2(cx_anim, cy_anim - barrel_width * 0.5f),
                ImVec2(cx_anim + barrel_length, cy_anim - barrel_width * 0.5f),
                ImVec2(cx_anim + barrel_length, cy_anim + barrel_width * 0.5f),
                ImVec2(cx_anim, cy_anim + barrel_width * 0.5f)
            };
            draw_list->AddConvexPolyFilled(barrel, 4, detail_col);
            draw_list->AddPolyline(barrel, 4, border_col, ImDrawFlags_Closed, 2.0f);
            
            // Center circle
            draw_list->AddCircleFilled(ImVec2(cx_anim, cy_anim), radius * 0.35f, detail_col);
            draw_list->AddCircle(ImVec2(cx_anim, cy_anim), radius * 0.35f, border_col, 0, 2.0f);
        }else{
            // Blocker tower
            fill_col = ImColor(0.45f, 0.65f, 0.45f, 1.0f);
            border_col = ImColor(0.25f, 0.4f, 0.25f, 1.0f);
            detail_col = ImColor(0.6f, 0.8f, 0.6f, 1.0f);
            label = 'B';
            
            // Draw blocker as a fortress/wall
            // Outer octagon
            const int sides = 8;
            ImVec2 oct_pts[sides];
            const float pi_f = static_cast<float>(pi);
            for(int i = 0; i < sides; ++i){
                const float angle = static_cast<float>(i) * 2.0f * pi_f / static_cast<float>(sides);
                oct_pts[i] = ImVec2(cx_anim + radius * std::cos(angle), cy_anim + radius * std::sin(angle));
            }
            draw_list->AddConvexPolyFilled(oct_pts, sides, fill_col);
            for(int i = 0; i < sides; ++i){
                draw_list->AddLine(oct_pts[i], oct_pts[(i + 1) % sides], border_col, 2.5f);
            }
            
            // Inner square for detail
            const float inner_size = radius * 0.6f;
            draw_list->AddRectFilled(ImVec2(cx_anim - inner_size, cy_anim - inner_size),
                                    ImVec2(cx_anim + inner_size, cy_anim + inner_size),
                                    detail_col);
            draw_list->AddRect(ImVec2(cx_anim - inner_size, cy_anim - inner_size),
                              ImVec2(cx_anim + inner_size, cy_anim + inner_size),
                              border_col, 0.0f, 0, 2.0f);
            
            // Draw battlements (num_battlements must be >= 2 for spacing calculation)
            const int num_battlements = 4;
            const float battlement_w = inner_size * 0.4f;
            const float battlement_h = radius * 0.3f;
            for(int i = 0; i < num_battlements; ++i){
                float bx = cx_anim - inner_size + (i * 2.0f * inner_size / (num_battlements - 1)) - battlement_w * 0.5f;
                float by = cy_anim - radius - battlement_h;
                draw_list->AddRectFilled(ImVec2(bx, by), ImVec2(bx + battlement_w, by + battlement_h), fill_col);
                draw_list->AddRect(ImVec2(bx, by), ImVec2(bx + battlement_w, by + battlement_h), border_col, 0.0f, 0, 1.5f);
            }
        }
        
        char text_str[2] = { label, '\0' };
        const ImVec2 text_sz = ImGui::CalcTextSize(text_str);
        draw_list->AddText(ImVec2(cx_anim - text_sz.x * 0.5f, cy_anim - text_sz.y * 0.5f),
                           ImColor(0.1f, 0.1f, 0.1f, 1.0f), text_str);

        // Draw HP indicator (only when not at max HP)
        if(tower.hp < tower.max_hp){
            const float bar_w = lg_game.cell_width * 0.5f;
            const float bar_h = 5.0f;
            const float bar_x = cx_anim - bar_w * 0.5f;
            const float bar_y = cy_anim - radius - 12.0f;
            const float hp_ratio = static_cast<float>(tower.hp / tower.max_hp);
            draw_list->AddRectFilled(ImVec2(bar_x, bar_y),
                                     ImVec2(bar_x + bar_w, bar_y + bar_h),
                                     ImColor(0.2f, 0.2f, 0.2f, 1.0f));
            draw_list->AddRectFilled(ImVec2(bar_x, bar_y),
                                     ImVec2(bar_x + bar_w * hp_ratio, bar_y + bar_h),
                                     ImColor(0.2f, 0.8f, 0.2f, 1.0f));
        }

        // Draw a tooltip if the mouse is hovering over the tower.
        const float mouse_dist = std::sqrt((mouse_pos.x - cx) * (mouse_pos.x - cx) + (mouse_pos.y - cy) * (mouse_pos.y - cy));
        if( f
        &&  (mouse_dist < (radius * 1.0f)) ){
            ImGui::SetNextWindowSize(ImVec2(600, -1));
            ImGui::BeginTooltip();
            ImGui::Text("Tower");
            ImGui::Text("Lane: %ld", static_cast<long>(tower.lane));
            ImGui::Text("Column: %ld", static_cast<long>(tower.col));
            ImGui::Text("HP: %.1f / %.1f", static_cast<float>(tower.hp), static_cast<float>(tower.max_hp));
            ImGui::Text("Cooldown: %.1f", static_cast<float>(tower.cooldown));
            ImGui::EndTooltip();
        }
    }

    // Draw enemies.
    for(const auto& enemy : lg_enemies){
        const float ex = board_origin.x + static_cast<float>(enemy.x);
        const float ey = board_origin.y + static_cast<float>(lane_y_center(enemy.lane));
        float radius = static_cast<float>(lg_game.cell_width * 0.23);
        
        // Apply damage animation (shake/flash effect)
        float ex_anim = ex;
        float ey_anim = ey;
        if(enemy.damage_animation_timer > 0.0){
            const float shake = 2.5f * static_cast<float>(enemy.damage_animation_timer / damage_animation_duration);
            ex_anim += shake * std::sin(static_cast<float>(enemy.damage_animation_timer) * 60.0f);
            ey_anim += shake * std::cos(static_cast<float>(enemy.damage_animation_timer) * 60.0f);
        }
        
        // Apply attack animation (lean forward)
        float lean_offset = 0.0f;
        if(enemy.attack_animation_timer > 0.0){
            lean_offset = -5.0f * static_cast<float>(enemy.attack_animation_timer / attack_animation_duration);
        }
        
        const ImU32 fill_col = ImColor(0.35f, 0.1f, 0.1f, 1.0f);
        const ImU32 border_col = ImColor(0.6f, 0.15f, 0.15f, 1.0f);
        const ImU32 detail_col = ImColor(0.5f, 0.2f, 0.2f, 1.0f);
        const ImU32 eye_col = ImColor(0.9f, 0.1f, 0.1f, 1.0f);
        
        // Draw enemy body (larger circle)
        draw_list->AddCircleFilled(ImVec2(ex_anim + lean_offset, ey_anim), radius, fill_col);
        draw_list->AddCircle(ImVec2(ex_anim + lean_offset, ey_anim), radius, border_col, 0, 2.5f);
        
        // Draw spikes/details around the enemy
        const int num_spikes = 6;
        const float pi_f = static_cast<float>(pi);
        for(int i = 0; i < num_spikes; ++i){
            const float angle = static_cast<float>(i) * 2.0f * pi_f / static_cast<float>(num_spikes);
            const float spike_base = radius * 0.8f;
            const float spike_tip = radius * 1.15f;
            const float sx = ex_anim + lean_offset + spike_base * std::cos(angle);
            const float sy = ey_anim + spike_base * std::sin(angle);
            const float tx = ex_anim + lean_offset + spike_tip * std::cos(angle);
            const float ty = ey_anim + spike_tip * std::sin(angle);
            draw_list->AddLine(ImVec2(sx, sy), ImVec2(tx, ty), detail_col, 3.0f);
        }
        
        // Draw eyes (two small circles)
        const float eye_offset_x = radius * 0.3f;
        const float eye_offset_y = radius * 0.2f;
        const float eye_radius = radius * 0.15f;
        draw_list->AddCircleFilled(ImVec2(ex_anim + lean_offset - eye_offset_x, ey_anim - eye_offset_y), 
                                  eye_radius, eye_col);
        draw_list->AddCircleFilled(ImVec2(ex_anim + lean_offset + eye_offset_x, ey_anim - eye_offset_y), 
                                  eye_radius, eye_col);
        
        // Draw mouth (small arc or line)
        const float mouth_width = radius * 0.4f;
        const float mouth_y = ey_anim + radius * 0.15f;
        draw_list->AddLine(ImVec2(ex_anim + lean_offset - mouth_width * 0.5f, mouth_y),
                          ImVec2(ex_anim + lean_offset + mouth_width * 0.5f, mouth_y),
                          border_col, 2.0f);
        
        const char text_str[2] = { 'E', '\0' };
        const ImVec2 text_sz = ImGui::CalcTextSize(text_str);
        draw_list->AddText(ImVec2(ex_anim + lean_offset - text_sz.x * 0.5f, ey_anim - text_sz.y * 0.5f),
                           ImColor(0.95f, 0.85f, 0.85f, 1.0f), text_str);

        // Draw HP indicator (only when not at max HP)
        if(enemy.hp < enemy.max_hp){
            const float bar_w = lg_game.cell_width * 0.5f;
            const float bar_h = 5.0f;
            const float bar_x = ex_anim + lean_offset - bar_w * 0.5f;
            const float bar_y = ey_anim - radius - 12.0f;
            const float hp_ratio = static_cast<float>(enemy.hp / enemy.max_hp);
            draw_list->AddRectFilled(ImVec2(bar_x, bar_y),
                                     ImVec2(bar_x + bar_w, bar_y + bar_h),
                                     ImColor(0.2f, 0.2f, 0.2f, 1.0f));
            draw_list->AddRectFilled(ImVec2(bar_x, bar_y),
                                     ImVec2(bar_x + bar_w * hp_ratio, bar_y + bar_h),
                                     ImColor(0.85f, 0.2f, 0.2f, 1.0f));
        }
    }

    // Draw projectiles.
    for(const auto& proj : lg_projectiles){
        const float px = board_origin.x + static_cast<float>(proj.x);
        const float py = board_origin.y + static_cast<float>(lane_y_center(proj.lane));
        draw_list->AddCircleFilled(ImVec2(px, py), 4.0f, ImColor(0.95f, 0.9f, 0.3f, 1.0f));
    }

    // Draw tokens.
    for(const auto& token : lg_tokens){
        const float tx = board_origin.x + static_cast<float>(token.pos.x);
        const float ty = board_origin.y + static_cast<float>(token.pos.y);
        draw_list->AddCircleFilled(ImVec2(tx, ty), static_cast<float>(token.radius),
                                   ImColor(0.95f, 0.8f, 0.1f, 1.0f));
        draw_list->AddCircle(ImVec2(tx, ty), static_cast<float>(token.radius),
                             ImColor(0.8f, 0.6f, 0.1f, 1.0f), 0, 2.0f);
        const char text_str[2] = { '+', '\0' };
        const ImVec2 text_sz = ImGui::CalcTextSize(text_str);
        draw_list->AddText(ImVec2(tx - text_sz.x * 0.5f, ty - text_sz.y * 0.5f),
                           ImColor(0.1f, 0.05f, 0.0f, 1.0f), text_str);
    }

    // Capture interactions on the board.
    ImGui::SetCursorScreenPos(board_origin);
    ImGui::InvisibleButton("lg_board", ImVec2(lg_game.board_width, lg_game.board_height));
    ImGui::End();

    return true;
}

