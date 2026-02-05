// TacticalStealth.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <vector>
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

#include "TacticalStealth.h"

namespace {
    constexpr double pi = 3.14159265358979323846;
}

TacticalStealthGame::TacticalStealthGame(){
    ts_game.re.seed( std::random_device()() );
    Reset();
}

void TacticalStealthGame::Reset(){
    enemies.clear();
    
    ts_game.level = 1;
    ts_game.score = 0;
    ts_game.current_hide_timer = 0.0;
    ts_game.level_complete_timer = 0.0;
    ts_game.game_over = false;
    ts_game.level_complete = false;
    ts_game.countdown_active = true;
    ts_game.countdown_remaining = 3.0;
    
    GenerateMaze();
    PlaceEnemies();
    PlacePlayer();
    
    player_vel = vec2<double>(0.0, 0.0);
    player_sneaking = false;
    
    const auto t_now = std::chrono::steady_clock::now();
    t_ts_updated = t_now;
    t_ts_started = t_now;
    
    return;
}

void TacticalStealthGame::GenerateMaze(){
    // Initialize grid with all walls
    ts_game.walls.resize(ts_game.grid_cols * ts_game.grid_rows, true);
    // Reset all cells to walls (in case of replay with existing vector)
    for(auto& w : ts_game.walls) w = true;
    
    // Ensure minimum grid dimensions for maze generation
    if(ts_game.grid_cols < 5 || ts_game.grid_rows < 5){
        // Grid too small for proper maze, just clear center
        for(int64_t y = 1; y < ts_game.grid_rows - 1; ++y){
            for(int64_t x = 1; x < ts_game.grid_cols - 1; ++x){
                ts_game.walls[y * ts_game.grid_cols + x] = false;
            }
        }
        return;
    }
    
    // Use recursive backtracking to generate a maze
    std::vector<bool> visited(ts_game.grid_cols * ts_game.grid_rows, false);
    std::vector<std::pair<int64_t, int64_t>> stack;
    
    // Start from a random odd cell (to leave room for walls)
    std::uniform_int_distribution<int64_t> col_dist(1, std::max<int64_t>(1, (ts_game.grid_cols - 2) / 2));
    std::uniform_int_distribution<int64_t> row_dist(1, std::max<int64_t>(1, (ts_game.grid_rows - 2) / 2));
    
    int64_t start_col = col_dist(ts_game.re) * 2 - 1;
    int64_t start_row = row_dist(ts_game.re) * 2 - 1;
    
    // Clamp to valid range
    start_col = std::clamp<int64_t>(start_col, 1, ts_game.grid_cols - 2);
    start_row = std::clamp<int64_t>(start_row, 1, ts_game.grid_rows - 2);
    
    stack.emplace_back(start_col, start_row);
    visited[start_row * ts_game.grid_cols + start_col] = true;
    ts_game.walls[start_row * ts_game.grid_cols + start_col] = false;
    
    const std::vector<std::pair<int64_t, int64_t>> directions = {{0, -2}, {0, 2}, {-2, 0}, {2, 0}};
    
    while(!stack.empty()){
        auto [cx, cy] = stack.back();
        
        // Find unvisited neighbors
        std::vector<std::pair<int64_t, int64_t>> neighbors;
        for(const auto& [dx, dy] : directions){
            int64_t nx = cx + dx;
            int64_t ny = cy + dy;
            if(nx > 0 && nx < ts_game.grid_cols - 1 &&
               ny > 0 && ny < ts_game.grid_rows - 1 &&
               !visited[ny * ts_game.grid_cols + nx]){
                neighbors.emplace_back(nx, ny);
            }
        }
        
        if(neighbors.empty()){
            stack.pop_back();
        }else{
            // Choose random neighbor
            std::uniform_int_distribution<size_t> neighbor_dist(0, neighbors.size() - 1);
            auto [nx, ny] = neighbors[neighbor_dist(ts_game.re)];
            
            // Remove wall between current and neighbor
            int64_t wx = (cx + nx) / 2;
            int64_t wy = (cy + ny) / 2;
            ts_game.walls[wy * ts_game.grid_cols + wx] = false;
            ts_game.walls[ny * ts_game.grid_cols + nx] = false;
            
            visited[ny * ts_game.grid_cols + nx] = true;
            stack.emplace_back(nx, ny);
        }
    }
    
    // Add some extra passages to make the maze less linear
    std::uniform_real_distribution<double> rd(0.0, 1.0);
    for(int64_t y = 1; y < ts_game.grid_rows - 1; ++y){
        for(int64_t x = 1; x < ts_game.grid_cols - 1; ++x){
            if(ts_game.walls[y * ts_game.grid_cols + x] && rd(ts_game.re) < 0.15){
                // Count adjacent walkable cells
                int walkable_neighbors = 0;
                if(x > 0 && !ts_game.walls[y * ts_game.grid_cols + (x-1)]) walkable_neighbors++;
                if(x < ts_game.grid_cols-1 && !ts_game.walls[y * ts_game.grid_cols + (x+1)]) walkable_neighbors++;
                if(y > 0 && !ts_game.walls[(y-1) * ts_game.grid_cols + x]) walkable_neighbors++;
                if(y < ts_game.grid_rows-1 && !ts_game.walls[(y+1) * ts_game.grid_cols + x]) walkable_neighbors++;
                
                // Only remove if it would create a loop (2+ neighbors)
                if(walkable_neighbors >= 2){
                    ts_game.walls[y * ts_game.grid_cols + x] = false;
                }
            }
        }
    }
}

