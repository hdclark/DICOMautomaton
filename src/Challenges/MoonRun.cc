// MoonRun.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <sstream>

#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"

#include <SDL.h>
#include "../imgui20210904/imgui.h"

#include "MoonRun.h"

namespace {
    constexpr double pi = 3.14159265358979323846;
}

MoonRunGame::MoonRunGame(){
    mr_game.re.seed( std::random_device()() );
    Reset();
}

void MoonRunGame::Reset(){
    mr_obstacles.clear();
    mr_moon_features.clear();
    mr_debris.clear();
    mr_ground_rocks.clear();

    mr_game.ground_y = mr_game.box_height - mr_game.ground_height;
    mr_game.scroll_speed = 220.0;
    mr_game.is_jumping = false;
    mr_game.is_crouching = false;
    mr_game.jump_height = 0.0;
    mr_game.jump_velocity = 0.0;
    mr_game.run_phase = 0.0;
    mr_game.game_over = false;
    mr_game.game_over_time = 0.0;
    mr_game.score = 0;

    mr_game.crater_spawn_timer = 0.4;
    mr_game.aerial_spawn_timer = 0.9;

    mr_game.moon_radius = mr_game.box_width * 0.75;
    mr_game.moon_rotation = 0.0;

    mr_game.re.seed( std::random_device()() );

    std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * pi);
    std::uniform_real_distribution<double> moon_rad_dist(mr_game.moon_radius * 0.45, mr_game.moon_radius * 0.95);
    std::uniform_real_distribution<double> crater_size_dist(3.0, 11.0);
    for(int i = 0; i < 26; ++i){
        mr_moon_feature_t feature;
        feature.angle = angle_dist(mr_game.re);
        feature.radius = moon_rad_dist(mr_game.re);
        feature.size = crater_size_dist(mr_game.re);
        feature.crater = (i % 4 != 0);
        mr_moon_features.push_back(feature);
    }

    std::uniform_real_distribution<double> debris_x_dist(0.0, mr_game.box_width);
    std::uniform_real_distribution<double> debris_y_dist(0.0, mr_game.ground_y - 40.0);
    std::uniform_real_distribution<double> debris_speed_dist(10.0, 25.0);
    std::uniform_real_distribution<double> debris_size_dist(1.0, 3.0);
    for(int i = 0; i < 22; ++i){
        mr_debris_t debris;
        debris.pos = vec2<double>(debris_x_dist(mr_game.re), debris_y_dist(mr_game.re));
        debris.vel = vec2<double>(-debris_speed_dist(mr_game.re), 0.0);
        debris.size = debris_size_dist(mr_game.re);
        mr_debris.push_back(debris);
    }

    std::uniform_real_distribution<double> rock_x_dist(0.0, mr_game.box_width);
    std::uniform_real_distribution<double> rock_size_dist(2.0, 6.0);
    std::uniform_real_distribution<double> rock_height_dist(4.0, 18.0);
    for(int i = 0; i < 18; ++i){
        mr_ground_rock_t rock;
        rock.x = rock_x_dist(mr_game.re);
        rock.size = rock_size_dist(mr_game.re);
        rock.height = rock_height_dist(mr_game.re);
        mr_ground_rocks.push_back(rock);
    }

    const auto t_now = std::chrono::steady_clock::now();
    t_mr_updated = t_now;
    t_mr_started = t_now;
    return;
}

