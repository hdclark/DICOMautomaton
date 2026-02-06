// MazeExplorer.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <vector>
#include <deque>
#include <random>
#include <chrono>
#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <iomanip>

#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"

#include <SDL.h>
#include "../imgui20210904/imgui.h"

#include "MazeExplorer.h"

namespace {
    constexpr double pi = 3.14159265358979323846;
}

MazeExplorerGame::MazeExplorerGame(){
    me_game.re.seed(std::random_device()());
    Reset();
}

void MazeExplorerGame::Reset(){
    me_game.level_complete = false;
    me_game.completion_time = 0.0;
    me_game.is_jumping = false;
    me_game.jump_height = 0.0;
    me_game.jump_velocity = 0.0;
    me_game.walk_phase = 0.0;

    player_floor = 0;
    player_angle = 0.0;

    GenerateLevel();

    player_pos = vec2<double>(start_cell.x + 0.5, start_cell.y + 0.5);

    const auto t_now = std::chrono::steady_clock::now();
    t_updated = t_now;
    t_started = t_now;
}

bool MazeExplorerGame::InBounds(int64_t x, int64_t y) const{
    return (x >= 0 && y >= 0 && x < me_game.grid_cols && y < me_game.grid_rows);
}

size_t MazeExplorerGame::CellIndex(int64_t x, int64_t y) const{
    return static_cast<size_t>(y * me_game.grid_cols + x);
}

size_t MazeExplorerGame::FlatIndex(const maze_coord_t &coord) const{
    return static_cast<size_t>(coord.floor * me_game.grid_cols * me_game.grid_rows
                               + coord.y * me_game.grid_cols + coord.x);
}

bool MazeExplorerGame::IsWall(int64_t floor_idx, int64_t x, int64_t y) const{
    if(floor_idx < 0 || floor_idx >= me_game.num_floors) return true;
    if(!InBounds(x, y)) return true;
    const auto &cells = floors.at(static_cast<size_t>(floor_idx)).cells;
    return cells.at(CellIndex(x, y)).wall;
}

bool MazeExplorerGame::IsStairUp(int64_t floor_idx, int64_t x, int64_t y) const{
    if(floor_idx < 0 || floor_idx >= me_game.num_floors) return false;
    if(!InBounds(x, y)) return false;
    return floors.at(static_cast<size_t>(floor_idx)).cells.at(CellIndex(x, y)).stair_up;
}

bool MazeExplorerGame::IsStairDown(int64_t floor_idx, int64_t x, int64_t y) const{
    if(floor_idx < 0 || floor_idx >= me_game.num_floors) return false;
    if(!InBounds(x, y)) return false;
    return floors.at(static_cast<size_t>(floor_idx)).cells.at(CellIndex(x, y)).stair_down;
}

MazeExplorerGame::maze_coord_t MazeExplorerGame::RandomWalkableCell(int64_t floor_idx){
    std::vector<maze_coord_t> cells;
    for(int64_t y = 1; y < me_game.grid_rows - 1; ++y){
        for(int64_t x = 1; x < me_game.grid_cols - 1; ++x){
            if(!IsWall(floor_idx, x, y)){
                cells.push_back({ floor_idx, x, y });
            }
        }
    }
    if(cells.empty()) return { floor_idx, 1, 1 };

    std::uniform_int_distribution<size_t> dist(0, cells.size() - 1);
    return cells[dist(me_game.re)];
}