void TacticalStealthGame::PlaceEnemies(){
    enemies.clear();
    
    const int64_t num_enemies = ts_game.base_enemies + (ts_game.level - 1);
    
    // Collect all walkable cells
    std::vector<std::pair<int64_t, int64_t>> walkable_cells;
    for(int64_t y = 1; y < ts_game.grid_rows - 1; ++y){
        for(int64_t x = 1; x < ts_game.grid_cols - 1; ++x){
            if(!ts_game.walls[y * ts_game.grid_cols + x]){
                walkable_cells.emplace_back(x, y);
            }
        }
    }
    
    if(walkable_cells.size() < static_cast<size_t>(num_enemies + 1)){
        return; // Not enough space
    }
    
    std::uniform_int_distribution<size_t> cell_dist(0, walkable_cells.size() - 1);
    std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * pi);
    
    std::vector<size_t> used_cells;
    
    for(int64_t i = 0; i < num_enemies && !walkable_cells.empty(); ++i){
        // Find a cell not too close to already placed enemies
        size_t attempts = 0;
        size_t cell_idx;
        bool found = false;
        
        while(attempts < 100 && !found){
            cell_idx = cell_dist(ts_game.re) % walkable_cells.size();
            found = true;
            for(size_t used_idx : used_cells){
                auto [ux, uy] = walkable_cells[used_idx];
                auto [cx, cy] = walkable_cells[cell_idx];
                double dist = std::hypot(static_cast<double>(cx - ux), static_cast<double>(cy - uy));
                if(dist < 3.0){ // Minimum 3 cells apart
                    found = false;
                    break;
                }
            }
            attempts++;
        }
        
        if(!found) continue;
        
        used_cells.push_back(cell_idx);
        auto [cx, cy] = walkable_cells[cell_idx];
        
        ts_enemy_t enemy;
        enemy.pos.x = (cx + 0.5) * ts_game.cell_size;
        enemy.pos.y = (cy + 0.5) * ts_game.cell_size;
        enemy.facing = angle_dist(ts_game.re);
        enemy.state = ts_enemy_state_t::Patrolling;
        enemy.patrol_idx = 0;
        enemy.patrol_forward = true;
        enemy.walk_anim = 0.0;
        
        // Generate a patrol path using BFS to find nearby walkable cells
        enemy.patrol_path.clear();
        enemy.patrol_path.push_back(enemy.pos);
        
        // Simple patrol: find 3-6 random nearby walkable cells
        std::uniform_int_distribution<int64_t> path_len_dist(3, 6);
        int64_t path_len = path_len_dist(ts_game.re);
        
        vec2<double> last_pos = enemy.pos;
        for(int64_t p = 0; p < path_len; ++p){
            // Find walkable neighbors of current position
            std::vector<vec2<double>> candidates;
            int64_t last_gx = static_cast<int64_t>(last_pos.x / ts_game.cell_size);
            int64_t last_gy = static_cast<int64_t>(last_pos.y / ts_game.cell_size);
            
            for(int64_t dy = -2; dy <= 2; ++dy){
                for(int64_t dx = -2; dx <= 2; ++dx){
                    if(dx == 0 && dy == 0) continue;
                    int64_t nx = last_gx + dx;
                    int64_t ny = last_gy + dy;
                    if(nx > 0 && nx < ts_game.grid_cols - 1 &&
                       ny > 0 && ny < ts_game.grid_rows - 1 &&
                       !ts_game.walls[ny * ts_game.grid_cols + nx]){
                        vec2<double> cand((nx + 0.5) * ts_game.cell_size, (ny + 0.5) * ts_game.cell_size);
                        // Check not too close to existing path points
                        bool too_close = false;
                        for(const auto& pp : enemy.patrol_path){
                            if(cand.distance(pp) < ts_game.cell_size * 0.5){
                                too_close = true;
                                break;
                            }
                        }
                        if(!too_close){
                            candidates.push_back(cand);
                        }
                    }
                }
            }
            
            if(!candidates.empty()){
                std::uniform_int_distribution<size_t> cand_dist(0, candidates.size() - 1);
                last_pos = candidates[cand_dist(ts_game.re)];
                enemy.patrol_path.push_back(last_pos);
            }
        }
        
        // Initialize look timer
        std::uniform_real_distribution<double> look_dist(ts_game.look_interval_min, ts_game.look_interval_max);
        enemy.look_timer = look_dist(ts_game.re);
        
        enemies.push_back(enemy);
    }
}

