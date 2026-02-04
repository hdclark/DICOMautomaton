// FreeSki.cc - A part of DICOMautomaton 2026. Written by hal clark.

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

#include "FreeSki.h"

FreeSkiGame::FreeSkiGame(){
    fs_game.re.seed( std::random_device()() );
    Reset();
}

void FreeSkiGame::Reset(){
    fs_game_objs.clear();
    
    fs_game.skier_x = fs_game.box_width / 2.0;
    fs_game.skier_y = fs_game.box_height * 0.15;
    fs_game.scroll_speed = 50.0;
    fs_game.world_scroll_y = 0.0;
    fs_game.is_jumping = false;
    fs_game.jump_height = 0.0;
    fs_game.jump_velocity = 0.0;
    fs_game.did_flip = false;
    fs_game.can_double_tap = false;
    fs_game.game_over = false;
    fs_game.game_over_time = 0.0;
    fs_game.countdown_active = true;
    fs_game.countdown_remaining = 3.0;
    fs_game.score = 0;
    
    fs_game.last_tree_spawn = 0.0;
    fs_game.last_rock_spawn = 0.0;
    fs_game.last_jump_spawn = 0.0;
    fs_game.last_other_skier_spawn = 0.0;
    
    // Reset the update time.
    const auto t_now = std::chrono::steady_clock::now();
    t_fs_updated = t_now;
    t_fs_started = t_now;
    t_fs_last_spacebar = t_now;

    fs_game.re.seed( std::random_device()() );
    return;
}



