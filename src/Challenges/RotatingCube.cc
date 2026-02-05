// RotatingCubeGame.cc - A part of DICOMautomaton 2026. Written by hal clark.

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

#include "../Rotating_Cube.h"

#include "RotatingCube.h"

RotatingCubeGame::RotatingCubeGame(){
    Reset();
}

void RotatingCubeGame::Reset(){
    rc_game.reset(rc_game_size);

    rc_game_colour_map.clear();

    rc_game_move_history.clear();
    rc_game_move_history_current = 0L;

    // Reset the update time.
    const auto t_now = std::chrono::steady_clock::now();
    t_cube_updated = t_now;

    return;
}

void RotatingCubeGame::append_history(rc_game_t::move_t x){

    // Trim any moves after the current history, if any are present.
    {
        decltype(rc_game_move_history) l_history;
        auto n = rc_game_move_history_current;
        for(const auto& move : rc_game_move_history){
            --n;
            if(n < 0L) break;
            l_history.emplace_back(move);
        }
        rc_game_move_history = l_history;
    }

    rc_game_move_history.emplace_back(x);
    const auto n = static_cast<int64_t>(rc_game_move_history.size());
    rc_game_move_history_current = n;
    return;
}

void RotatingCubeGame::jump_to_history(int64_t n){
    const auto l_N = static_cast<int64_t>(rc_game_move_history.size());
    if( !isininc(0L, n, l_N) ){
        throw std::invalid_argument("Requested history is not available");
    }

    rc_game.reset(rc_game_size);
    rc_game_move_history_current = n;
    for(const auto& move : rc_game_move_history){
        --n;
        if(n < 0L) break;
        rc_game.move(move);
    }
    return;
}