std::vector<int64_t> MazeExplorerGame::ComputeDistances(const maze_coord_t &start) const{
    const auto total = static_cast<size_t>(me_game.num_floors * me_game.grid_cols * me_game.grid_rows);
    std::vector<int64_t> distances(total, -1);

    if(IsWall(start.floor, start.x, start.y)) return distances;

    std::deque<maze_coord_t> frontier;
    frontier.push_back(start);
    distances[FlatIndex(start)] = 0;

    while(!frontier.empty()){
        const auto node = frontier.front();
        frontier.pop_front();
        const auto node_dist = distances[FlatIndex(node)];

        const std::array<std::pair<int64_t, int64_t>, 4> dirs = {
            std::make_pair(1, 0), std::make_pair(-1, 0),
            std::make_pair(0, 1), std::make_pair(0, -1)
        };

        for(const auto &dir : dirs){
            const auto nx = node.x + dir.first;
            const auto ny = node.y + dir.second;
            if(IsWall(node.floor, nx, ny)) continue;
            maze_coord_t next{ node.floor, nx, ny };
            const auto flat = FlatIndex(next);
            if(distances[flat] >= 0) continue;
            distances[flat] = node_dist + 1;
            frontier.push_back(next);
        }

        if(IsStairUp(node.floor, node.x, node.y)){
            const auto nf = node.floor + 1;
            if(!IsWall(nf, node.x, node.y)){
                maze_coord_t next{ nf, node.x, node.y };
                const auto flat = FlatIndex(next);
                if(distances[flat] < 0){
                    distances[flat] = node_dist + 1;
                    frontier.push_back(next);
                }
            }
        }
        if(IsStairDown(node.floor, node.x, node.y)){
            const auto nf = node.floor - 1;
            if(!IsWall(nf, node.x, node.y)){
                maze_coord_t next{ nf, node.x, node.y };
                const auto flat = FlatIndex(next);
                if(distances[flat] < 0){
                    distances[flat] = node_dist + 1;
                    frontier.push_back(next);
                }
            }
        }
    }

    return distances;
}

void MazeExplorerGame::GenerateMazeFloor(int64_t floor_idx){
    auto &cells = floors.at(static_cast<size_t>(floor_idx)).cells;
    const auto total = static_cast<size_t>(me_game.grid_cols * me_game.grid_rows);
    cells.assign(total, {});
    for(auto &cell : cells){
        cell.wall = true;
        cell.stair_up = false;
        cell.stair_down = false;
    }

    if(me_game.grid_cols < 5 || me_game.grid_rows < 5){
        for(int64_t y = 1; y < me_game.grid_rows - 1; ++y){
            for(int64_t x = 1; x < me_game.grid_cols - 1; ++x){
                cells[CellIndex(x, y)].wall = false;
            }
        }
        return;
    }

    std::vector<bool> visited(total, false);
    std::vector<std::pair<int64_t, int64_t>> stack;

    std::uniform_int_distribution<int64_t> col_dist(1, std::max<int64_t>(1, (me_game.grid_cols - 2) / 2));
    std::uniform_int_distribution<int64_t> row_dist(1, std::max<int64_t>(1, (me_game.grid_rows - 2) / 2));

    int64_t start_col = col_dist(me_game.re) * 2 - 1;
    int64_t start_row = row_dist(me_game.re) * 2 - 1;
    start_col = std::clamp<int64_t>(start_col, 1, me_game.grid_cols - 2);
    start_row = std::clamp<int64_t>(start_row, 1, me_game.grid_rows - 2);

    stack.emplace_back(start_col, start_row);
    visited[CellIndex(start_col, start_row)] = true;
    cells[CellIndex(start_col, start_row)].wall = false;

    const std::vector<std::pair<int64_t, int64_t>> directions = {{0, -2}, {0, 2}, {-2, 0}, {2, 0}};

    while(!stack.empty()){
        auto [cx, cy] = stack.back();
        std::vector<std::pair<int64_t, int64_t>> neighbors;
        for(const auto &dir : directions){
            const auto nx = cx + dir.first;
            const auto ny = cy + dir.second;
            if(nx > 0 && nx < me_game.grid_cols - 1 && ny > 0 && ny < me_game.grid_rows - 1){
                const auto idx = CellIndex(nx, ny);
                if(!visited[idx]){
                    neighbors.emplace_back(nx, ny);
                }
            }
        }

        if(neighbors.empty()){
            stack.pop_back();
        }else{
            std::uniform_int_distribution<size_t> neighbor_dist(0, neighbors.size() - 1);
            auto [nx, ny] = neighbors[neighbor_dist(me_game.re)];

            const auto wx = (cx + nx) / 2;
            const auto wy = (cy + ny) / 2;
            cells[CellIndex(wx, wy)].wall = false;
            cells[CellIndex(nx, ny)].wall = false;

            visited[CellIndex(nx, ny)] = true;
            stack.emplace_back(nx, ny);
        }
    }

    std::uniform_real_distribution<double> rd(0.0, 1.0);
    for(int64_t y = 1; y < me_game.grid_rows - 1; ++y){
        for(int64_t x = 1; x < me_game.grid_cols - 1; ++x){
            const auto idx = CellIndex(x, y);
            if(!cells[idx].wall) continue;
            if(rd(me_game.re) < 0.12){
                int walkable_neighbors = 0;
                if(!cells[CellIndex(x - 1, y)].wall) ++walkable_neighbors;
                if(!cells[CellIndex(x + 1, y)].wall) ++walkable_neighbors;
                if(!cells[CellIndex(x, y - 1)].wall) ++walkable_neighbors;
                if(!cells[CellIndex(x, y + 1)].wall) ++walkable_neighbors;
                if(walkable_neighbors >= 2){
                    cells[idx].wall = false;
                }
            }
        }
    }
}

