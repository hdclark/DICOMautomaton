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
    
    // Reset cardboard box state
    box_available = true;
    box_active = false;
    box_timer = 0.0;
    box_anim_timer = 0.0;
    box_state = box_state_t::Inactive;

    GenerateMaze();
    PlaceEnemies();
    PlacePlayer();
    
    player_sneaking = false;
    
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
        
        // Generate a patrol path by selecting random waypoints and using BFS to connect them
        enemy.patrol_path.clear();
        enemy.patrol_path.push_back(enemy.pos);
        
        // Select 3-6 random waypoint targets
        std::uniform_int_distribution<int64_t> num_waypoints_dist(3, 6);
        int64_t num_waypoints = num_waypoints_dist(ts_game.re);

        // Collect walkable cells that are at least 2 cells away from current path endpoints
        vec2<double> last_pos = enemy.pos;
        for(int64_t p = 0; p < num_waypoints; ++p){
            std::vector<std::pair<int64_t, int64_t>> candidates;
            int64_t last_gx = static_cast<int64_t>(last_pos.x / ts_game.cell_size);
            int64_t last_gy = static_cast<int64_t>(last_pos.y / ts_game.cell_size);

            // Look for cells within 2-4 cells distance
            for(int64_t dy = -4; dy <= 4; ++dy){
                for(int64_t dx = -4; dx <= 4; ++dx){
                    if(std::abs(dx) < 2 && std::abs(dy) < 2) continue; // Too close
                    int64_t nx = last_gx + dx;
                    int64_t ny = last_gy + dy;
                    if(nx > 0 && nx < ts_game.grid_cols - 1 &&
                       ny > 0 && ny < ts_game.grid_rows - 1 &&
                       !ts_game.walls[ny * ts_game.grid_cols + nx]){
                        vec2<double> cand((nx + 0.5) * ts_game.cell_size, (ny + 0.5) * ts_game.cell_size);
                        // Check not too close to existing path waypoints
                        bool too_close = false;
                        for(const auto& pp : enemy.patrol_path){
                            if(cand.distance(pp) < ts_game.cell_size * 1.5){
                                too_close = true;
                                break;
                            }
                        }
                        if(!too_close){
                            candidates.emplace_back(nx, ny);
                        }
                    }
                }
            }
            
            if(!candidates.empty()){
                std::uniform_int_distribution<size_t> cand_dist(0, candidates.size() - 1);
                auto [target_gx, target_gy] = candidates[cand_dist(ts_game.re)];
                vec2<double> target_pos((target_gx + 0.5) * ts_game.cell_size, (target_gy + 0.5) * ts_game.cell_size);
                
                // Use BFS to find path from last_pos to target_pos
                auto segment = FindPath(last_pos, target_pos);
                for(const auto& pt : segment){
                    // Avoid duplicating the start point
                    if(enemy.patrol_path.empty() || pt.distance(enemy.patrol_path.back()) > ts_game.cell_size * 0.5){
                        enemy.patrol_path.push_back(pt);
                    }
                }
                last_pos = target_pos;
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

bool TacticalStealthGame::IsBlocked(double x, double y, double radius) const {
    // Check if a circular object at (x, y) with given radius is blocked by walls or bounds
    if(IsWall(x - radius, y - radius) ||
       IsWall(x + radius, y - radius) ||
       IsWall(x - radius, y + radius) ||
       IsWall(x + radius, y + radius) ||
       IsWall(x, y)){
        return true;
    }
    // Keep within bounds
    if(x < radius || x > ts_game.box_width - radius ||
       y < radius || y > ts_game.box_height - radius){
        return true;
    }
    return false;
}

vec2<double> TacticalStealthGame::TryMoveWithSlide(const vec2<double> &pos, const vec2<double> &desired_move, double radius) const {
    // Try to move, and if blocked, slide along walls by trying each axis separately
    vec2<double> new_pos = pos + desired_move;

    if(!IsBlocked(new_pos.x, new_pos.y, radius)){
        // Full movement possible
        return new_pos;
    }

    // Try moving only in X direction (drop Y component)
    vec2<double> x_only = pos + vec2<double>(desired_move.x, 0.0);
    bool x_ok = !IsBlocked(x_only.x, x_only.y, radius);

    // Try moving only in Y direction (drop X component)
    vec2<double> y_only = pos + vec2<double>(0.0, desired_move.y);
    bool y_ok = !IsBlocked(y_only.x, y_only.y, radius);

    if(x_ok && !y_ok){
        return x_only;  // Slide along X axis
    }else if(y_ok && !x_ok){
        return y_only;  // Slide along Y axis
    }else if(x_ok && y_ok){
        // Both work, pick the one that moves us closer to intended position
        double x_dist = (new_pos - x_only).length();
        double y_dist = (new_pos - y_only).length();
        return (x_dist < y_dist) ? x_only : y_only;
    }

    // Neither axis works, stay in place
    return pos;
}
std::vector<vec2<double>> TacticalStealthGame::FindPath(const vec2<double> &from, const vec2<double> &to) const {
    // BFS pathfinding from one position to another through walkable cells
    std::vector<vec2<double>> path;

    int64_t from_gx = static_cast<int64_t>(from.x / ts_game.cell_size);
    int64_t from_gy = static_cast<int64_t>(from.y / ts_game.cell_size);
    int64_t to_gx = static_cast<int64_t>(to.x / ts_game.cell_size);
    int64_t to_gy = static_cast<int64_t>(to.y / ts_game.cell_size);

    // Clamp to valid grid
    from_gx = std::clamp<int64_t>(from_gx, 0, ts_game.grid_cols - 1);
    from_gy = std::clamp<int64_t>(from_gy, 0, ts_game.grid_rows - 1);
    to_gx = std::clamp<int64_t>(to_gx, 0, ts_game.grid_cols - 1);
    to_gy = std::clamp<int64_t>(to_gy, 0, ts_game.grid_rows - 1);

    // If same cell, return just the destination
    if(from_gx == to_gx && from_gy == to_gy){
        path.push_back(to);
        return path;
    }

    // BFS
    std::vector<int64_t> parent(ts_game.grid_cols * ts_game.grid_rows, -1);
    std::deque<std::pair<int64_t, int64_t>> queue;
    queue.emplace_back(from_gx, from_gy);
    parent[from_gy * ts_game.grid_cols + from_gx] = from_gy * ts_game.grid_cols + from_gx;  // Mark as visited

    const std::vector<std::pair<int64_t, int64_t>> directions = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

    bool found = false;
    while(!queue.empty() && !found){
        auto [cx, cy] = queue.front();
        queue.pop_front();

        for(const auto& [dx, dy] : directions){
            int64_t nx = cx + dx;
            int64_t ny = cy + dy;

            if(nx < 0 || nx >= ts_game.grid_cols || ny < 0 || ny >= ts_game.grid_rows){
                continue;
            }

            int64_t nidx = ny * ts_game.grid_cols + nx;
            if(parent[nidx] != -1){
                continue;  // Already visited
            }
            if(ts_game.walls[nidx]){
                continue;  // Wall
            }

            parent[nidx] = cy * ts_game.grid_cols + cx;
            queue.emplace_back(nx, ny);

            if(nx == to_gx && ny == to_gy){
                found = true;
                break;
            }
        }
    }

    if(!found){
        // No path found, return direct line (will be handled by wall sliding)
        path.push_back(to);
        return path;
    }

    // Reconstruct path from destination to source
    std::vector<std::pair<int64_t, int64_t>> grid_path;
    int64_t cidx = to_gy * ts_game.grid_cols + to_gx;
    while(cidx != from_gy * ts_game.grid_cols + from_gx){
        int64_t cx = cidx % ts_game.grid_cols;
        int64_t cy = cidx / ts_game.grid_cols;
        grid_path.emplace_back(cx, cy);
        cidx = parent[cidx];
    }

    // Reverse to get from->to order
    std::reverse(grid_path.begin(), grid_path.end());

    // Convert grid positions to world coordinates (center of cells)
    for(const auto& [gx, gy] : grid_path){
        path.emplace_back((gx + 0.5) * ts_game.cell_size, (gy + 0.5) * ts_game.cell_size);
    }

    return path;
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
        
        // Handle cardboard box mechanic
        if(f && ImGui::IsKeyPressed(SDL_SCANCODE_B) && box_available && box_state == box_state_t::Inactive){
            // Start donning the box
            box_state = box_state_t::Donning;
            box_anim_timer = box_anim_duration;
            box_available = false;
        }
        
        // Update cardboard box state machine
        bool box_blocks_movement = false;
        bool box_blocks_detection = false;
        switch(box_state){
            case box_state_t::Inactive:
                // Normal state
                break;
            case box_state_t::Donning:
                box_anim_timer -= dt;
                box_blocks_movement = true;
                if(box_anim_timer <= 0.0){
                    box_state = box_state_t::Active;
                    box_timer = box_duration;
                }
                break;
            case box_state_t::Active:
                box_timer -= dt;
                box_blocks_movement = true;
                box_blocks_detection = true;
                box_active = true;
                if(box_timer <= 0.0){
                    box_state = box_state_t::Doffing;
                    box_anim_timer = box_anim_duration;
                }
                break;
            case box_state_t::Doffing:
                box_anim_timer -= dt;
                box_blocks_movement = true;
                if(box_anim_timer <= 0.0){
                    box_state = box_state_t::Inactive;
                    box_active = false;
                }
                break;
        }
        
        // Update hide timer, but only if all enemies are patrolling normally and not in cardboard box.
        bool all_enemies_normal = true;
        for(const auto &enemy : enemies){
            if( (enemy.state != ts_enemy_state_t::Patrolling)
            &&  (enemy.state != ts_enemy_state_t::Returning) ){
                all_enemies_normal = false;
            }
        }
        // Hide timer stops while in cardboard box (box_blocks_detection is true when active)
        if(all_enemies_normal && !box_blocks_detection){
            ts_game.current_hide_timer += dt;
        }
        if(ts_game.current_hide_timer >= hide_time){
            ts_game.level_complete = true;
            ts_game.score += ts_game.level;
            ts_game.level_complete_timer = 0.0;
        }
   
        bool player_moving = (input_dir.length() > 0.01) && !box_blocks_movement;

        if(player_moving){
            input_dir = input_dir.unit();
            vec2<double> desired_move = input_dir * player_speed * dt;
            double r = ts_game.player_radius * 0.8;

            // Use wall sliding for smooth movement
            vec2<double> new_pos = TryMoveWithSlide(player_pos, desired_move, r);            
            
            if(new_pos.distance(player_pos) > 0.01){
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
            
            // Check if can see player (blocked by cardboard box)
            bool can_see_player = !box_blocks_detection && IsInFieldOfView(enemy.pos, enemy.facing, fov_angle, fov_range, player_pos);

            // Check if can hear player (moving, not sneaking, within hearing range; blocked by cardboard box)
            double hearing_dist = ts_game.hearing_range * level_mult;
            bool can_hear_player = !box_blocks_detection && player_moving && !player_sneaking && 
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
                    
                    // Check if caught player (blocked by cardboard box)
                    if(!box_blocks_detection){
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
                vec2<double> dir = target_pos - enemy.pos;
//                if(dir.length() > 1.0){
                if(dir.length() > 0.1){
                    dir = dir.unit();
                    vec2<double> desired_move = dir * current_speed * dt;
                    double r = ts_game.enemy_size * 0.4;
                    
                    // Use wall sliding for smooth movement
                    vec2<double> new_pos = TryMoveWithSlide(enemy.pos, desired_move, r);
                    
                    if(new_pos.distance(enemy.pos) > 0.01){
                        // Update facing direction based on actual movement
                        vec2<double> actual_dir = new_pos - enemy.pos;
                        if(actual_dir.length() > 0.001){
                            enemy.facing = std::atan2(actual_dir.y, actual_dir.x);
                        }
                        enemy.pos = new_pos;
                        enemy.walk_anim += dt * 12.0;
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
            
            // Reset cardboard box for new round
            box_available = true;
            box_active = false;
            box_timer = 0.0;
            box_anim_timer = 0.0;
            box_state = box_state_t::Inactive;
            
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
    
    // Draw player (circle) and cardboard box if active
    {
        ImVec2 player_screen_pos(curr_pos.x + player_pos.x, curr_pos.y + player_pos.y);
        
        // Check if we should draw the cardboard box
        bool draw_box = (box_state == box_state_t::Active || 
                        box_state == box_state_t::Donning || 
                        box_state == box_state_t::Doffing);
        
        if(draw_box){
            // Calculate animation progress for donning/doffing
            double anim_progress = 1.0;
            if(box_state == box_state_t::Donning){
                anim_progress = 1.0 - (box_anim_timer / box_anim_duration);  // 0 to 1 as donning
            }else if(box_state == box_state_t::Doffing){
                anim_progress = box_anim_timer / box_anim_duration;  // 1 to 0 as doffing
            }
            
            // Cardboard color (tan/brown)
            ImU32 cardboard_color = ImColor(0.82f, 0.68f, 0.47f, 1.0f);
            ImU32 cardboard_dark = ImColor(0.65f, 0.50f, 0.35f, 1.0f);
            ImU32 cardboard_line = ImColor(0.45f, 0.35f, 0.25f, 1.0f);
            
            double box_size = ts_game.player_radius * 2.0;
            double box_y_offset = (1.0 - anim_progress) * box_size * 1.5;  // Box comes down from above
            
            // Draw the main box body
            ImVec2 box_tl(player_screen_pos.x - box_size, player_screen_pos.y - box_size + box_y_offset);
            ImVec2 box_br(player_screen_pos.x + box_size, player_screen_pos.y + box_size + box_y_offset);
            window_draw_list->AddRectFilled(box_tl, box_br, cardboard_color);
            window_draw_list->AddRect(box_tl, box_br, cardboard_line, 0.0f, 0, 1.5f);
            
            // Draw horizontal tape/seam line
            double seam_y = player_screen_pos.y + box_y_offset;
            window_draw_list->AddLine(
                ImVec2(box_tl.x, seam_y), 
                ImVec2(box_br.x, seam_y), 
                cardboard_dark, 1.5f);
            
            // Draw the four open flaps on top
            double flap_height = box_size * 0.6 * anim_progress;  // Flaps open during animation
            double flap_angle = anim_progress * 0.5;  // Flaps tilt outward
            
            // Left flap
            {
                ImVec2 flap_base_l(box_tl.x, box_tl.y);
                ImVec2 flap_base_r(player_screen_pos.x - box_size * 0.1, box_tl.y);
                ImVec2 flap_top_l(flap_base_l.x - flap_height * flap_angle, flap_base_l.y - flap_height);
                ImVec2 flap_top_r(flap_base_r.x - flap_height * flap_angle * 0.3, flap_base_r.y - flap_height);
                window_draw_list->AddQuadFilled(flap_base_l, flap_base_r, flap_top_r, flap_top_l, cardboard_dark);
                window_draw_list->AddQuad(flap_base_l, flap_base_r, flap_top_r, flap_top_l, cardboard_line);
            }
            // Right flap
            {
                ImVec2 flap_base_l(player_screen_pos.x + box_size * 0.1, box_tl.y);
                ImVec2 flap_base_r(box_br.x, box_tl.y);
                ImVec2 flap_top_l(flap_base_l.x + flap_height * flap_angle * 0.3, flap_base_l.y - flap_height);
                ImVec2 flap_top_r(flap_base_r.x + flap_height * flap_angle, flap_base_r.y - flap_height);
                window_draw_list->AddQuadFilled(flap_base_l, flap_base_r, flap_top_r, flap_top_l, cardboard_dark);
                window_draw_list->AddQuad(flap_base_l, flap_base_r, flap_top_r, flap_top_l, cardboard_line);
            }
            // Front flap (smaller, partially visible)
            {
                ImVec2 flap_base_l(box_tl.x + box_size * 0.2, box_tl.y);
                ImVec2 flap_base_r(box_br.x - box_size * 0.2, box_tl.y);
                ImVec2 flap_top_l(flap_base_l.x, flap_base_l.y - flap_height * 0.7);
                ImVec2 flap_top_r(flap_base_r.x, flap_base_r.y - flap_height * 0.7);
                window_draw_list->AddQuadFilled(flap_base_l, flap_base_r, flap_top_r, flap_top_l, cardboard_color);
                window_draw_list->AddQuad(flap_base_l, flap_base_r, flap_top_r, flap_top_l, cardboard_line);
            }
            // Back flap (behind, slightly taller)
            {
                double back_flap_height = flap_height * 0.9;
                ImVec2 flap_base_l(box_tl.x + box_size * 0.15, box_tl.y);
                ImVec2 flap_base_r(box_br.x - box_size * 0.15, box_tl.y);
                ImVec2 flap_top_l(flap_base_l.x, flap_base_l.y - back_flap_height);
                ImVec2 flap_top_r(flap_base_r.x, flap_base_r.y - back_flap_height);
                window_draw_list->AddQuadFilled(flap_base_l, flap_base_r, flap_top_r, flap_top_l, cardboard_dark);
                window_draw_list->AddQuad(flap_base_l, flap_base_r, flap_top_r, flap_top_l, cardboard_line);
            }
            
            // If player is partially visible (during animation), draw them underneath
            if(anim_progress < 0.8){
                double player_alpha = 1.0 - anim_progress;
                double draw_radius = ts_game.player_radius;
                ImU32 player_color = ImColor(0.3f, 0.8f, 1.0f, static_cast<float>(player_alpha));
                window_draw_list->AddCircleFilled(player_screen_pos, draw_radius, player_color);
            }
        }else{
            // Normal player drawing
            // Walking animation - subtle size pulsing
            double anim_offset = std::sin(ts_game.walk_anim) * 1.5;
            double draw_radius = ts_game.player_radius + (player_sneaking ? 0 : anim_offset);
            
            ImU32 player_color = player_sneaking ? ImColor(0.3f, 0.7f, 0.3f, 0.8f) 
                                                 : ImColor(0.3f, 0.8f, 1.0f, 1.0f);
            window_draw_list->AddCircleFilled(player_screen_pos, draw_radius, player_color);
            window_draw_list->AddCircle(player_screen_pos, draw_radius, ImColor(1.0f, 1.0f, 1.0f, 0.5f));
        }
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
    
    // Check for mouse hover over enemies and display patrol path + tooltip
    {
        ImVec2 mouse_pos = ImGui::GetMousePos();
        for(size_t ei = 0; ei < enemies.size(); ++ei){
            const auto& enemy = enemies[ei];
            ImVec2 enemy_screen_pos(curr_pos.x + enemy.pos.x, curr_pos.y + enemy.pos.y);
            double size = ts_game.enemy_size;
            
            // Check if mouse is within enemy bounding box (approximately)
            double dx = mouse_pos.x - enemy_screen_pos.x;
            double dy = mouse_pos.y - enemy_screen_pos.y;
            double dist = std::hypot(dx, dy);
            
            if(dist < size * 2.0){
                // Draw patrol path
                if(enemy.patrol_path.size() >= 2){
                    for(size_t pi = 0; pi + 1 < enemy.patrol_path.size(); ++pi){
                        ImVec2 p1(curr_pos.x + enemy.patrol_path[pi].x, curr_pos.y + enemy.patrol_path[pi].y);
                        ImVec2 p2(curr_pos.x + enemy.patrol_path[pi + 1].x, curr_pos.y + enemy.patrol_path[pi + 1].y);
                        window_draw_list->AddLine(p1, p2, ImColor(0.5f, 0.5f, 1.0f, 0.7f), 2.0f);
                    }
                    // Draw waypoints
                    for(size_t pi = 0; pi < enemy.patrol_path.size(); ++pi){
                        ImVec2 wp(curr_pos.x + enemy.patrol_path[pi].x, curr_pos.y + enemy.patrol_path[pi].y);
                        ImU32 wp_color = (pi == enemy.patrol_idx) ? ImColor(0.0f, 1.0f, 0.0f, 0.8f) : ImColor(0.5f, 0.5f, 1.0f, 0.5f);
                        window_draw_list->AddCircleFilled(wp, 4.0f, wp_color);
                    }
                }
                
                // Draw tooltip with enemy status
                std::stringstream tooltip_ss;
                tooltip_ss << "Enemy " << (ei + 1) << "\n";
                tooltip_ss << "State: ";
                switch(enemy.state){
                    case ts_enemy_state_t::Patrolling: tooltip_ss << "Patrolling"; break;
                    case ts_enemy_state_t::Alerted: tooltip_ss << "Alerted"; break;
                    case ts_enemy_state_t::Pursuing: tooltip_ss << "Pursuing"; break;
                    case ts_enemy_state_t::Returning: tooltip_ss << "Returning"; break;
                }
                tooltip_ss << "\n";
                tooltip_ss << std::fixed << std::setprecision(1);
                tooltip_ss << "Look Timer: " << enemy.look_timer << "s\n";
                tooltip_ss << "Alert Timer: " << enemy.alert_timer << "s\n";
                tooltip_ss << "Pursuit Timer: " << enemy.pursuit_timer << "s";
                
                ImGui::SetTooltip("%s", tooltip_ss.str().c_str());
                break; // Only show tooltip for one enemy at a time
            }
        }
    }
  

    // Draw HUD
    {
        std::stringstream ss;
        ss << "Level: " << ts_game.level << "  Score: " << ts_game.score;
        // Add box status indicator
        if(box_state == box_state_t::Active){
            ss << "  [BOX: " << std::fixed << std::setprecision(1) << box_timer << "s]";
        }else if(box_available){
            ss << "  [BOX: Ready]";
        }else{
            ss << "  [BOX: Used]";
        }
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
        const char* instructions = "Arrow keys: move | Space: sneak | B: box | R: reset";
        auto text_size = ImGui::CalcTextSize(instructions);
        ImVec2 inst_pos(curr_pos.x + ts_game.box_width/2.0 - text_size.x/2.0,
                        curr_pos.y + ts_game.box_height - 30);
        window_draw_list->AddText(inst_pos, ImColor(0.7f, 0.7f, 0.7f, 1.0f), instructions);
    }
    
    ImGui::Dummy(ImVec2(ts_game.box_width, ts_game.box_height + 50));
    ImGui::End();
    return true;
}

