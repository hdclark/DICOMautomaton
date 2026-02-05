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
    
    player_sneaking = false;
    player_in_box = false;
    box_used_this_round = false;
    box_timer = 0.0;
    box_anim_timer = 0.0;
    box_donning = false;
    
    const auto t_now = std::chrono::steady_clock::now();
    t_ts_updated = t_now;
    
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
            cell_idx = cell_dist(ts_game.re);
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
        
        // Generate patrol waypoints using random walk through adjacent open cells
        enemy.patrol_path.clear();
        enemy.patrol_path.push_back(enemy.pos);
        
        // Random walk generates 3-6 waypoints
        std::uniform_int_distribution<int64_t> waypoint_count_dist(3, 6);
        const int64_t desired_waypoints = waypoint_count_dist(ts_game.re);
        
        int64_t walk_col = cx;
        int64_t walk_row = cy;
        std::deque<std::pair<int64_t, int64_t>> trail;
        trail.push_back({walk_col, walk_row});
        
        // Step offsets for orthogonal movement only
        const int64_t step_offsets[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
        
        for(int64_t wp = 0; wp < desired_waypoints; ++wp){
            // Gather valid adjacent cells that are walkable
            std::vector<std::pair<int64_t, int64_t>> valid_steps;
            for(int k = 0; k < 4; ++k){
                const int64_t test_col = walk_col + step_offsets[k][0];
                const int64_t test_row = walk_row + step_offsets[k][1];
                
                // Bounds check and wall check
                if(test_col >= 1 && test_col < ts_game.grid_cols - 1 &&
                   test_row >= 1 && test_row < ts_game.grid_rows - 1){
                    const size_t cell_idx = static_cast<size_t>(test_row * ts_game.grid_cols + test_col);
                    if(!ts_game.walls[cell_idx]){
                        // Avoid recently visited cells if possible
                        bool in_trail = false;
                        for(const auto& tc : trail){
                            if(tc.first == test_col && tc.second == test_row){
                                in_trail = true;
                                break;
                            }
                        }
                        if(!in_trail){
                            valid_steps.push_back({test_col, test_row});
                        }
                    }
                }
            }
            
            // Fallback: if no fresh cells, accept any walkable neighbor
            if(valid_steps.empty()){
                for(int k = 0; k < 4; ++k){
                    const int64_t test_col = walk_col + step_offsets[k][0];
                    const int64_t test_row = walk_row + step_offsets[k][1];
                    if(test_col >= 1 && test_col < ts_game.grid_cols - 1 &&
                       test_row >= 1 && test_row < ts_game.grid_rows - 1){
                        const size_t cell_idx = static_cast<size_t>(test_row * ts_game.grid_cols + test_col);
                        if(!ts_game.walls[cell_idx]){
                            valid_steps.push_back({test_col, test_row});
                        }
                    }
                }
            }
            
            if(!valid_steps.empty()){
                std::uniform_int_distribution<size_t> step_picker(0, valid_steps.size() - 1);
                const auto& chosen = valid_steps[step_picker(ts_game.re)];
                walk_col = chosen.first;
                walk_row = chosen.second;
                trail.push_back({walk_col, walk_row});
                // Keep trail short to allow some revisiting
                if(trail.size() > 4) trail.pop_front();
                
                const double wp_x = (static_cast<double>(walk_col) + 0.5) * ts_game.cell_size;
                const double wp_y = (static_cast<double>(walk_row) + 0.5) * ts_game.cell_size;
                enemy.patrol_path.push_back(vec2<double>(wp_x, wp_y));
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
        // Update hide timer, but only if all enemies are patrolling normally and player is not in box.
        bool all_enemies_normal = true;
        for(const auto &enemy : enemies){
            if( (enemy.state != ts_enemy_state_t::Patrolling)
            &&  (enemy.state != ts_enemy_state_t::Returning) ){
                all_enemies_normal = false;
            }
        }
        if(all_enemies_normal && !player_in_box){
            ts_game.current_hide_timer += dt;
        }
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
        
        if(player_moving && !player_in_box){
            input_dir = input_dir.unit();
            const double delta_x = input_dir.x * player_speed * dt;
            const double delta_y = input_dir.y * player_speed * dt;
            
            // Collision radius for player bounding
            const double collision_r = ts_game.player_radius * 0.8;
            
            // Lambda to test if a position is blocked by walls or boundaries
            auto is_blocked = [&](double px, double py) -> bool {
                // Test center and four edge points
                if(IsWall(px, py)) return true;
                if(IsWall(px - collision_r, py - collision_r)) return true;
                if(IsWall(px + collision_r, py - collision_r)) return true;
                if(IsWall(px - collision_r, py + collision_r)) return true;
                if(IsWall(px + collision_r, py + collision_r)) return true;
                // Boundary checks
                if(px < ts_game.player_radius) return true;
                if(px > ts_game.box_width - ts_game.player_radius) return true;
                if(py < ts_game.player_radius) return true;
                if(py > ts_game.box_height - ts_game.player_radius) return true;
                return false;
            };
            
            // Attempt full diagonal move first
            const double target_x = player_pos.x + delta_x;
            const double target_y = player_pos.y + delta_y;
            
            if(!is_blocked(target_x, target_y)){
                player_pos.x = target_x;
                player_pos.y = target_y;
                ts_game.walk_anim += dt * 15.0;
            }else{
                // Wall sliding: decompose motion into components and try each
                bool did_slide = false;
                
                // Attempt X-axis slide if there's horizontal intent
                if(std::abs(delta_x) > 1e-6){
                    const double slide_x = player_pos.x + delta_x;
                    if(!is_blocked(slide_x, player_pos.y)){
                        player_pos.x = slide_x;
                        did_slide = true;
                    }
                }
                
                // Attempt Y-axis slide if there's vertical intent
                if(std::abs(delta_y) > 1e-6){
                    const double slide_y = player_pos.y + delta_y;
                    if(!is_blocked(player_pos.x, slide_y)){
                        player_pos.y = slide_y;
                        did_slide = true;
                    }
                }
                
                if(did_slide){
                    ts_game.walk_anim += dt * 15.0;
                }
            }
        }
        
        // Cardboard box mechanic - press 'B' to hide
        if(f && ImGui::IsKeyPressed(SDL_SCANCODE_B) && !box_used_this_round && !player_in_box && !box_donning){
            box_donning = true;
            box_anim_timer = ts_game.box_anim_duration;
        }
        
        // Handle box animation
        if(box_anim_timer > 0.0){
            box_anim_timer -= dt;
            if(box_anim_timer <= 0.0){
                box_anim_timer = 0.0;
                if(box_donning){
                    // Finished donning, now in box
                    player_in_box = true;
                    box_timer = ts_game.box_duration;
                }else{
                    // Finished doffing
                    box_used_this_round = true;
                }
            }
        }
        
        // Handle box timer
        if(player_in_box && box_anim_timer <= 0.0){
            box_timer -= dt;
            if(box_timer <= 0.0){
                box_timer = 0.0;
                player_in_box = false;
                box_donning = false;
                box_anim_timer = ts_game.box_anim_duration;  // Start doff animation
            }
        }
        
        // Update enemies
        std::uniform_real_distribution<double> look_dist(ts_game.look_interval_min, ts_game.look_interval_max);
        
        for(auto& enemy : enemies){
            // Update exclamation timer
            if(enemy.exclaim_timer > 0.0){
                enemy.exclaim_timer -= dt;
            }
            
            // Check if can see player (not possible when player is in cardboard box)
            bool can_see_player = !player_in_box && 
                                  IsInFieldOfView(enemy.pos, enemy.facing, fov_angle, fov_range, player_pos);
            
            // Check if can hear player (moving, not sneaking, within hearing range, not in box)
            double hearing_dist = ts_game.hearing_range * level_mult;
            bool can_hear_player = !player_in_box && player_moving && !player_sneaking && 
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
                                    if(enemy.patrol_path.size() <= 1){
                                        // With zero or one waypoint, just stay at the only valid index.
                                        enemy.patrol_idx = 0;
                                    }else if(enemy.patrol_path.size() == 2){
                                        // Special handling for exactly two waypoints: simply toggle between 0 and 1.
                                        enemy.patrol_idx = (enemy.patrol_idx == 0) ? 1 : 0;
                                    }else if(enemy.patrol_forward){
                                        // Moving forward along a path with at least two waypoints.
                                        if(enemy.patrol_idx + 1 >= enemy.patrol_path.size()){
                                            // Bounce back from the end: step to the previous waypoint and reverse.
                                            enemy.patrol_idx = enemy.patrol_path.size() - 2;
                                            enemy.patrol_forward = false;
                                        }else{
                                            enemy.patrol_idx++;
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
                    
                    // Check if caught player (can't catch when player is in box)
                    if(!player_in_box){
                        double catch_dist = ts_game.player_radius * ts_game.catch_distance_mult * 2.0;
                        if(enemy.pos.distance(player_pos) < catch_dist){
                            ts_game.game_over = true;
                        }
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
                vec2<double> direction_to_target = target_pos - enemy.pos;
                const double dist_to_target = direction_to_target.length();
                
                if(dist_to_target > 1.0){
                    direction_to_target = direction_to_target.unit();
                    const double step_x = direction_to_target.x * current_speed * dt;
                    const double step_y = direction_to_target.y * current_speed * dt;
                    
                    // Enemy collision half-size
                    const double half_sz = ts_game.enemy_size * 0.4;
                    
                    // Test function for wall collision at given position
                    auto hits_wall = [&](double ex, double ey) -> bool {
                        if(IsWall(ex, ey)) return true;
                        if(IsWall(ex - half_sz, ey - half_sz)) return true;
                        if(IsWall(ex + half_sz, ey - half_sz)) return true;
                        if(IsWall(ex - half_sz, ey + half_sz)) return true;
                        if(IsWall(ex + half_sz, ey + half_sz)) return true;
                        return false;
                    };
                    
                    double result_x = enemy.pos.x;
                    double result_y = enemy.pos.y;
                    double face_x = direction_to_target.x;
                    double face_y = direction_to_target.y;
                    bool advanced = false;
                    
                    // Try combined movement first
                    const double combined_x = enemy.pos.x + step_x;
                    const double combined_y = enemy.pos.y + step_y;
                    
                    if(!hits_wall(combined_x, combined_y)){
                        result_x = combined_x;
                        result_y = combined_y;
                        advanced = true;
                    }else{
                        // Decompose: try X component alone
                        if(std::abs(step_x) > 1e-6 && !hits_wall(enemy.pos.x + step_x, enemy.pos.y)){
                            result_x = enemy.pos.x + step_x;
                            face_x = (step_x > 0) ? 1.0 : -1.0;
                            face_y = 0.0;
                            advanced = true;
                        }
                        // Try Y component alone
                        if(std::abs(step_y) > 1e-6 && !hits_wall(result_x, enemy.pos.y + step_y)){
                            result_y = enemy.pos.y + step_y;
                            if(!advanced){
                                face_x = 0.0;
                                face_y = (step_y > 0) ? 1.0 : -1.0;
                            }
                            advanced = true;
                        }
                    }
                    
                    if(advanced){
                        enemy.pos.x = result_x;
                        enemy.pos.y = result_y;
                        enemy.walk_anim += dt * 12.0;
                        enemy.facing = std::atan2(face_y, face_x);
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
    
    // Draw player (circle) or cardboard box hiding spot
    {
        const float scr_x = static_cast<float>(curr_pos.x + player_pos.x);
        const float scr_y = static_cast<float>(curr_pos.y + player_pos.y);
        
        // Determine if cardboard concealment should be rendered
        const bool render_concealment = player_in_box || box_anim_timer > 0.0;
        
        if(render_concealment){
            // Compute animation interpolation (0=open, 1=closed)
            double interp = 1.0;
            if(box_anim_timer > 0.0){
                const double normalized_time = 1.0 - (box_anim_timer / ts_game.box_anim_duration);
                interp = box_donning ? normalized_time : (1.0 - normalized_time);
            }
            
            // Kraft paper tones for the container
            const ImU32 kraft_main = ImColor(0.72f, 0.58f, 0.40f, 1.0f);
            const ImU32 kraft_shadow = ImColor(0.55f, 0.44f, 0.30f, 1.0f);
            const ImU32 kraft_edge = ImColor(0.40f, 0.32f, 0.22f, 1.0f);
            
            const float container_extent = static_cast<float>(ts_game.player_radius * 2.2);
            const float h = container_extent * 0.5f;
            
            // Container body (main square)
            window_draw_list->AddRectFilled(
                ImVec2(scr_x - h, scr_y - h),
                ImVec2(scr_x + h, scr_y + h),
                kraft_main);
            window_draw_list->AddRect(
                ImVec2(scr_x - h, scr_y - h),
                ImVec2(scr_x + h, scr_y + h),
                kraft_edge, 0.0f, 0, 1.5f);
            
            // Vertical fold lines on container face
            window_draw_list->AddLine(
                ImVec2(scr_x - h * 0.35f, scr_y - h * 0.6f),
                ImVec2(scr_x - h * 0.35f, scr_y + h * 0.6f),
                kraft_shadow, 1.0f);
            window_draw_list->AddLine(
                ImVec2(scr_x + h * 0.35f, scr_y - h * 0.6f),
                ImVec2(scr_x + h * 0.35f, scr_y + h * 0.6f),
                kraft_shadow, 1.0f);
            
            // Four lid flaps that animate open/closed
            const float lid_len = h * 0.55f;
            const float swing = static_cast<float>((1.0 - interp) * (pi / 2.8));
            const float lid_offset = lid_len * std::sin(swing);
            const float lid_narrow = 0.65f;
            
            // North lid flap
            {
                const float tip_y = scr_y - h - lid_offset;
                window_draw_list->AddTriangleFilled(
                    ImVec2(scr_x - h, scr_y - h),
                    ImVec2(scr_x + h, scr_y - h),
                    ImVec2(scr_x, tip_y),
                    kraft_shadow);
                window_draw_list->AddTriangle(
                    ImVec2(scr_x - h, scr_y - h),
                    ImVec2(scr_x + h, scr_y - h),
                    ImVec2(scr_x, tip_y),
                    kraft_edge);
            }
            // South lid flap
            {
                const float tip_y = scr_y + h + lid_offset;
                window_draw_list->AddTriangleFilled(
                    ImVec2(scr_x - h, scr_y + h),
                    ImVec2(scr_x + h, scr_y + h),
                    ImVec2(scr_x, tip_y),
                    kraft_shadow);
                window_draw_list->AddTriangle(
                    ImVec2(scr_x - h, scr_y + h),
                    ImVec2(scr_x + h, scr_y + h),
                    ImVec2(scr_x, tip_y),
                    kraft_edge);
            }
            // West lid flap
            {
                const float tip_x = scr_x - h - lid_offset;
                window_draw_list->AddTriangleFilled(
                    ImVec2(scr_x - h, scr_y - h * lid_narrow),
                    ImVec2(scr_x - h, scr_y + h * lid_narrow),
                    ImVec2(tip_x, scr_y),
                    kraft_main);
                window_draw_list->AddTriangle(
                    ImVec2(scr_x - h, scr_y - h * lid_narrow),
                    ImVec2(scr_x - h, scr_y + h * lid_narrow),
                    ImVec2(tip_x, scr_y),
                    kraft_edge);
            }
            // East lid flap
            {
                const float tip_x = scr_x + h + lid_offset;
                window_draw_list->AddTriangleFilled(
                    ImVec2(scr_x + h, scr_y - h * lid_narrow),
                    ImVec2(scr_x + h, scr_y + h * lid_narrow),
                    ImVec2(tip_x, scr_y),
                    kraft_main);
                window_draw_list->AddTriangle(
                    ImVec2(scr_x + h, scr_y - h * lid_narrow),
                    ImVec2(scr_x + h, scr_y + h * lid_narrow),
                    ImVec2(tip_x, scr_y),
                    kraft_edge);
            }
        }else{
            // Standard player circle rendering
            const double pulse = std::sin(ts_game.walk_anim) * 1.5;
            const float render_radius = static_cast<float>(ts_game.player_radius + (player_sneaking ? 0.0 : pulse));
            
            const ImU32 circle_fill = player_sneaking 
                ? ImColor(0.3f, 0.7f, 0.3f, 0.8f) 
                : ImColor(0.3f, 0.8f, 1.0f, 1.0f);
            window_draw_list->AddCircleFilled(ImVec2(scr_x, scr_y), render_radius, circle_fill);
            window_draw_list->AddCircle(ImVec2(scr_x, scr_y), render_radius, ImColor(1.0f, 1.0f, 1.0f, 0.5f));
        }
    }
    
    // Render guard entities and detect cursor proximity for info display
    const ImVec2 cursor_screen = ImGui::GetMousePos();
    const ts_enemy_t* focused_guard = nullptr;
    
    for(const auto& guard : enemies){
        const float gx = static_cast<float>(curr_pos.x + guard.pos.x);
        const float gy = static_cast<float>(curr_pos.y + guard.pos.y);
        
        // Animate vertical bobbing during movement
        const float vertical_bob = static_cast<float>(std::sin(guard.walk_anim) * 2.0);
        const float extent = static_cast<float>(ts_game.enemy_size);
        
        // Cursor proximity test for showing patrol route
        const float cursor_dist_sq = (cursor_screen.x - gx) * (cursor_screen.x - gx) + 
                                     (cursor_screen.y - gy) * (cursor_screen.y - gy);
        if(cursor_dist_sq < (extent * 2.0f) * (extent * 2.0f)){
            focused_guard = &guard;
        }
        
        // Select tint based on behavioral mode
        ImU32 guard_tint = ImColor(0.9f, 0.2f, 0.2f, 1.0f);
        if(guard.state == ts_enemy_state_t::Pursuing){
            guard_tint = ImColor(1.0f, 0.0f, 0.0f, 1.0f);
        }else if(guard.state == ts_enemy_state_t::Alerted){
            guard_tint = ImColor(1.0f, 0.5f, 0.0f, 1.0f);
        }else if(guard.state == ts_enemy_state_t::Returning){
            guard_tint = ImColor(0.7f, 0.4f, 0.4f, 1.0f);
        }
        
        // Rotated square rendering
        const float cos_f = static_cast<float>(std::cos(guard.facing));
        const float sin_f = static_cast<float>(std::sin(guard.facing));
        
        auto transform_vertex = [&](float lx, float ly) -> ImVec2 {
            return ImVec2(gx + cos_f * lx - sin_f * ly,
                         gy + vertical_bob + sin_f * lx + cos_f * ly);
        };
        
        const ImVec2 verts[4] = {
            transform_vertex(-extent, -extent),
            transform_vertex(extent, -extent),
            transform_vertex(extent, extent),
            transform_vertex(-extent, extent)
        };
        
        window_draw_list->AddQuadFilled(verts[0], verts[1], verts[2], verts[3], guard_tint);
        window_draw_list->AddQuad(verts[0], verts[1], verts[2], verts[3], ImColor(0.0f, 0.0f, 0.0f, 0.5f));
        
        // Alert indicator above guard
        if(guard.exclaim_timer > 0.0){
            const float fade = std::min(1.0f, static_cast<float>(guard.exclaim_timer / 0.3f));
            window_draw_list->AddText(
                ImVec2(gx - 2.0f, gy - extent * 2.5f), 
                ImColor(1.0f, 1.0f, 0.0f, fade), "!");
        }
    }
    
    // Overlay patrol route and stats popup when cursor is over a guard
    if(focused_guard != nullptr){
        // Render patrol route visualization
        const size_t route_len = focused_guard->patrol_path.size();
        if(route_len > 1){
            const ImU32 route_tint = ImColor(0.4f, 0.6f, 1.0f, 0.55f);
            for(size_t seg = 0; seg + 1 < route_len; ++seg){
                const ImVec2 seg_start(
                    static_cast<float>(curr_pos.x + focused_guard->patrol_path[seg].x),
                    static_cast<float>(curr_pos.y + focused_guard->patrol_path[seg].y));
                const ImVec2 seg_end(
                    static_cast<float>(curr_pos.x + focused_guard->patrol_path[seg + 1].x),
                    static_cast<float>(curr_pos.y + focused_guard->patrol_path[seg + 1].y));
                window_draw_list->AddLine(seg_start, seg_end, route_tint, 2.0f);
            }
            // Waypoint dots
            for(size_t wp_idx = 0; wp_idx < route_len; ++wp_idx){
                const ImVec2 wp_pos(
                    static_cast<float>(curr_pos.x + focused_guard->patrol_path[wp_idx].x),
                    static_cast<float>(curr_pos.y + focused_guard->patrol_path[wp_idx].y));
                const bool is_current = (wp_idx == focused_guard->patrol_idx);
                const ImU32 dot_tint = is_current ? ImColor(1.0f, 0.9f, 0.2f, 0.85f) 
                                                   : ImColor(0.4f, 0.6f, 1.0f, 0.75f);
                window_draw_list->AddCircleFilled(wp_pos, 3.5f, dot_tint);
            }
        }
        
        // Information popup
        ImGui::BeginTooltip();
        
        const char* behavior_label = "Unknown";
        switch(focused_guard->state){
            case ts_enemy_state_t::Patrolling: behavior_label = "Patrolling"; break;
            case ts_enemy_state_t::Alerted: behavior_label = "Alerted"; break;
            case ts_enemy_state_t::Pursuing: behavior_label = "Pursuing"; break;
            case ts_enemy_state_t::Returning: behavior_label = "Returning"; break;
        }
        
        ImGui::Text("Status: %s", behavior_label);
        ImGui::Text("Look Timer: %.1f s", focused_guard->look_timer);
        ImGui::Text("Alert Timer: %.1f s", focused_guard->alert_timer);
        ImGui::Text("Pursuit Timer: %.1f s", focused_guard->pursuit_timer);
        
        ImGui::EndTooltip();
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
        const char* instructions = "Arrows: move | Space: sneak | B: hide in box | R: reset";
        auto text_size = ImGui::CalcTextSize(instructions);
        ImVec2 inst_pos(curr_pos.x + ts_game.box_width/2.0 - text_size.x/2.0,
                        curr_pos.y + ts_game.box_height - 30);
        window_draw_list->AddText(inst_pos, ImColor(0.7f, 0.7f, 0.7f, 1.0f), instructions);
    }
    
    ImGui::Dummy(ImVec2(ts_game.box_width, ts_game.box_height + 50));
    ImGui::End();
    return true;
}