void MazeExplorerGame::GenerateLevel(){
    bool created = false;

    for(int attempt = 0; attempt < 40 && !created; ++attempt){
        floors.clear();
        floors.resize(static_cast<size_t>(me_game.num_floors));
        for(int64_t floor_idx = 0; floor_idx < me_game.num_floors; ++floor_idx){
            GenerateMazeFloor(floor_idx);
        }

        for(int64_t floor_idx = 0; floor_idx < me_game.num_floors - 1; ++floor_idx){
            std::vector<maze_coord_t> candidates;
            for(int64_t y = 1; y < me_game.grid_rows - 1; ++y){
                for(int64_t x = 1; x < me_game.grid_cols - 1; ++x){
                    if(!IsWall(floor_idx, x, y) && !IsWall(floor_idx + 1, x, y)){
                        candidates.push_back({ floor_idx, x, y });
                    }
                }
            }

            if(candidates.empty()){
                std::uniform_int_distribution<int64_t> col_dist(1, me_game.grid_cols - 2);
                std::uniform_int_distribution<int64_t> row_dist(1, me_game.grid_rows - 2);
                const auto x = col_dist(me_game.re);
                const auto y = row_dist(me_game.re);
                floors[static_cast<size_t>(floor_idx)].cells[CellIndex(x, y)].wall = false;
                floors[static_cast<size_t>(floor_idx + 1)].cells[CellIndex(x, y)].wall = false;
                candidates.push_back({ floor_idx, x, y });
            }

            std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
            const auto chosen = candidates[pick(me_game.re)];
            floors[static_cast<size_t>(floor_idx)].cells[CellIndex(chosen.x, chosen.y)].stair_up = true;
            floors[static_cast<size_t>(floor_idx + 1)].cells[CellIndex(chosen.x, chosen.y)].stair_down = true;
        }

        start_cell = RandomWalkableCell(0);
        auto distances = ComputeDistances(start_cell);

        const auto target_floor = me_game.num_floors - 1;
        int64_t max_dist = -1;
        std::vector<maze_coord_t> far_cells;
        for(int64_t y = 1; y < me_game.grid_rows - 1; ++y){
            for(int64_t x = 1; x < me_game.grid_cols - 1; ++x){
                maze_coord_t candidate{ target_floor, x, y };
                const auto dist = distances[FlatIndex(candidate)];
                if(dist < 0) continue;
                if(dist > max_dist){
                    far_cells.clear();
                    max_dist = dist;
                }
                if(dist == max_dist){
                    far_cells.push_back(candidate);
                }
            }
        }

        if(max_dist < 0 || far_cells.empty()){
            continue;
        }

        std::uniform_int_distribution<size_t> pick(0, far_cells.size() - 1);
        goal_cell = far_cells[pick(me_game.re)];
        created = true;
    }

    if(!created){
        floors.clear();
        floors.resize(static_cast<size_t>(me_game.num_floors));
        for(int64_t floor_idx = 0; floor_idx < me_game.num_floors; ++floor_idx){
            auto &cells = floors[static_cast<size_t>(floor_idx)].cells;
            cells.assign(static_cast<size_t>(me_game.grid_cols * me_game.grid_rows), {});
            for(auto &cell : cells){
                cell.wall = false;
                cell.stair_up = false;
                cell.stair_down = false;
            }
        }
        for(int64_t floor_idx = 0; floor_idx < me_game.num_floors - 1; ++floor_idx){
            floors[static_cast<size_t>(floor_idx)].cells[CellIndex(1, 1)].stair_up = true;
            floors[static_cast<size_t>(floor_idx + 1)].cells[CellIndex(1, 1)].stair_down = true;
        }
        start_cell = { 0, 1, 1 };
        goal_cell = { me_game.num_floors - 1, me_game.grid_cols - 2, me_game.grid_rows - 2 };
    }
}