bool FreeSkiGame::Display(bool &enabled){
    if( !enabled ) return true;

    const auto win_width  = static_cast<int>( std::ceil(fs_game.box_width) ) + 15;
    const auto win_height = static_cast<int>( std::ceil(fs_game.box_height) ) + 60;
    auto flags = ImGuiWindowFlags_AlwaysAutoResize
               | ImGuiWindowFlags_NoScrollWithMouse
               | ImGuiWindowFlags_NoNavInputs
               | ImGuiWindowFlags_NoScrollbar ;
    ImGui::SetNextWindowSize(ImVec2(win_width, win_height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::Begin("FreeSki", &enabled, flags );

    const auto f = ImGui::IsWindowFocused();

    // Reset the game before any game state is used.
    if( f && ImGui::IsKeyPressed(SDL_SCANCODE_R) ){
        Reset();
    }

    // Display state.
    ImVec2 curr_pos = ImGui::GetCursorScreenPos();
    ImDrawList *window_draw_list = ImGui::GetWindowDrawList();

    // Draw border
    {
        const auto c = ImColor(0.7f, 0.7f, 0.8f, 1.0f);
        window_draw_list->AddRect(curr_pos, ImVec2( curr_pos.x + fs_game.box_width, 
                                                    curr_pos.y + fs_game.box_height ), c);
    }

    const auto t_now = std::chrono::steady_clock::now();
    auto t_updated_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_fs_updated).count();
    
    // Limit time steps to avoid simulation breakdown
    if(30 < t_updated_diff) t_updated_diff = 30;
    const auto dt = static_cast<double>(t_updated_diff) / 1000.0; // Delta time in seconds

    // Handle countdown
    if(fs_game.countdown_active){
        fs_game.countdown_remaining -= dt;
        if(fs_game.countdown_remaining <= 0.0){
            fs_game.countdown_active = false;
            fs_game.countdown_remaining = 0.0;
            // Initialize spawn timers when countdown finishes to prevent immediate burst
            const auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_fs_started).count() / 1000.0;
            fs_game.last_tree_spawn = current_time;
            fs_game.last_rock_spawn = current_time;
            fs_game.last_jump_spawn = current_time;
            fs_game.last_other_skier_spawn = current_time;
        }
        
        // Draw countdown
        const auto countdown_text = std::to_string(static_cast<int>(std::ceil(fs_game.countdown_remaining)));
        const auto text_size = ImGui::CalcTextSize(countdown_text.c_str());
        const ImVec2 text_pos(curr_pos.x + fs_game.box_width/2.0 - text_size.x/2.0,
                              curr_pos.y + fs_game.box_height/2.0 - text_size.y/2.0);
        window_draw_list->AddText(text_pos, ImColor(1.0f, 1.0f, 1.0f, 1.0f), countdown_text.c_str());
    }

    // Update game state when not in countdown and not game over
    if(!fs_game.countdown_active && !fs_game.game_over){
        // Gradually increase scroll speed
        fs_game.scroll_speed += fs_game.speed_increase_rate * dt;
        if(fs_game.scroll_speed > fs_game.max_scroll_speed){
            fs_game.scroll_speed = fs_game.max_scroll_speed;
        }
        
        // Update world scroll
        fs_game.world_scroll_y += fs_game.scroll_speed * dt;
        
        // Handle player left/right movement
        if( f ){
            const auto move_speed = 200.0;
            if( ImGui::IsKeyDown(SDL_SCANCODE_LEFT) ){
                fs_game.skier_x -= move_speed * dt;
            }
            if( ImGui::IsKeyDown(SDL_SCANCODE_RIGHT) ){
                fs_game.skier_x += move_speed * dt;
            }
            // Clamp to bounds
            fs_game.skier_x = std::clamp(fs_game.skier_x, fs_game.skier_size, fs_game.box_width - fs_game.skier_size);
            
            // Handle jump
            if( ImGui::IsKeyPressed(SDL_SCANCODE_SPACE) ){
                if(!fs_game.is_jumping){
                    // Start jump
                    fs_game.is_jumping = true;
                    fs_game.jump_velocity = fs_game.jump_speed;
                    fs_game.did_flip = false;
                    fs_game.can_double_tap = true;
                    t_fs_last_spacebar = t_now;
                }else if(fs_game.can_double_tap){
                    // Check if this is a double-tap while airborne
                    const auto t_since_last_spacebar = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_fs_last_spacebar).count();
                    if(t_since_last_spacebar > 50 && t_since_last_spacebar < 300){
                        // Double tap detected - do a flip!
                        fs_game.did_flip = true;
                        fs_game.can_double_tap = false;
                    }
                    t_fs_last_spacebar = t_now;
                }
            }
        }
        
        // Update jump physics
        if(fs_game.is_jumping){
            const auto gravity = 400.0; // pixels/sec^2
            fs_game.jump_velocity -= gravity * dt;
            fs_game.jump_height += fs_game.jump_velocity * dt;
            
            if(fs_game.jump_height <= 0.0){
                fs_game.jump_height = 0.0;
                fs_game.is_jumping = false;
                fs_game.jump_velocity = 0.0;
                fs_game.can_double_tap = false; // Reset flip capability when landing
            }
        }
        
        // Spawn new objects
        const auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_fs_started).count() / 1000.0;
        std::uniform_real_distribution<> rd_x(20.0, fs_game.box_width - 20.0);
        
        // Trees
        if((total_time - fs_game.last_tree_spawn) > (1.0 / fs_game.tree_spawn_rate)){
            fs_game_objs.emplace_back();
            fs_game_objs.back().type = fs_obj_type_t::tree;
            fs_game_objs.back().pos = vec2<double>(rd_x(fs_game.re), fs_game.world_scroll_y + fs_game.box_height + 50.0);
            fs_game_objs.back().size = 25.0;
            fs_game.last_tree_spawn = total_time;
        }
        
        // Rocks
        if((total_time - fs_game.last_rock_spawn) > (1.0 / fs_game.rock_spawn_rate)){
            fs_game_objs.emplace_back();
            fs_game_objs.back().type = fs_obj_type_t::rock;
            fs_game_objs.back().pos = vec2<double>(rd_x(fs_game.re), fs_game.world_scroll_y + fs_game.box_height + 50.0);
            fs_game_objs.back().size = 12.0;
            fs_game.last_rock_spawn = total_time;
        }
        
        // Jumps
        if((total_time - fs_game.last_jump_spawn) > (1.0 / fs_game.jump_spawn_rate)){
            fs_game_objs.emplace_back();
            fs_game_objs.back().type = fs_obj_type_t::jump;
            fs_game_objs.back().pos = vec2<double>(rd_x(fs_game.re), fs_game.world_scroll_y + fs_game.box_height + 50.0);
            fs_game_objs.back().size = 20.0;
            fs_game.last_jump_spawn = total_time;
        }
        
        // Other skiers
        if((total_time - fs_game.last_other_skier_spawn) > (1.0 / fs_game.other_skier_spawn_rate)){
            fs_game_objs.emplace_back();
            fs_game_objs.back().type = fs_obj_type_t::other_skier;
            fs_game_objs.back().pos = vec2<double>(rd_x(fs_game.re), fs_game.world_scroll_y + fs_game.box_height + 50.0);
            fs_game_objs.back().size = 12.0;
            fs_game.last_other_skier_spawn = total_time;
        }
        
        // Check collisions with objects
        const auto skier_world_y = fs_game.world_scroll_y + fs_game.skier_y;
        for(auto &obj : fs_game_objs){
            const auto dx = obj.pos.x - fs_game.skier_x;
            const auto dy = obj.pos.y - skier_world_y;
            const auto dist = std::sqrt(dx*dx + dy*dy);
            const auto collision_dist = fs_game.skier_size + obj.size;
            
            if(dist < collision_dist){
                // Collision detected
                if(obj.type == fs_obj_type_t::jump){
                    // Hit a jump
                    if(fs_game.is_jumping){
                        // Jumped over it - award points
                        fs_game.score += fs_game.did_flip ? 2 : 1;
                        // Remove the jump
                        obj.pos.y = fs_game.world_scroll_y + fs_game.box_height + 100.0;
                    }else{
                        // Hit it - speed boost
                        fs_game.scroll_speed = std::min(fs_game.scroll_speed * 1.15, fs_game.max_scroll_speed);
                        // Remove the jump
                        obj.pos.y = fs_game.world_scroll_y + fs_game.box_height + 100.0;
                    }
                    continue; // Skip to next object
                }else if(obj.type == fs_obj_type_t::rock){
                    if(fs_game.is_jumping){
                        // Jumped over rock - award points
                        fs_game.score += fs_game.did_flip ? 2 : 1;
                        obj.pos.y = fs_game.world_scroll_y + fs_game.box_height + 100.0;
                    }else{
                        // Hit rock - game over
                        fs_game.game_over = true;
                        fs_game.game_over_time = 0.0;
                        fs_game.scroll_speed = 0.0;
                    }
                    continue; // Skip to next object
                }else if(obj.type == fs_obj_type_t::tree || obj.type == fs_obj_type_t::other_skier){
                    // Hit tree or other skier - game over
                    fs_game.game_over = true;
                    fs_game.game_over_time = 0.0;
                    fs_game.scroll_speed = 0.0;
                    continue; // Skip to next object
                }
            }
        }
        
        // Remove objects that are off screen
        fs_game_objs.erase(
            std::remove_if(fs_game_objs.begin(), fs_game_objs.end(),
                [&](const fs_game_obj_t &obj) -> bool {
                    const auto screen_y = obj.pos.y - fs_game.world_scroll_y;
                    return screen_y < -50.0;
                }),
            fs_game_objs.end()
        );
    }

    // Handle game over animation
    if(fs_game.game_over){
        fs_game.game_over_time += dt;
    }

    // Draw objects
    for(const auto &obj : fs_game_objs){
        const auto screen_y = obj.pos.y - fs_game.world_scroll_y;
        
        // Only draw if on screen
        if(screen_y < -50.0 || screen_y > fs_game.box_height + 50.0) continue;
        
        ImVec2 obj_pos = curr_pos;
        obj_pos.x = curr_pos.x + obj.pos.x;
        obj_pos.y = curr_pos.y + screen_y;
        
        if(fs_game.game_over){
            // Apply jitter effect
            const auto jitter_amount = 3.0;
            std::uniform_real_distribution<> rd_jitter(-jitter_amount, jitter_amount);
            obj_pos.x += rd_jitter(fs_game.re);
            obj_pos.y += rd_jitter(fs_game.re);
        }
        
        if(obj.type == fs_obj_type_t::tree){
            // Draw trees as a green triangle
            const auto c = ImColor(0.2f, 0.8f, 0.2f, 1.0f);
            const auto p1 = ImVec2(obj_pos.x, obj_pos.y - obj.size);
            const auto p2 = ImVec2(obj_pos.x - obj.size*0.7, obj_pos.y + obj.size*0.5);
            const auto p3 = ImVec2(obj_pos.x + obj.size*0.7, obj_pos.y + obj.size*0.5);
            window_draw_list->AddTriangleFilled(p1, p2, p3, c);
        }else if(obj.type == fs_obj_type_t::rock){
            // Draw rocks as a gray circle
            const auto c = ImColor(0.5f, 0.5f, 0.5f, 1.0f);
            window_draw_list->AddCircleFilled(obj_pos, obj.size, c);
        }else if(obj.type == fs_obj_type_t::jump){
            // Draw jumps as a brown/yellow box (rectangle)
            const auto c = ImColor(0.8f, 0.6f, 0.2f, 1.0f);
            const auto p1 = ImVec2(obj_pos.x - obj.size, obj_pos.y - obj.size*0.5);
            const auto p2 = ImVec2(obj_pos.x + obj.size, obj_pos.y + obj.size*0.5);
            window_draw_list->AddRectFilled(p1, p2, c);

        }else if(obj.type == fs_obj_type_t::other_skier){
            // Draw other skiers as a cyan triangle
            const auto c = ImColor(0.0f, 0.8f, 0.8f, 1.0f);
            const auto p1 = ImVec2(obj_pos.x, obj_pos.y - obj.size);
            const auto p2 = ImVec2(obj_pos.x - obj.size*0.7, obj_pos.y + obj.size*0.5);
            const auto p3 = ImVec2(obj_pos.x + obj.size*0.7, obj_pos.y + obj.size*0.5);
            window_draw_list->AddTriangleFilled(p1, p2, p3, c);
        }
    }
    
    // Draw the player skier
    {
        ImVec2 skier_pos = curr_pos;
        skier_pos.x = curr_pos.x + fs_game.skier_x;
        skier_pos.y = curr_pos.y + fs_game.skier_y - fs_game.jump_height;
        
        if(fs_game.game_over){
            // Apply twirl effect
            const auto angle = fs_game.game_over_time * 10.0;
            const auto twirl_radius = 10.0;
            skier_pos.x += std::cos(angle) * twirl_radius;
            skier_pos.y += std::sin(angle) * twirl_radius;
        }
        
        // Draw as red triangle
        auto c = ImColor(1.0f, 0.2f, 0.2f, 1.0f);
        if(fs_game.is_jumping){
            c = ImColor(1.0f, 0.5f, 0.0f, 1.0f); // Orange when jumping
        }
        const auto size = fs_game.skier_size;
        const auto p1 = ImVec2(skier_pos.x, skier_pos.y - size);
        const auto p2 = ImVec2(skier_pos.x - size*0.7, skier_pos.y + size*0.5);
        const auto p3 = ImVec2(skier_pos.x + size*0.7, skier_pos.y + size*0.5);
        window_draw_list->AddTriangleFilled(p1, p2, p3, c);
        
        // Draw flip indicator
        if(fs_game.is_jumping && fs_game.did_flip){
            const auto flip_c = ImColor(1.0f, 1.0f, 0.0f, 1.0f);
            window_draw_list->AddCircle(skier_pos, size * 1.5, flip_c, 12, 2.0f);
        }
    }

    // Draw score and speed
    {
        std::stringstream ss;
        ss << "Score: " << fs_game.score;
        ss << "  Speed: " << static_cast<int>(fs_game.scroll_speed);
        const auto score_text = ss.str();
        const ImVec2 text_pos(curr_pos.x + 10, curr_pos.y + 10);
        window_draw_list->AddText(text_pos, ImColor(1.0f, 1.0f, 1.0f, 1.0f), score_text.c_str());
    }
    
    // Draw game over message
    if(fs_game.game_over){
        const auto game_over_text = "GAME OVER! Press R to reset";
        const auto text_size = ImGui::CalcTextSize(game_over_text);
        const ImVec2 text_pos(curr_pos.x + fs_game.box_width/2.0 - text_size.x/2.0,
                              curr_pos.y + fs_game.box_height/2.0 - text_size.y/2.0);
        window_draw_list->AddText(text_pos, ImColor(1.0f, 0.0f, 0.0f, 1.0f), game_over_text);
    }

    t_fs_updated = t_now;

    ImGui::Dummy(ImVec2(fs_game.box_width, fs_game.box_height));
    ImGui::End();
    return true;
}