void TacticalStealthGame::PlacePlayer(){
    // Find a walkable cell not in any enemy's field of view
    std::vector<std::pair<int64_t, int64_t>> walkable_cells;
    for(int64_t y = 1; y < ts_game.grid_rows - 1; ++y){
        for(int64_t x = 1; x < ts_game.grid_cols - 1; ++x){
            if(!ts_game.walls[y * ts_game.grid_cols + x]){
                walkable_cells.emplace_back(x, y);
            }
        }
    }
    
    // Calculate current level's FOV parameters
    double level_mult = 1.0 + (ts_game.level - 1) * ts_game.fov_scale_per_level;
    double fov_angle = ts_game.base_fov_angle * level_mult;
    double fov_range = ts_game.base_fov_range * level_mult;
    
    std::shuffle(walkable_cells.begin(), walkable_cells.end(), ts_game.re);
    
    for(const auto& [cx, cy] : walkable_cells){
        vec2<double> candidate((cx + 0.5) * ts_game.cell_size, (cy + 0.5) * ts_game.cell_size);
        
        bool in_fov = false;
        for(const auto& enemy : enemies){
            if(IsInFieldOfView(enemy.pos, enemy.facing, fov_angle, fov_range, candidate)){
                in_fov = true;
                break;
            }
            // Also ensure not too close to any enemy
            if(candidate.distance(enemy.pos) < ts_game.cell_size * 3){
                in_fov = true;
                break;
            }
        }
        
        if(!in_fov){
            player_pos = candidate;
            return;
        }
    }
    
    // Fallback: just place in first walkable cell
    if(!walkable_cells.empty()){
        auto [cx, cy] = walkable_cells[0];
        player_pos.x = (cx + 0.5) * ts_game.cell_size;
        player_pos.y = (cy + 0.5) * ts_game.cell_size;
    }
}

bool TacticalStealthGame::IsWall(double x, double y) const {
    int64_t gx = static_cast<int64_t>(x / ts_game.cell_size);
    int64_t gy = static_cast<int64_t>(y / ts_game.cell_size);
    
    if(gx < 0 || gx >= ts_game.grid_cols || gy < 0 || gy >= ts_game.grid_rows){
        return true;
    }
    
    return ts_game.walls[gy * ts_game.grid_cols + gx];
}

bool TacticalStealthGame::LineOfSight(const vec2<double> &from, const vec2<double> &to) const {
    // Simple ray-march to check for walls
    vec2<double> dir = to - from;
    double dist = dir.length();
    if(dist < 0.001) return true;
    
    dir = dir.unit();
    double step = ts_game.cell_size * 0.3;
    
    for(double t = step; t < dist; t += step){
        vec2<double> point = from + dir * t;
        if(IsWall(point.x, point.y)){
            return false;
        }
    }
    return true;
}