double MazeExplorerGame::NormalizeAngle(double angle) const{
    const auto two_pi = 2.0 * pi;
    angle = std::fmod(angle, two_pi);
    if(angle < 0.0) angle += two_pi;
    return angle;
}

double MazeExplorerGame::AngleDelta(double angle) const{
    const auto two_pi = 2.0 * pi;
    angle = std::fmod(angle + pi, two_pi);
    if(angle < 0.0) angle += two_pi;
    return angle - pi;
}

double MazeExplorerGame::CastRay(const vec2<double> &pos, double angle, int64_t floor_idx, int &hit_side) const{
    const auto ray_dir_x = std::cos(angle);
    const auto ray_dir_y = std::sin(angle);

    int64_t map_x = static_cast<int64_t>(std::floor(pos.x));
    int64_t map_y = static_cast<int64_t>(std::floor(pos.y));

    const auto delta_dist_x = (std::abs(ray_dir_x) < 1.0e-8) ? 1.0e30 : std::abs(1.0 / ray_dir_x);
    const auto delta_dist_y = (std::abs(ray_dir_y) < 1.0e-8) ? 1.0e30 : std::abs(1.0 / ray_dir_y);

    int64_t step_x = 0;
    int64_t step_y = 0;
    double side_dist_x = 0.0;
    double side_dist_y = 0.0;

    if(ray_dir_x < 0){
        step_x = -1;
        side_dist_x = (pos.x - map_x) * delta_dist_x;
    }else{
        step_x = 1;
        side_dist_x = (map_x + 1.0 - pos.x) * delta_dist_x;
    }

    if(ray_dir_y < 0){
        step_y = -1;
        side_dist_y = (pos.y - map_y) * delta_dist_y;
    }else{
        step_y = 1;
        side_dist_y = (map_y + 1.0 - pos.y) * delta_dist_y;
    }

    hit_side = 0;
    double distance = 0.0;

    while(distance < me_game.max_view_distance){
        if(side_dist_x < side_dist_y){
            side_dist_x += delta_dist_x;
            map_x += step_x;
            hit_side = 0;
            distance = side_dist_x;
        }else{
            side_dist_y += delta_dist_y;
            map_y += step_y;
            hit_side = 1;
            distance = side_dist_y;
        }

        if(IsWall(floor_idx, map_x, map_y)){
            break;
        }
    }

    if(distance >= me_game.max_view_distance){
        return me_game.max_view_distance;
    }

    if(hit_side == 0){
        return side_dist_x - delta_dist_x;
    }
    return side_dist_y - delta_dist_y;
}