bool MoonRunGame::Display(bool &enabled){
    if( !enabled ) return true;

    const auto win_width = static_cast<int>(mr_game.box_width) + 20;
    const auto win_height = static_cast<int>(mr_game.box_height) + 60;
    auto flags = ImGuiWindowFlags_AlwaysAutoResize
               | ImGuiWindowFlags_NoScrollWithMouse
               | ImGuiWindowFlags_NoNavInputs
               | ImGuiWindowFlags_NoScrollbar;
    ImGui::SetNextWindowSize(ImVec2(win_width, win_height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(120, 120), ImGuiCond_FirstUseEver);
    ImGui::Begin("Moon Run", &enabled, flags);

    const auto f = ImGui::IsWindowFocused();
    if( f && ImGui::IsKeyPressed(SDL_SCANCODE_R) ){
        Reset();
    }

    const auto t_now = std::chrono::steady_clock::now();
    auto t_diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_mr_updated).count();
    if(t_diff_ms > 40) t_diff_ms = 40;
    const double dt = static_cast<double>(t_diff_ms) / 1000.0;

    const auto handle_jump = [&](bool trigger){
        if(trigger && !mr_game.is_jumping){
            mr_game.is_jumping = true;
            mr_game.jump_velocity = mr_game.jump_speed;
            mr_game.is_crouching = false;
        }
    };

    if(f && !mr_game.game_over){
        const bool jump_pressed = ImGui::IsKeyPressed(SDL_SCANCODE_SPACE) || ImGui::IsKeyPressed(SDL_SCANCODE_UP);
        handle_jump(jump_pressed);
        mr_game.is_crouching = ImGui::IsKeyDown(SDL_SCANCODE_DOWN);
    }else if(mr_game.game_over){
        mr_game.is_crouching = false;
    }

    if(!mr_game.game_over){
        mr_game.scroll_speed += mr_game.speed_increase_rate * dt;
        if(mr_game.scroll_speed > mr_game.max_scroll_speed){
            mr_game.scroll_speed = mr_game.max_scroll_speed;
        }

        mr_game.run_phase += dt * (mr_game.run_phase_speed + mr_game.scroll_speed * 0.02);
        if(mr_game.run_phase > 2.0 * pi){
            mr_game.run_phase = std::fmod(mr_game.run_phase, 2.0 * pi);
        }

        mr_game.moon_rotation += dt * mr_game.moon_rotation_speed;
        if(mr_game.moon_rotation > 2.0 * pi){
            mr_game.moon_rotation -= 2.0 * pi;
        }

        if(mr_game.is_jumping){
            mr_game.jump_velocity -= mr_game.gravity * dt;
            mr_game.jump_height += mr_game.jump_velocity * dt;
            if(mr_game.jump_height <= 0.0){
                mr_game.jump_height = 0.0;
                mr_game.is_jumping = false;
                mr_game.jump_velocity = 0.0;
            }
        }

        const auto update_obstacle_positions = [&](double speed_scale){
            for(auto &ob : mr_obstacles){
                ob.pos.x -= mr_game.scroll_speed * dt * speed_scale;
            }
        };
        update_obstacle_positions(1.0);

        mr_game.crater_spawn_timer -= dt;
        mr_game.aerial_spawn_timer -= dt;

        std::uniform_real_distribution<double> crater_interval(0.8, 1.5);
        std::uniform_real_distribution<double> aerial_interval(1.1, 2.1);
        std::uniform_real_distribution<double> crater_size_dist(14.0, 26.0);
        std::uniform_real_distribution<double> aerial_height_dist(70.0, 120.0);
        std::uniform_real_distribution<double> aerial_width_dist(26.0, 42.0);
        std::uniform_real_distribution<double> variant_dist(0.0, 1.0);

        if(mr_game.crater_spawn_timer <= 0.0){
            mr_obstacle_t obstacle;
            obstacle.type = mr_obstacle_type_t::crater;
            obstacle.width = crater_size_dist(mr_game.re);
            obstacle.height = obstacle.width * 0.55;
            obstacle.pos = vec2<double>(mr_game.box_width + obstacle.width + 20.0, mr_game.ground_y);
            mr_obstacles.push_back(obstacle);
            mr_game.crater_spawn_timer = crater_interval(mr_game.re);
        }

        if(mr_game.aerial_spawn_timer <= 0.0){
            mr_obstacle_t obstacle;
            obstacle.type = mr_obstacle_type_t::aerial;
            obstacle.width = aerial_width_dist(mr_game.re);
            obstacle.height = obstacle.width * 0.55;
            obstacle.pos = vec2<double>(mr_game.box_width + obstacle.width + 30.0,
                                        mr_game.ground_y - aerial_height_dist(mr_game.re));
            obstacle.variant = (variant_dist(mr_game.re) > 0.5)
                                 ? mr_aerial_variant_t::alien
                                 : mr_aerial_variant_t::projectile;
            mr_obstacles.push_back(obstacle);
            mr_game.aerial_spawn_timer = aerial_interval(mr_game.re);
        }

        for(auto &rock : mr_ground_rocks){
            rock.x -= mr_game.scroll_speed * dt * 0.4;
            if(rock.x < -rock.size * 3.0){
                std::uniform_real_distribution<double> rock_x_dist(mr_game.box_width, mr_game.box_width * 1.4);
                rock.x = rock_x_dist(mr_game.re);
            }
        }

        for(auto &debris : mr_debris){
            debris.pos += debris.vel * dt;
            if(debris.pos.x < -debris.size * 4.0){
                std::uniform_real_distribution<double> debris_x_dist(mr_game.box_width, mr_game.box_width * 1.5);
                std::uniform_real_distribution<double> debris_y_dist(0.0, mr_game.ground_y - 50.0);
                debris.pos.x = debris_x_dist(mr_game.re);
                debris.pos.y = debris_y_dist(mr_game.re);
            }
        }

        const double runner_height = mr_game.is_crouching ? mr_game.runner_crouch_height : mr_game.runner_height;
        const double runner_left = mr_game.runner_x - mr_game.runner_width * 0.45;
        const double runner_right = mr_game.runner_x + mr_game.runner_width * 0.45;
        const double runner_top = mr_game.ground_y - mr_game.jump_height - runner_height;
        const double runner_bottom = mr_game.ground_y - mr_game.jump_height;

        for(auto &ob : mr_obstacles){
            if(ob.type == mr_obstacle_type_t::crater){
                const double crater_left = ob.pos.x - ob.width;
                const double crater_right = ob.pos.x + ob.width;
                if(crater_right > runner_left && crater_left < runner_right){
                    if(mr_game.jump_height < ob.height * 0.4){
                        mr_game.game_over = true;
                        mr_game.scroll_speed = 0.0;
                        break;
                    }
                }
            }else if(ob.type == mr_obstacle_type_t::aerial){
                const double ob_left = ob.pos.x - ob.width * 0.5;
                const double ob_right = ob.pos.x + ob.width * 0.5;
                const double ob_top = ob.pos.y - ob.height * 0.5;
                const double ob_bottom = ob.pos.y + ob.height * 0.5;
                const bool overlap_x = (ob_right > runner_left) && (ob_left < runner_right);
                const bool overlap_y = (ob_bottom > runner_top) && (ob_top < runner_bottom);
                if(overlap_x && overlap_y){
                    mr_game.game_over = true;
                    mr_game.scroll_speed = 0.0;
                    break;
                }
            }
        }

        for(auto &ob : mr_obstacles){
            if(ob.scored) continue;
            if(ob.pos.x + ob.width < runner_left){
                ob.scored = true;
                mr_game.score += 1;
            }
        }

        mr_obstacles.erase(std::remove_if(mr_obstacles.begin(), mr_obstacles.end(),
                                          [&](const mr_obstacle_t &ob){
                                              return ob.pos.x < -80.0;
                                          }),
                           mr_obstacles.end());

    }else{
        mr_game.game_over_time += dt;
    }

    ImVec2 curr_pos = ImGui::GetCursorScreenPos();
    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    const ImVec2 box_br(curr_pos.x + mr_game.box_width, curr_pos.y + mr_game.box_height);

    draw_list->AddRectFilled(curr_pos, box_br, ImColor(0.04f, 0.05f, 0.1f, 1.0f));

    const ImVec2 moon_center(curr_pos.x + mr_game.box_width * 0.5,
                             curr_pos.y + mr_game.box_height + mr_game.moon_radius * 0.18);
    draw_list->AddCircleFilled(moon_center, static_cast<float>(mr_game.moon_radius),
                               ImColor(0.7f, 0.7f, 0.73f, 1.0f), 120);
    draw_list->AddCircleFilled(ImVec2(moon_center.x + mr_game.moon_radius * 0.12,
                                      moon_center.y - mr_game.moon_radius * 0.08),
                               static_cast<float>(mr_game.moon_radius * 0.95),
                               ImColor(0.76f, 0.76f, 0.78f, 0.9f), 120);

    for(const auto &feature : mr_moon_features){
        const double angle = feature.angle + mr_game.moon_rotation;
        const double fx = moon_center.x + std::cos(angle) * feature.radius;
        const double fy = moon_center.y + std::sin(angle) * feature.radius;
        if(fy > curr_pos.y + mr_game.box_height) continue;
        const float r = static_cast<float>(feature.size);
        const ImVec2 fpos(static_cast<float>(fx), static_cast<float>(fy));
        if(feature.crater){
            draw_list->AddCircleFilled(fpos, r, ImColor(0.55f, 0.55f, 0.58f, 1.0f), 16);
            draw_list->AddCircle(fpos, r * 0.6f, ImColor(0.45f, 0.45f, 0.48f, 1.0f), 12, 1.4f);
        }else{
            draw_list->AddCircleFilled(fpos, r, ImColor(0.62f, 0.62f, 0.65f, 1.0f), 12);
        }
    }

    for(const auto &debris : mr_debris){
        const ImVec2 pos(curr_pos.x + static_cast<float>(debris.pos.x),
                         curr_pos.y + static_cast<float>(debris.pos.y));
        draw_list->AddCircleFilled(pos, static_cast<float>(debris.size),
                                   ImColor(0.85f, 0.85f, 0.9f, 0.8f), 8);
    }

    const ImVec2 ground_tl(curr_pos.x, curr_pos.y + mr_game.ground_y);
    const ImVec2 ground_br(curr_pos.x + mr_game.box_width, curr_pos.y + mr_game.box_height);
    draw_list->AddRectFilled(ground_tl, ground_br, ImColor(0.18f, 0.18f, 0.2f, 1.0f));
    draw_list->AddLine(ground_tl, ImVec2(ground_br.x, ground_tl.y),
                       ImColor(0.3f, 0.3f, 0.32f, 1.0f), 2.0f);

    for(const auto &rock : mr_ground_rocks){
        const float rx = static_cast<float>(curr_pos.x + rock.x);
        const float ry = static_cast<float>(curr_pos.y + mr_game.ground_y - rock.height);
        draw_list->AddCircleFilled(ImVec2(rx, ry),
                                   static_cast<float>(rock.size),
                                   ImColor(0.25f, 0.25f, 0.28f, 1.0f), 10);
    }

    for(const auto &ob : mr_obstacles){
        if(ob.type == mr_obstacle_type_t::crater){
            const ImVec2 center(curr_pos.x + static_cast<float>(ob.pos.x),
                                curr_pos.y + static_cast<float>(ob.pos.y));
            draw_list->AddCircleFilled(center, static_cast<float>(ob.width),
                                       ImColor(0.1f, 0.1f, 0.12f, 1.0f), 18);
            draw_list->AddCircle(center, static_cast<float>(ob.width * 0.65),
                                 ImColor(0.2f, 0.2f, 0.22f, 1.0f), 14, 2.0f);
        }else if(ob.type == mr_obstacle_type_t::aerial){
            const ImVec2 center(curr_pos.x + static_cast<float>(ob.pos.x),
                                curr_pos.y + static_cast<float>(ob.pos.y));
            if(ob.variant == mr_aerial_variant_t::alien){
                const ImVec2 dome(center.x, center.y - ob.height * 0.25);
                draw_list->AddCircleFilled(center, static_cast<float>(ob.width * 0.5),
                                           ImColor(0.55f, 0.65f, 0.75f, 1.0f), 20);
                draw_list->AddCircleFilled(dome, static_cast<float>(ob.width * 0.28),
                                           ImColor(0.75f, 0.85f, 0.95f, 0.9f), 16);
                draw_list->AddCircleFilled(ImVec2(center.x - ob.width * 0.15, center.y + ob.height * 0.05),
                                           static_cast<float>(ob.width * 0.06),
                                           ImColor(1.0f, 0.9f, 0.2f, 1.0f), 8);
                draw_list->AddCircleFilled(ImVec2(center.x + ob.width * 0.15, center.y + ob.height * 0.05),
                                           static_cast<float>(ob.width * 0.06),
                                           ImColor(1.0f, 0.9f, 0.2f, 1.0f), 8);
            }else{
                const ImVec2 tip(center.x + ob.width * 0.5, center.y);
                const ImVec2 tail1(center.x - ob.width * 0.5, center.y - ob.height * 0.4);
                const ImVec2 tail2(center.x - ob.width * 0.5, center.y + ob.height * 0.4);
                draw_list->AddTriangleFilled(tip, tail1, tail2,
                                             ImColor(0.95f, 0.5f, 0.2f, 1.0f));
                draw_list->AddLine(ImVec2(center.x - ob.width * 0.55, center.y),
                                   ImVec2(center.x - ob.width * 0.8, center.y + ob.height * 0.3),
                                   ImColor(0.95f, 0.7f, 0.4f, 0.8f), 2.0f);
            }
        }
    }

    const double runner_height = mr_game.is_crouching ? mr_game.runner_crouch_height : mr_game.runner_height;
    const double runner_base_y = mr_game.ground_y - mr_game.jump_height;
    const double runner_body_len = runner_height * 0.45;
    const double head_radius = runner_height * 0.13;
    const double leg_len = runner_height * 0.35;
    const double arm_len = runner_height * 0.28;
    const double hip_y = runner_base_y - 4.0;
    const double shoulder_y = hip_y - runner_body_len;
    const double head_center_y = shoulder_y - head_radius - 2.0;

    const ImVec2 hip(curr_pos.x + static_cast<float>(mr_game.runner_x),
                     curr_pos.y + static_cast<float>(hip_y));
    const ImVec2 shoulder(hip.x, curr_pos.y + static_cast<float>(shoulder_y));

    double leg_swing = std::sin(mr_game.run_phase) * 0.5;
    double arm_swing = std::sin(mr_game.run_phase + pi) * 0.4;
    if(mr_game.is_jumping){
        leg_swing = 0.2;
        arm_swing = -0.4;
    }else if(mr_game.is_crouching){
        leg_swing *= 0.4;
        arm_swing *= 0.5;
    }

    const auto make_leg = [&](double angle, ImU32 col){
        const ImVec2 foot(hip.x + std::cos(angle) * leg_len,
                          hip.y + std::sin(angle) * leg_len);
        draw_list->AddLine(hip, foot, col, 2.5f);
    };

    const auto make_arm = [&](double angle, ImU32 col){
        const ImVec2 hand(shoulder.x + std::cos(angle) * arm_len,
                          shoulder.y + std::sin(angle) * arm_len);
        draw_list->AddLine(shoulder, hand, col, 2.0f);
    };

    const ImU32 runner_col = ImColor(0.9f, 0.9f, 0.95f, 1.0f);
    const ImU32 runner_accent = ImColor(0.6f, 0.75f, 0.95f, 1.0f);

    draw_list->AddCircleFilled(ImVec2(shoulder.x, curr_pos.y + static_cast<float>(head_center_y)),
                               static_cast<float>(head_radius), runner_col, 16);
    draw_list->AddLine(hip, shoulder, runner_col, 2.5f);

    make_leg(pi / 2.0 + leg_swing, runner_col);
    make_leg(pi / 2.0 - leg_swing, runner_col);

    make_arm(-pi / 2.5 + arm_swing, runner_accent);
    make_arm(-pi / 2.5 - arm_swing, runner_accent);

    draw_list->AddCircleFilled(ImVec2(hip.x, hip.y + leg_len * 0.65),
                               static_cast<float>(mr_game.runner_width * 0.35),
                               ImColor(0.1f, 0.1f, 0.1f, 0.25f), 12);

    {
        std::stringstream ss;
        ss << "Score: " << mr_game.score << "  Speed: " << static_cast<int>(mr_game.scroll_speed);
        const ImVec2 text_pos(curr_pos.x + 10, curr_pos.y + 10);
        draw_list->AddText(text_pos, ImColor(0.95f, 0.95f, 0.98f, 1.0f), ss.str().c_str());
    }

    if(mr_game.game_over){
        const auto game_over_text = "GAME OVER! Press R to reset";
        const auto text_size = ImGui::CalcTextSize(game_over_text);
        const ImVec2 text_pos(curr_pos.x + mr_game.box_width / 2.0 - text_size.x / 2.0,
                              curr_pos.y + mr_game.box_height / 2.0 - text_size.y / 2.0);
        draw_list->AddText(text_pos, ImColor(1.0f, 0.2f, 0.2f, 1.0f), game_over_text);
    }

    draw_list->AddRect(curr_pos, box_br, ImColor(0.5f, 0.5f, 0.55f, 1.0f));

    t_mr_updated = t_now;

    ImGui::Dummy(ImVec2(mr_game.box_width, mr_game.box_height));
    ImGui::End();
    return true;
}