bool TacticalStealthGame::IsInFieldOfView(const vec2<double> &enemy_pos, double enemy_facing, 
                                          double fov_angle, double fov_range, 
                                          const vec2<double> &target) const {
    vec2<double> to_target = target - enemy_pos;
    double dist = to_target.length();
    
    if(dist > fov_range) return false;
    if(dist < 0.001) return true;
    
    double angle_to_target = std::atan2(to_target.y, to_target.x);
    double angle_diff = angle_to_target - enemy_facing;
    
    // Normalize angle difference to [-pi, pi]
    while(angle_diff > pi) angle_diff -= 2.0 * pi;
    while(angle_diff < -pi) angle_diff += 2.0 * pi;
    
    if(std::abs(angle_diff) > fov_angle) return false;
    
    // Check line of sight (no walls in between)
    return LineOfSight(enemy_pos, target);
}

bool TacticalStealthGame::Display(bool &enabled){
    if( !enabled ) return true;
    
    const auto win_width  = static_cast<int>( ts_game.box_width ) + 20;
    const auto win_height = static_cast<int>( ts_game.box_height ) + 80;
    auto flags = ImGuiWindowFlags_AlwaysAutoResize
               | ImGuiWindowFlags_NoScrollWithMouse
               | ImGuiWindowFlags_NoNavInputs
               | ImGuiWindowFlags_NoScrollbar;
    ImGui::SetNextWindowSize(ImVec2(win_width, win_height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::Begin("Tactical Stealth", &enabled, flags);
    
    const auto f = ImGui::IsWindowFocused();
    
    // Reset the game
    if( f && ImGui::IsKeyPressed(SDL_SCANCODE_R) ){
        Reset();
    }
    
    // Display state
    ImVec2 curr_pos = ImGui::GetCursorScreenPos();
    ImDrawList *window_draw_list = ImGui::GetWindowDrawList();
    
    // Calculate current level's parameters
    double level_mult = 1.0 + (ts_game.level - 1) * ts_game.speed_scale_per_level;
    double fov_mult = 1.0 + (ts_game.level - 1) * ts_game.fov_scale_per_level;
    double enemy_speed = ts_game.base_speed * level_mult;
    double fov_angle = ts_game.base_fov_angle * fov_mult;
    double fov_range = ts_game.base_fov_range * fov_mult;
    double hide_time = ts_game.hide_time_base + (ts_game.level - 1) * ts_game.hide_time_increment;
    
    // Draw border
    {
        const auto c = ImColor(0.3f, 0.3f, 0.4f, 1.0f);
        window_draw_list->AddRectFilled(curr_pos, 
            ImVec2(curr_pos.x + ts_game.box_width, curr_pos.y + ts_game.box_height), 
            ImColor(0.05f, 0.05f, 0.1f, 1.0f));
        window_draw_list->AddRect(curr_pos, 
            ImVec2(curr_pos.x + ts_game.box_width, curr_pos.y + ts_game.box_height), c);
    }
    
    // Time update
    const auto t_now = std::chrono::steady_clock::now();
    auto t_updated_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_ts_updated).count();
    if(t_updated_diff > 50) t_updated_diff = 50;
    const auto dt = static_cast<double>(t_updated_diff) / 1000.0;
    t_ts_updated = t_now;
    
    // Draw maze walls
    for(int64_t y = 0; y < ts_game.grid_rows; ++y){
        for(int64_t x = 0; x < ts_game.grid_cols; ++x){
            if(ts_game.walls[y * ts_game.grid_cols + x]){
                ImVec2 p1(curr_pos.x + x * ts_game.cell_size, 
                          curr_pos.y + y * ts_game.cell_size);
                ImVec2 p2(curr_pos.x + (x + 1) * ts_game.cell_size, 
                          curr_pos.y + (y + 1) * ts_game.cell_size);
                window_draw_list->AddRectFilled(p1, p2, ImColor(0.2f, 0.2f, 0.25f, 1.0f));
            }
        }
    }
    
    // Handle countdown
    if(ts_game.countdown_active){
        ts_game.countdown_remaining -= dt;
        if(ts_game.countdown_remaining <= 0.0){
            ts_game.countdown_active = false;
            ts_game.countdown_remaining = 0.0;
        }
        
        // Draw countdown
        const auto countdown_text = std::to_string(static_cast<int>(std::ceil(ts_game.countdown_remaining)));
        const auto text_size = ImGui::CalcTextSize(countdown_text.c_str());
        const ImVec2 text_pos(curr_pos.x + ts_game.box_width/2.0 - text_size.x/2.0,
                              curr_pos.y + ts_game.box_height/2.0 - text_size.y/2.0);
        window_draw_list->AddText(text_pos, ImColor(1.0f, 1.0f, 1.0f, 1.0f), countdown_text.c_str());
    }
    
    // Game logic (only when not in countdown and not game over)
    if(!ts_game.countdown_active && !ts_game.game_over && !ts_game.level_complete){
        // Update hide timer
        ts_game.current_hide_timer += dt;
        if(ts_game.current_hide_timer >= hide_time){
            ts_game.level_complete = true;
            ts_game.score += ts_game.level;
            ts_game.level_complete_timer = 0.0;
        }
        
        // Player input
        player_sneaking = f && ImGui::IsKeyDown(SDL_SCANCODE_SPACE);
        double player_speed = ts_game.base_speed * (player_sneaking ? ts_game.quiet_speed_mult : 1.0);
        
        vec2<double> input_dir(0.0, 0.0);
        if( f ){
            if( ImGui::IsKeyDown(SDL_SCANCODE_LEFT) || ImGui::IsKeyDown(SDL_SCANCODE_A) ){
                input_dir.x -= 1.0;
            }
            if( ImGui::IsKeyDown(SDL_SCANCODE_RIGHT) || ImGui::IsKeyDown(SDL_SCANCODE_D) ){
                input_dir.x += 1.0;
            }
            if( ImGui::IsKeyDown(SDL_SCANCODE_UP) || ImGui::IsKeyDown(SDL_SCANCODE_W) ){
                input_dir.y -= 1.0;
            }
            if( ImGui::IsKeyDown(SDL_SCANCODE_DOWN) || ImGui::IsKeyDown(SDL_SCANCODE_S) ){
                input_dir.y += 1.0;
            }
        }
        
        bool player_moving = (input_dir.length() > 0.01);
        
        if(player_moving){
            input_dir = input_dir.unit();
            vec2<double> new_pos = player_pos + input_dir * player_speed * dt;
            
            // Collision check with walls
            bool blocked = false;
            // Check corners of player bounding box
            double r = ts_game.player_radius * 0.8;
            if(IsWall(new_pos.x - r, new_pos.y - r) ||
               IsWall(new_pos.x + r, new_pos.y - r) ||
               IsWall(new_pos.x - r, new_pos.y + r) ||
               IsWall(new_pos.x + r, new_pos.y + r) ||
               IsWall(new_pos.x, new_pos.y)){
                blocked = true;
            }
            
            // Keep within bounds
            if(new_pos.x < ts_game.player_radius || new_pos.x > ts_game.box_width - ts_game.player_radius ||
               new_pos.y < ts_game.player_radius || new_pos.y > ts_game.box_height - ts_game.player_radius){
                blocked = true;
            }
            
            if(!blocked){
                player_pos = new_pos;
                ts_game.walk_anim += dt * 15.0;
            }
        }
        
        // Update enemies
        std::uniform_real_distribution<double> look_dist(ts_game.look_interval_min, ts_game.look_interval_max);
        
        for(auto& enemy : enemies){
            // Update exclamation timer
            if(enemy.exclaim_timer > 0.0){
                enemy.exclaim_timer -= dt;
            }
            
            // Check if can see player
            bool can_see_player = IsInFieldOfView(enemy.pos, enemy.facing, fov_angle, fov_range, player_pos);
            
            // Check if can hear player (moving, not sneaking, within hearing range)
            double hearing_dist = ts_game.hearing_range * level_mult;
            bool can_hear_player = player_moving && !player_sneaking && 
                                   player_pos.distance(enemy.pos) < hearing_dist;
            
            // State transitions
            if(can_see_player){
                if(enemy.state != ts_enemy_state_t::Pursuing){
                    enemy.exclaim_timer = ts_game.exclaim_duration;
                }
                enemy.state = ts_enemy_state_t::Pursuing;
                enemy.pursuit_timer = 0.0;
                enemy.last_known_player_pos = player_pos;
                enemy.is_looking = false;
            }else if(can_hear_player && enemy.state == ts_enemy_state_t::Patrolling){
                enemy.state = ts_enemy_state_t::Alerted;
                enemy.alert_timer = ts_game.alert_duration;
                enemy.exclaim_timer = ts_game.exclaim_duration;
                enemy.last_known_player_pos = player_pos;
                enemy.is_looking = false;
            }
            
            // State behavior
            double current_speed = enemy_speed;
            vec2<double> target_pos = enemy.pos;
            
            switch(enemy.state){
                case ts_enemy_state_t::Patrolling:
                    // Look around occasionally
                    if(enemy.is_looking){
                        enemy.look_timer -= dt;
                        // Rotate while looking
                        enemy.facing += dt * 2.0;
                        if(enemy.look_timer <= 0.0){
                            enemy.is_looking = false;
                            enemy.look_timer = look_dist(ts_game.re);
                        }
                    }else{
                        enemy.look_timer -= dt;
                        if(enemy.look_timer <= 0.0){
                            enemy.is_looking = true;
                            enemy.look_timer = ts_game.look_duration;
                        }else{
                            // Follow patrol path
                            if(!enemy.patrol_path.empty()){
                                target_pos = enemy.patrol_path[enemy.patrol_idx];
                                
                                if(enemy.pos.distance(target_pos) < ts_game.cell_size * 0.3){
                                    // Reached waypoint, move to next
                                    if(enemy.patrol_forward){
                                        enemy.patrol_idx++;
                                        if(enemy.patrol_idx >= enemy.patrol_path.size()){
                                            enemy.patrol_idx = enemy.patrol_path.size() - 2;
                                            enemy.patrol_forward = false;
                                            if(enemy.patrol_path.size() <= 1){
                                                enemy.patrol_idx = 0;
                                            }
                                        }
                                    }else{
                                        if(enemy.patrol_idx == 0){
                                            enemy.patrol_forward = true;
                                        }else{
                                            enemy.patrol_idx--;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    break;
                    
                case ts_enemy_state_t::Alerted:
                    // Look around more actively
                    enemy.facing += dt * 3.0;
                    enemy.alert_timer -= dt;
                    
                    // Move toward last known position
                    if(enemy.pos.distance(enemy.last_known_player_pos) > ts_game.cell_size * 0.5){
                        target_pos = enemy.last_known_player_pos;
                    }
                    
                    if(enemy.alert_timer <= 0.0){
                        enemy.state = ts_enemy_state_t::Returning;
                    }
                    break;
                    
                case ts_enemy_state_t::Pursuing:
                    current_speed = enemy_speed * ts_game.pursuit_speed_mult;
                    target_pos = player_pos;
                    enemy.pursuit_timer += dt;
                    enemy.last_known_player_pos = player_pos;
                    
                    // Give up if can't see player for too long
                    if(!can_see_player){
                        if(enemy.pursuit_timer > ts_game.pursuit_timeout){
                            enemy.state = ts_enemy_state_t::Returning;
                        }
                    }else{
                        enemy.pursuit_timer = 0.0;
                    }
                    
                    // Check if caught player
                    double catch_dist = ts_game.player_radius * ts_game.catch_distance_mult * 2.0;
                    if(enemy.pos.distance(player_pos) < catch_dist){
                        ts_game.game_over = true;
                    }
                    break;
                    
                case ts_enemy_state_t::Returning:
                    // Return to patrol
                    if(!enemy.patrol_path.empty()){
                        target_pos = enemy.patrol_path[enemy.patrol_idx];
                        if(enemy.pos.distance(target_pos) < ts_game.cell_size * 0.3){
                            enemy.state = ts_enemy_state_t::Patrolling;
                            enemy.look_timer = look_dist(ts_game.re);
                        }
                    }else{
                        enemy.state = ts_enemy_state_t::Patrolling;
                    }
                    break;
            }
            
            // Move enemy toward target (if not looking around)
            if(!enemy.is_looking || enemy.state != ts_enemy_state_t::Patrolling){
                vec2<double> dir = target_pos - enemy.pos;
                if(dir.length() > 1.0){
                    dir = dir.unit();
                    vec2<double> new_pos = enemy.pos + dir * current_speed * dt;
                    
                    // Simple collision check
                    double r = ts_game.enemy_size * 0.4;
                    if(!IsWall(new_pos.x - r, new_pos.y - r) &&
                       !IsWall(new_pos.x + r, new_pos.y - r) &&
                       !IsWall(new_pos.x - r, new_pos.y + r) &&
                       !IsWall(new_pos.x + r, new_pos.y + r) &&
                       !IsWall(new_pos.x, new_pos.y)){
                        enemy.pos = new_pos;
                        enemy.walk_anim += dt * 12.0;
                        
                        // Update facing direction
                        enemy.facing = std::atan2(dir.y, dir.x);
                    }
                }
            }
        }
    }
    
    // Handle level complete
    if(ts_game.level_complete){
        ts_game.level_complete_timer += dt;
        if(ts_game.level_complete_timer > 2.0){
            // Next level
            ts_game.level++;
            ts_game.current_hide_timer = 0.0;
            ts_game.level_complete = false;
            ts_game.countdown_active = true;
            ts_game.countdown_remaining = 3.0;
            
            // Regenerate maze and enemies for new level
            GenerateMaze();
            PlaceEnemies();
            PlacePlayer();
        }
    }
    
    // Draw enemy FOV cones (draw first so they appear behind enemies)
    for(const auto& enemy : enemies){
        float alpha = 0.15f;
        ImU32 fov_color = ImColor(1.0f, 1.0f, 0.5f, alpha);
        
        if(enemy.state == ts_enemy_state_t::Pursuing){
            fov_color = ImColor(1.0f, 0.3f, 0.3f, alpha * 2);
        }else if(enemy.state == ts_enemy_state_t::Alerted){
            fov_color = ImColor(1.0f, 0.8f, 0.3f, alpha * 1.5f);
        }
        
        // Draw FOV as a filled triangle/cone
        ImVec2 enemy_screen_pos(curr_pos.x + enemy.pos.x, curr_pos.y + enemy.pos.y);
        
        const int segments = 12;
        for(int i = 0; i < segments; ++i){
            double a1 = enemy.facing - fov_angle + (2.0 * fov_angle * i / segments);
            double a2 = enemy.facing - fov_angle + (2.0 * fov_angle * (i + 1) / segments);
            
            ImVec2 p1 = enemy_screen_pos;
            ImVec2 p2(enemy_screen_pos.x + std::cos(a1) * fov_range,
                      enemy_screen_pos.y + std::sin(a1) * fov_range);
            ImVec2 p3(enemy_screen_pos.x + std::cos(a2) * fov_range,
                      enemy_screen_pos.y + std::sin(a2) * fov_range);
            
            window_draw_list->AddTriangleFilled(p1, p2, p3, fov_color);
        }
    }
    
    // Draw player (circle)
    {
        ImVec2 player_screen_pos(curr_pos.x + player_pos.x, curr_pos.y + player_pos.y);
        
        // Walking animation - subtle size pulsing
        double anim_offset = std::sin(ts_game.walk_anim) * 1.5;
        double draw_radius = ts_game.player_radius + (player_sneaking ? 0 : anim_offset);
        
        ImU32 player_color = player_sneaking ? ImColor(0.3f, 0.7f, 0.3f, 0.8f) 
                                              : ImColor(0.3f, 0.8f, 1.0f, 1.0f);
        window_draw_list->AddCircleFilled(player_screen_pos, draw_radius, player_color);
        window_draw_list->AddCircle(player_screen_pos, draw_radius, ImColor(1.0f, 1.0f, 1.0f, 0.5f));
    }
    
    // Draw enemies (squares)
    for(const auto& enemy : enemies){
        ImVec2 enemy_screen_pos(curr_pos.x + enemy.pos.x, curr_pos.y + enemy.pos.y);
        
        // Walking animation - bobbing
        double bob_offset = std::sin(enemy.walk_anim) * 2.0;
        double size = ts_game.enemy_size;
        
        ImU32 enemy_color = ImColor(0.9f, 0.2f, 0.2f, 1.0f);
        if(enemy.state == ts_enemy_state_t::Pursuing){
            enemy_color = ImColor(1.0f, 0.0f, 0.0f, 1.0f);
        }else if(enemy.state == ts_enemy_state_t::Alerted){
            enemy_color = ImColor(1.0f, 0.5f, 0.0f, 1.0f);
        }else if(enemy.state == ts_enemy_state_t::Returning){
            enemy_color = ImColor(0.7f, 0.4f, 0.4f, 1.0f);
        }
        
        // Draw square with rotation
        double c = std::cos(enemy.facing);
        double s = std::sin(enemy.facing);
        
        auto rotate_point = [&](double lx, double ly) -> ImVec2 {
            return ImVec2(enemy_screen_pos.x + c * lx - s * ly,
                         enemy_screen_pos.y + bob_offset + s * lx + c * ly);
        };
        
        ImVec2 corners[4] = {
            rotate_point(-size, -size),
            rotate_point(size, -size),
            rotate_point(size, size),
            rotate_point(-size, size)
        };
        
        window_draw_list->AddQuadFilled(corners[0], corners[1], corners[2], corners[3], enemy_color);
        window_draw_list->AddQuad(corners[0], corners[1], corners[2], corners[3], ImColor(0.0f, 0.0f, 0.0f, 0.5f));
        
        // Draw exclamation mark if detected player
        if(enemy.exclaim_timer > 0.0){
            float exclaim_alpha = std::min(1.0f, static_cast<float>(enemy.exclaim_timer / 0.3f));
            ImVec2 exclaim_pos(enemy_screen_pos.x, enemy_screen_pos.y - size * 2.5);
            window_draw_list->AddText(exclaim_pos, ImColor(1.0f, 1.0f, 0.0f, exclaim_alpha), "!");
        }
    }
    
    // Draw HUD
    {
        std::stringstream ss;
        ss << "Level: " << ts_game.level << "  Score: " << ts_game.score;
        const ImVec2 level_pos(curr_pos.x + 10, curr_pos.y + ts_game.box_height + 5);
        window_draw_list->AddText(level_pos, ImColor(1.0f, 1.0f, 1.0f, 1.0f), ss.str().c_str());
        
        // Hide timer bar
        double hide_time_total = ts_game.hide_time_base + (ts_game.level - 1) * ts_game.hide_time_increment;
        double progress = std::min(1.0, ts_game.current_hide_timer / hide_time_total);
        
        ImVec2 bar_start(curr_pos.x + 10, curr_pos.y + ts_game.box_height + 25);
        ImVec2 bar_end(curr_pos.x + ts_game.box_width - 10, curr_pos.y + ts_game.box_height + 40);
        ImVec2 bar_fill(bar_start.x + (bar_end.x - bar_start.x) * progress, bar_end.y);
        
        window_draw_list->AddRectFilled(bar_start, bar_end, ImColor(0.2f, 0.2f, 0.2f, 1.0f));
        window_draw_list->AddRectFilled(bar_start, bar_fill, ImColor(0.2f, 0.8f, 0.2f, 1.0f));
        window_draw_list->AddRect(bar_start, bar_end, ImColor(0.5f, 0.5f, 0.5f, 1.0f));
        
        // Timer text
        std::stringstream timer_ss;
        timer_ss << "Hide: " << std::fixed << std::setprecision(1) << ts_game.current_hide_timer 
                 << " / " << hide_time_total << "s";
        ImVec2 timer_text_size = ImGui::CalcTextSize(timer_ss.str().c_str());
        ImVec2 timer_pos((bar_start.x + bar_end.x) / 2 - timer_text_size.x / 2, bar_start.y + 2);
        window_draw_list->AddText(timer_pos, ImColor(1.0f, 1.0f, 1.0f, 1.0f), timer_ss.str().c_str());
    }
    
    // Draw level complete message
    if(ts_game.level_complete){
        const char* text = "LEVEL COMPLETE!";
        auto text_size = ImGui::CalcTextSize(text);
        ImVec2 text_pos(curr_pos.x + ts_game.box_width/2.0 - text_size.x/2.0,
                        curr_pos.y + ts_game.box_height/2.0 - text_size.y/2.0);
        window_draw_list->AddText(text_pos, ImColor(0.0f, 1.0f, 0.0f, 1.0f), text);
    }
    
    // Draw game over message
    if(ts_game.game_over){
        const char* text = "GAME OVER! Press R to reset";
        auto text_size = ImGui::CalcTextSize(text);
        ImVec2 text_pos(curr_pos.x + ts_game.box_width/2.0 - text_size.x/2.0,
                        curr_pos.y + ts_game.box_height/2.0 - text_size.y/2.0);
        window_draw_list->AddText(text_pos, ImColor(1.0f, 0.0f, 0.0f, 1.0f), text);
    }
    
    // Draw instructions
    if(ts_game.countdown_active){
        const char* instructions = "Arrow keys: move | Space: sneak | R: reset";
        auto text_size = ImGui::CalcTextSize(instructions);
        ImVec2 inst_pos(curr_pos.x + ts_game.box_width/2.0 - text_size.x/2.0,
                        curr_pos.y + ts_game.box_height - 30);
        window_draw_list->AddText(inst_pos, ImColor(0.7f, 0.7f, 0.7f, 1.0f), instructions);
    }
    
    ImGui::Dummy(ImVec2(ts_game.box_width, ts_game.box_height + 50));
    ImGui::End();
    return true;
}