bool MazeExplorerGame::Display(bool &enabled){
    if(!enabled) return true;

    const auto win_width = static_cast<int>(std::ceil(me_game.box_width)) + 40;
    const auto win_height = static_cast<int>(std::ceil(me_game.box_height)) + 140;
    auto flags = ImGuiWindowFlags_AlwaysAutoResize
               | ImGuiWindowFlags_NoScrollWithMouse
               | ImGuiWindowFlags_NoNavInputs
               | ImGuiWindowFlags_NoScrollbar;
    ImGui::SetNextWindowSize(ImVec2(win_width, win_height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(120, 140), ImGuiCond_FirstUseEver);
    ImGui::Begin("Maze Explorer", &enabled, flags);

    const auto focused = ImGui::IsWindowFocused();

    if(focused && ImGui::IsKeyPressed(SDL_SCANCODE_R)){
        Reset();
    }

    ImVec2 view_pos = ImGui::GetCursorScreenPos();
    ImVec2 view_size(me_game.box_width, me_game.box_height);
    auto draw_list = ImGui::GetWindowDrawList();

    draw_list->AddRect(view_pos, ImVec2(view_pos.x + view_size.x, view_pos.y + view_size.y),
                       ImColor(0.6f, 0.6f, 0.7f, 1.0f));

    const auto t_now = std::chrono::steady_clock::now();
    auto dt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_updated).count();
    if(dt_ms < 0) dt_ms = 0;
    if(dt_ms > 50) dt_ms = 50;
    const auto dt = static_cast<double>(dt_ms) / 1000.0;

    bool moved = false;
    if(focused){
        const auto forward = vec2<double>(std::cos(player_angle), std::sin(player_angle));
        const auto right = vec2<double>(-std::sin(player_angle), std::cos(player_angle));
        vec2<double> move_dir(0.0, 0.0);

        if(ImGui::IsKeyDown(SDL_SCANCODE_W)) move_dir += forward;
        if(ImGui::IsKeyDown(SDL_SCANCODE_S)) move_dir -= forward;
        if(ImGui::IsKeyDown(SDL_SCANCODE_A)) move_dir -= right;
        if(ImGui::IsKeyDown(SDL_SCANCODE_D)) move_dir += right;

        if(ImGui::IsKeyDown(SDL_SCANCODE_LEFT)) player_angle -= me_game.rot_speed * dt;
        if(ImGui::IsKeyDown(SDL_SCANCODE_RIGHT)) player_angle += me_game.rot_speed * dt;

        player_angle = NormalizeAngle(player_angle);

        const auto move_len = std::hypot(move_dir.x, move_dir.y);
        if(move_len > 1.0e-4){
            move_dir.x /= move_len;
            move_dir.y /= move_len;
            const auto delta = move_dir * (me_game.move_speed * dt);
            const auto new_x = player_pos.x + delta.x;
            const auto new_y = player_pos.y + delta.y;
            auto cand = player_pos;
            if(!IsWall(player_floor, static_cast<int64_t>(std::floor(new_x)), static_cast<int64_t>(std::floor(player_pos.y)))){
                cand.x = new_x;
            }
            if(!IsWall(player_floor, static_cast<int64_t>(std::floor(cand.x)), static_cast<int64_t>(std::floor(new_y)))){
                cand.y = new_y;
            }
            player_pos = cand;
            moved = true;
        }

        const auto cell_x = static_cast<int64_t>(std::floor(player_pos.x));
        const auto cell_y = static_cast<int64_t>(std::floor(player_pos.y));

        if(ImGui::IsKeyPressed(SDL_SCANCODE_E) && IsStairUp(player_floor, cell_x, cell_y)){
            player_floor = std::min<int64_t>(me_game.num_floors - 1, player_floor + 1);
        }
        if(ImGui::IsKeyPressed(SDL_SCANCODE_Q) && IsStairDown(player_floor, cell_x, cell_y)){
            player_floor = std::max<int64_t>(0, player_floor - 1);
        }

        if(ImGui::IsKeyPressed(SDL_SCANCODE_SPACE) && !me_game.is_jumping){
            me_game.is_jumping = true;
            me_game.jump_velocity = me_game.jump_speed;
        }
    }

    if(me_game.is_jumping){
        me_game.jump_velocity -= me_game.gravity * dt;
        me_game.jump_height += me_game.jump_velocity * dt;
        if(me_game.jump_height <= 0.0){
            me_game.jump_height = 0.0;
            me_game.jump_velocity = 0.0;
            me_game.is_jumping = false;
        }
    }

    if(moved){
        me_game.walk_phase += dt * 2.2;
    }else{
        me_game.walk_phase *= std::pow(0.85, dt * 60.0);
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_started).count() / 1000.0;
    const auto goal_pos = vec2<double>(goal_cell.x + 0.5, goal_cell.y + 0.5);
    const auto dist_to_goal = std::hypot(goal_pos.x - player_pos.x, goal_pos.y - player_pos.y);
    if(!me_game.level_complete && player_floor == goal_cell.floor && dist_to_goal < 0.5){
        me_game.level_complete = true;
        me_game.completion_time = elapsed;
    }

    const auto bob = std::sin(me_game.walk_phase * 2.0 * pi) * (moved ? 1.0 : 0.25);
    const auto vertical_offset = static_cast<float>((-me_game.jump_height * 0.35 + bob * 0.06) * view_size.y);

    const int ray_count = std::max(80, static_cast<int>(view_size.x / 3.0));
    const auto slice_width = view_size.x / static_cast<float>(ray_count);

    for(int ray = 0; ray < ray_count; ++ray){
        const auto ray_angle = player_angle - me_game.fov * 0.5
                             + (static_cast<double>(ray) + 0.5) * (me_game.fov / ray_count);
        int hit_side = 0;
        const auto dist = CastRay(player_pos, ray_angle, player_floor, hit_side);
        const auto corrected = dist * std::cos(ray_angle - player_angle);
        const auto safe_dist = std::max(0.05, corrected);
        const auto line_height = (view_size.y * 0.8) / safe_dist;
        const auto horizon = view_pos.y + view_size.y * 0.5f + vertical_offset;
        auto draw_start = static_cast<float>(horizon - line_height * 0.5);
        auto draw_end = static_cast<float>(horizon + line_height * 0.5);
        draw_start = std::clamp(draw_start, view_pos.y, view_pos.y + view_size.y);
        draw_end = std::clamp(draw_end, view_pos.y, view_pos.y + view_size.y);

        const auto shade = std::clamp(1.0 - dist / me_game.max_view_distance, 0.2, 1.0);
        auto wall_shade = shade * (hit_side == 1 ? 0.7 : 1.0);

        const auto x0 = view_pos.x + ray * slice_width;
        const auto x1 = x0 + slice_width + 1.0f;

        const auto ceil_col = ImColor(0.05f, 0.06f, 0.08f, 1.0f);
        const auto floor_col = ImColor(0.08f, 0.08f, 0.1f, 1.0f);
        draw_list->AddRectFilled(ImVec2(x0, view_pos.y), ImVec2(x1, draw_start), ceil_col);
        draw_list->AddRectFilled(ImVec2(x0, draw_end), ImVec2(x1, view_pos.y + view_size.y), floor_col);

        const auto wall_col = ImColor(0.75f * wall_shade, 0.62f * wall_shade, 0.45f * wall_shade, 1.0f);
        draw_list->AddRectFilled(ImVec2(x0, draw_start), ImVec2(x1, draw_end), wall_col);
    }

    if(player_floor == goal_cell.floor){
        const auto to_goal = goal_pos - player_pos;
        const auto angle_to_goal = std::atan2(to_goal.y, to_goal.x);
        const auto rel_angle = AngleDelta(angle_to_goal - player_angle);
        if(std::abs(rel_angle) < me_game.fov * 0.5 && dist_to_goal < me_game.max_view_distance){
            int hit_side = 0;
            const auto wall_dist = CastRay(player_pos, angle_to_goal, player_floor, hit_side);
            if(wall_dist >= dist_to_goal - 0.1){
                const auto screen_x = view_pos.x + (static_cast<float>(rel_angle / me_game.fov) + 0.5f) * view_size.x;
                const auto sprite_height = static_cast<float>((view_size.y * 0.6) / std::max(dist_to_goal, 0.3));
                const auto pulse = 0.6f + 0.4f * std::sin(elapsed * 3.0);
                const auto float_offset = std::sin(elapsed * 2.5) * sprite_height * 0.15f;
                const auto center_y = view_pos.y + view_size.y * 0.5f + vertical_offset - float_offset;

                const auto glow_col = ImColor(1.0f, 0.95f, 0.7f, 0.6f * pulse);
                const auto core_col = ImColor(1.0f, 0.9f, 0.4f, 0.95f);

                draw_list->AddCircleFilled(ImVec2(screen_x, center_y), sprite_height * 0.18f * (0.8f + pulse * 0.4f), core_col);
                draw_list->AddCircle(ImVec2(screen_x, center_y), sprite_height * 0.35f * (0.7f + pulse * 0.5f), glow_col, 24, 2.0f);
                draw_list->AddLine(ImVec2(screen_x - sprite_height * 0.2f, center_y),
                                   ImVec2(screen_x + sprite_height * 0.2f, center_y),
                                   glow_col, 2.0f);
                draw_list->AddLine(ImVec2(screen_x, center_y - sprite_height * 0.2f),
                                   ImVec2(screen_x, center_y + sprite_height * 0.2f),
                                   glow_col, 2.0f);
            }
        }
    }

    const auto timer_value = me_game.level_complete ? me_game.completion_time : elapsed;
    std::stringstream ss;
    ss << "Time: " << std::fixed << std::setprecision(1) << timer_value << "s";
    ss << "  Floor " << (player_floor + 1) << "/" << me_game.num_floors;

    if(me_game.level_complete){
        ss << "  Level complete!";
    }

    draw_list->AddText(ImVec2(view_pos.x + 10.0f, view_pos.y + 10.0f),
                       ImColor(1.0f, 1.0f, 1.0f, 1.0f), ss.str().c_str());

    const auto cell_x = static_cast<int64_t>(std::floor(player_pos.x));
    const auto cell_y = static_cast<int64_t>(std::floor(player_pos.y));
    const auto stair_up = IsStairUp(player_floor, cell_x, cell_y);
    const auto stair_down = IsStairDown(player_floor, cell_x, cell_y);
    if(stair_up || stair_down){
        const char *stair_text = nullptr;
        if(stair_up && stair_down){
            stair_text = "Stairs up (E) / down (Q)";
        }else if(stair_up){
            stair_text = "Stairs up (E)";
        }else{
            stair_text = "Stairs down (Q)";
        }
        draw_list->AddText(ImVec2(view_pos.x + 10.0f, view_pos.y + 30.0f),
                           ImColor(0.4f, 0.0f, 0.8f, 1.0f), stair_text);
    }

    const auto figure_origin = ImVec2(view_pos.x + view_size.x - 70.0f, view_pos.y + view_size.y - 90.0f);
    const auto leg_swing = static_cast<float>(std::sin(me_game.walk_phase * 2.0 * pi) * 6.0);
    const auto jump_lift = static_cast<float>(me_game.jump_height * 18.0);

    const auto head_center = ImVec2(figure_origin.x + 20.0f, figure_origin.y + 12.0f - jump_lift);
    draw_list->AddCircleFilled(head_center, 6.0f, ImColor(0.9f, 0.85f, 0.7f, 1.0f));

    const auto body_top = ImVec2(head_center.x, head_center.y + 6.0f);
    const auto body_bottom = ImVec2(head_center.x, head_center.y + 28.0f);
    draw_list->AddLine(body_top, body_bottom, ImColor(0.9f, 0.9f, 0.95f, 1.0f), 2.0f);

    const auto arm_left = ImVec2(head_center.x - 10.0f, head_center.y + 16.0f - (me_game.is_jumping ? 4.0f : 0.0f));
    const auto arm_right = ImVec2(head_center.x + 10.0f, head_center.y + 16.0f - (me_game.is_jumping ? 4.0f : 0.0f));
    draw_list->AddLine(ImVec2(head_center.x, head_center.y + 12.0f), arm_left, ImColor(0.8f, 0.8f, 0.9f, 1.0f), 2.0f);
    draw_list->AddLine(ImVec2(head_center.x, head_center.y + 12.0f), arm_right, ImColor(0.8f, 0.8f, 0.9f, 1.0f), 2.0f);

    const auto leg_left = ImVec2(body_bottom.x - 6.0f - leg_swing, body_bottom.y + 16.0f);
    const auto leg_right = ImVec2(body_bottom.x + 6.0f + leg_swing, body_bottom.y + 16.0f);
    draw_list->AddLine(body_bottom, leg_left, ImColor(0.7f, 0.7f, 0.85f, 1.0f), 2.0f);
    draw_list->AddLine(body_bottom, leg_right, ImColor(0.7f, 0.7f, 0.85f, 1.0f), 2.0f);

    ImGui::Dummy(view_size);
    ImGui::Separator();
    ImGui::Text("Controls: WASD move, arrows turn, space jump, E/Q stairs, R reset");
    ImGui::Text("Goal: locate the floating relic to finish the level.");

    t_updated = t_now;

    ImGui::End();
    return true;
}