bool RotatingCubeGame::Display(bool &enabled){
    if( !enabled ) return true;

    const auto win_width  = static_cast<int>(700);
    const auto win_height = static_cast<int>(500);
    auto flags = ImGuiWindowFlags_AlwaysAutoResize
               | ImGuiWindowFlags_NoScrollWithMouse
               | ImGuiWindowFlags_NoNavInputs
               | ImGuiWindowFlags_NoScrollbar ;
    ImGui::SetNextWindowSize(ImVec2(win_width, win_height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::Begin("Cube", &enabled, flags );

    const auto f = ImGui::IsWindowFocused();

    // Reset the game before any game state is used.
    if( f && ImGui::IsKeyPressed(SDL_SCANCODE_R) ){
        Reset();
    }

    double t_updated_diff = 0.0;
    double t_diff_decay_factor = 0.0;
    const auto update_t_diff = [&](){
        const auto t_now = std::chrono::steady_clock::now();
        t_updated_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_cube_updated).count();
        t_diff_decay_factor = 1.0 - (std::clamp<double>(t_updated_diff, 0.0, rc_game_anim_dt) / rc_game_anim_dt);
        return;
    };
    update_t_diff();

    const auto propagate_t_diff = [&](){
        const auto t_now = std::chrono::steady_clock::now();
        t_cube_updated = t_now;
        update_t_diff();
        return;
    };

    if(f){
        ImGuiIO &io = ImGui::GetIO();
        const bool hotkey_ctrl_z = io.KeyCtrl && ImGui::IsKeyPressed(SDL_SCANCODE_Z);
        const bool hotkey_ctrl_y = io.KeyCtrl && ImGui::IsKeyPressed(SDL_SCANCODE_Y);

        const auto l_N = static_cast<int64_t>(rc_game_move_history.size());
        if(0L < l_N){
            const auto l_n = rc_game_move_history_current;
            if(false){
            }else if( hotkey_ctrl_z
                  &&  (0L < l_n) ){
                jump_to_history( l_n - 1L );
                propagate_t_diff();
            }else if( hotkey_ctrl_y
                  &&  (l_n < l_N) ){
                jump_to_history( l_n + 1L );
                propagate_t_diff();
            }
        }
    }

    const auto reset = ImGui::Button("Reset");
    if(reset){
        Reset();
    }
    ImGui::SameLine();
    {
        auto l_N = static_cast<int>(rc_game.get_N());
        ImGui::SliderInt("Size", &l_N, 1, 10);
        if(l_N != rc_game_size){
            rc_game_size = l_N;
            Reset();
        }
    }
    for(const int64_t n : { 3L, 4L, 5L, 7L, 10L}){
        ImGui::SameLine();
        std::stringstream ss;
        ss << "Scramble (" << n << ")";
        const auto scramble = ImGui::Button(ss.str().c_str());
        if(scramble){
            const auto moves = rc_game.generate_random_moves(n);
            for(const auto& move : moves){
                rc_game.move(move);
                append_history( move );
                propagate_t_diff();
            }
        }
    }
    if( !rc_game_move_history.empty() ){
        ImGui::SameLine();

        const auto l_orig = rc_game_move_history_current;
        imgui_slider_int_wrapper<int64_t>("History", rc_game_move_history_current,
                                          0L, static_cast<int64_t>(rc_game_move_history.size()),
            [&](void){
                jump_to_history( rc_game_move_history_current );
                propagate_t_diff();
                return;
            });
    }
    ImGui::Separator();

    const int64_t rc_game_box_width = 1200;
    const int64_t rc_game_box_height = 800;


    // Lay out the faces and cells on a grid.
    const auto rc_game_N = rc_game.get_N();
    
    const int64_t cell_count_height = rc_game_N * 3 + 2;
    const int64_t cell_count_width = rc_game_N * 4 + 2;

    const int64_t cell_height = static_cast<int64_t>(std::floor(1.0f * rc_game_box_height / cell_count_height));
    const int64_t cell_width  = static_cast<int64_t>(std::floor(1.0f * rc_game_box_width  / cell_count_width ));


    ImVec2 curr_screen_pos = ImGui::GetCursorScreenPos();
    ImVec2 curr_window_pos = ImGui::GetCursorPos();
    //ImVec2 window_extent = ImGui::GetContentRegionAvail();
    ImDrawList *window_draw_list = ImGui::GetWindowDrawList();

    {
        const auto c = ImColor(0.7f, 0.7f, 0.8f, 1.0f);
        window_draw_list->AddRect(curr_screen_pos, ImVec2( curr_screen_pos.x + rc_game_box_width, 
                                                           curr_screen_pos.y + rc_game_box_height ), c);
    }

    const auto block_dims = ImVec2( cell_width, cell_height );

    // Use a placeholder object to determine which drag-and-drop payload is available, if any.
    ImGui::Dummy(ImVec2( rc_game_box_width, rc_game_box_height));
    std::optional<int64_t> drag_and_drop_index;
    if( ImGui::BeginDragDropTarget() ){
        int64_t l_index = -10;
        if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("rc_game_index", ImGuiDragDropFlags_AcceptPeekOnly); payload != nullptr){
            if(payload->DataSize != sizeof(int64_t)){
                throw std::logic_error("Drag-and-drop payload is not expected size, refusing to continue");
            }
            drag_and_drop_index = *static_cast<const int64_t*>(payload->Data);
        }

        ImGui::EndDragDropTarget();
    }
    ImGui::SetCursorPos(curr_window_pos);

    std::optional< std::tuple<int64_t, int64_t, rc_game_t::coords_t> > drag_and_drop_grid_coords;



    // Walk over the grid.
    for(int64_t i = 1; (i+1) < cell_count_width; ++i){
        for(int64_t j = 1; (j+1) < cell_count_height; ++j){
            ImVec2 cell_pos_window( curr_window_pos.x + (cell_width  * i),
                                    curr_window_pos.y + (cell_height * j) );
            ImVec2 cell_pos_screen( curr_screen_pos.x + (cell_width  * i),
                                    curr_screen_pos.y + (cell_height * j) );

            rc_game_t::coords_t c { -1, -1, -1 };

            if(false){
            }else if( isininc( rc_game_N*0 + 1, i, rc_game_N*1) 
                  &&  isininc( rc_game_N*1 + 1, j, rc_game_N*2) ){
                std::get<0>(c) = 0; // Face number.
                std::get<1>(c) = i - 1 - (rc_game_N*0); // x coordinate.
                std::get<2>(c) = j - 1 - (rc_game_N*1); // y coordinate.

            }else if( isininc( rc_game_N*1 + 1, i, rc_game_N*2) 
                  &&  isininc( rc_game_N*1 + 1, j, rc_game_N*2) ){
                std::get<0>(c) = 1; // Face number.
                std::get<1>(c) = i - 1 - (rc_game_N*1); // x coordinate.
                std::get<2>(c) = j - 1 - (rc_game_N*1); // y coordinate.

            }else if( isininc( rc_game_N*2 + 1, i, rc_game_N*3) 
                  &&  isininc( rc_game_N*1 + 1, j, rc_game_N*2) ){
                std::get<0>(c) = 2; // Face number.
                std::get<1>(c) = i - 1 - (rc_game_N*2); // x coordinate.
                std::get<2>(c) = j - 1 - (rc_game_N*1); // y coordinate.

            }else if( isininc( rc_game_N*3 + 1, i, rc_game_N*4) 
                  &&  isininc( rc_game_N*1 + 1, j, rc_game_N*2) ){
                std::get<0>(c) = 3; // Face number.
                std::get<1>(c) = i - 1 - (rc_game_N*3); // x coordinate.
                std::get<2>(c) = j - 1 - (rc_game_N*1); // y coordinate.

            }else if( isininc( rc_game_N*1 + 1, i, rc_game_N*2) 
                  &&  isininc( rc_game_N*0 + 1, j, rc_game_N*1) ){
                std::get<0>(c) = 4; // Face number.
                std::get<1>(c) = i - 1 - (rc_game_N*1); // x coordinate.
                std::get<2>(c) = j - 1 - (rc_game_N*0); // y coordinate.

            }else if( isininc( rc_game_N*2 + 1, i, rc_game_N*3) 
                  &&  isininc( rc_game_N*2 + 1, j, rc_game_N*3) ){
                std::get<0>(c) = 5; // Face number.
                std::get<1>(c) = i - 1 - (rc_game_N*2); // x coordinate.
                std::get<2>(c) = j - 1 - (rc_game_N*2); // y coordinate.

            }

            // Invert the y coordinate (map between screen space and the cell layout).
            std::get<2>(c) = (rc_game_N - 1) - std::get<2>(c);

            const auto index = rc_game.index(c);

            if(rc_game.confirm_index_valid(index)){
                // If this is the cell being dragged, save the coordinates for later.
                if( drag_and_drop_index
                &&  (index == drag_and_drop_index.value()) ){
                    drag_and_drop_grid_coords = std::make_tuple(i, j, c);
                }

                const auto l_colour_num = rc_game.get_const_cell(index).colour;
                auto l_colour = rc_game.colour_to_rgba(l_colour_num);

                // Animate the colour.
                const auto has_cached_colour = (rc_game_colour_map.count(index) != 0UL);
                if(!has_cached_colour){
                    // Insert the current colour into the cache.
                    rc_game_colour_map[index] = l_colour_num;
                }else{
                    // If the blend factor is sufficiently low, update the cache to terminate the animation.
                    if(t_diff_decay_factor < 0.01){
                        rc_game_colour_map[index] = l_colour_num;
                    }

                    // Retrieve the cached colour and interpolate between the colours.
                    const auto prev_colour_num = rc_game_colour_map[index];
                    const auto prev_colour = rc_game.colour_to_rgba(prev_colour_num);
                    for(auto i = 0UL; i < l_colour.size(); ++i){
                        l_colour.at(i) = std::clamp<float>(l_colour.at(i) + (prev_colour.at(i) - l_colour.at(i)) * t_diff_decay_factor, 0.0f, 1.0f);
                    }
                }
                const auto im_col = ImColor(l_colour.at(0), l_colour.at(1), l_colour.at(2), l_colour.at(3)).Value;

                // Check if this cell can accept an in-progress drag-and-drop index.
                bool drag_and_drop_active = !!drag_and_drop_index;

                const auto & [ cell_face, cell_x, cell_y ] = c;

                std::stringstream ss;
                ss << "##";
                ss << i << ", " << j << "\n"
                   << cell_face << ", "
                   << cell_x << ", "
                   << cell_y << "\n";

                ImGui::SetCursorPos(cell_pos_window);

                int styles_overridden = 0;
                {
                    auto im_colour_button = im_col;
                    auto im_colour_hovered = im_col;
                    auto im_colour_active = im_col;

                    im_colour_button.w *= 0.9;
                    im_colour_hovered.w *= 0.8;
                    im_colour_active.w *= 0.6;

                    if(drag_and_drop_active){
                        im_colour_button.w *= 0.75;
                        im_colour_hovered.w *= 0.75;
                        im_colour_active.w *= 0.75;
                    }

                    // Temporarily alter the appearance of buttons.
                    const auto& styles = ImGui::GetStyle();

                    ImGui::PushStyleColor(ImGuiCol_Button, im_colour_button);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, im_colour_hovered);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, im_colour_active);
                    styles_overridden += 3;
                }

                //ImGui::PushStyleColor(ImGuiCol_Button, im_col);

                // Draw the button.
                //
                // Note that if the text is not unique, then a unique ID needs to be provided.
                ImGui::Button(ss.str().c_str(), block_dims);

                //ImGui::PopStyleColor(1);
                if(0L < styles_overridden){
                    ImGui::PopStyleColor(styles_overridden);
                }

                // Make the cell draggable.
                if( ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)){
                    int64_t payload = index;
                    ImGui::SetDragDropPayload("rc_game_index", static_cast<void*>(&payload), sizeof(payload));

                    // Show a preview of the cube being dragged.
                    ImGui::Text("Cell");

                    ImGui::EndDragDropSource();
                }

                // Draw a border around the cell.
                const auto c_border = ImColor(1.0f, 1.0f, 1.0f, 0.60f);
                window_draw_list->AddRect(cell_pos_screen, ImVec2( cell_pos_screen.x + block_dims.x, 
                                                                   cell_pos_screen.y + block_dims.y ), c_border);
            }
        }
    }
    ImGui::SetCursorPos(curr_window_pos);



    // Walk over the grid.
    if(drag_and_drop_grid_coords){
        auto [i, j, c] = drag_and_drop_grid_coords.value();

        using pack_t = std::vector< std::tuple< int64_t, int64_t, rc_direction, std::string > >;
        for( auto [di, dj, dir, desc] : pack_t { { -1,  0, rc_direction::left,         std::string("left") },
                                                 {  1,  0, rc_direction::right,        std::string("right") },
                                                 {  0, -1, rc_direction::up,           std::string("up") },
                                                 {  0,  1, rc_direction::down,         std::string("down") },
                                                 { -1, -1, rc_direction::rotate_left,  std::string("rotate\nleft") },
                                                 {  1, -1, rc_direction::rotate_right, std::string("rotate\nright") } } ){
            ImVec2 cell_pos_screen( curr_screen_pos.x + (cell_width  * (i + di)),
                                    curr_screen_pos.y + (cell_height * (j + dj)) );
            ImVec2 cell_pos_window( curr_window_pos.x + (cell_width  * (i + di)),
                                    curr_window_pos.y + (cell_height * (j + dj)) );

            std::stringstream ss;
            if( false ){
            }else if( (dir == rc_direction::left)
                  ||  (dir == rc_direction::right)
                  ||  (dir == rc_direction::up)
                  ||  (dir == rc_direction::down) ){
                const auto [adj_c, adj_dir] = rc_game.get_neighbour_cell({c, dir});
                const auto [adj_f, adj_x, adj_y] = adj_c;
                ss << "##" << desc << "\n" << adj_f << "," << adj_x << "," << adj_y;
            }else{
                ss << "##" << desc;
            }

            ImGui::SetCursorPos(cell_pos_window);
            ImGui::Button(ss.str().c_str(), block_dims);

            // Accept a cell dragged here.
            if( ImGui::BeginDragDropTarget() ){
                if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("rc_game_index"); payload != nullptr){
                    if(payload->DataSize != sizeof(int64_t)){
                        throw std::logic_error("Drag-and-drop payload is not expected size, refusing to continue");
                    }
                    const int64_t payload_index = *static_cast<const int64_t*>(payload->Data);
                    const int64_t l_index = rc_game.index(c);
                    if(l_index != payload_index){
                        throw std::logic_error("Drag-and-drop inconsistency, unable to continue");
                    }

                    // Implement the move.
                    const auto l_move = rc_game_t::move_t{c, dir};
                    rc_game.move(l_move);
                    append_history( l_move );
                    propagate_t_diff();
                }
                ImGui::EndDragDropTarget();
            }

            // Show an indicator of what the drop buttons will do.
            const auto c_border = ImColor(0.8f, 0.8f, 0.8f, 1.0f);

            std::vector<vec2<float>> verts;

            if( (dir == rc_direction::left)
            ||  (dir == rc_direction::right)
            ||  (dir == rc_direction::up)
            ||  (dir == rc_direction::down) ){
                // Left arrow.        x       y
                verts.emplace_back( -0.50,  0.00 );
                verts.emplace_back( -0.10, -0.35 );
                verts.emplace_back( -0.10, -0.20 );
                verts.emplace_back(  0.50, -0.20 );
                verts.emplace_back(  0.50,  0.20 );
                verts.emplace_back( -0.10,  0.20 );
                verts.emplace_back( -0.10,  0.35 );
                verts.emplace_back( -0.50,  0.00 );

            }else{
                // Left rotation symbol.
                verts.emplace_back( -0.40,  0.40 );
                verts.emplace_back( -0.40, -0.05 );
                verts.emplace_back( -0.25,  0.10 );
                verts.emplace_back(  0.00, -0.05 );
                verts.emplace_back( -0.25, -0.35 );
                verts.emplace_back(  0.10, -0.45 );
                verts.emplace_back(  0.40,  0.00 );
                verts.emplace_back( -0.05,  0.30 );
                verts.emplace_back(  0.05,  0.40 );
                verts.emplace_back( -0.40,  0.40 );
            }

            const auto pi = std::acos(-1.0);
            for(auto & v : verts){
                if(false){
                }else if(dir == rc_direction::left){
                    // Do nothing.
                }else if(dir == rc_direction::right){
                    v = v.rotate_around_z(pi);
                }else if(dir == rc_direction::up){
                    v = v.rotate_around_z(pi * 1.5);
                }else if(dir == rc_direction::down){
                    v = v.rotate_around_z(pi * 0.5);

                }else if(dir == rc_direction::rotate_left){
                    // Do nothing.
                }else if(dir == rc_direction::rotate_right){
                    // Mirror.
                    v.x *= -1.0;
                }
            }

            window_draw_list->PathClear();
            for(auto & v : verts){
                // Scale and rotate if needed.
                ImVec2 im_v;
                im_v.x = cell_pos_screen.x + block_dims.x * 0.5 + v.x * (block_dims.x * 0.45);
                im_v.y = cell_pos_screen.y + block_dims.y * 0.5 - v.y * (block_dims.y * 0.45);
                window_draw_list->PathLineTo( im_v );
            }
            float thickness = 1.5f;
            bool closed = false;
            window_draw_list->PathStroke(c_border, closed, thickness);
        }

    }
    ImGui::SetCursorPos(curr_window_pos);
    ImGui::Dummy(ImVec2(rc_game_box_width, rc_game_box_height));
    ImGui::End();

    return true;
}

