// Encompass.cc - A part of DICOMautomaton 2026. Written by hal clark.

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

#include "Encompass.h"

EncompassGame::EncompassGame(){
    en_game.re.seed( std::random_device()() );
    Reset();
}

void EncompassGame::Reset(){
    en_game_objs.clear();

    // First, generate radii according to some distribution.
    std::vector<double> radii(en_game.N_objs, 1.0);
    {
        double dof = 3.0;
        std::chi_squared_distribution<> rd(dof);
        for(auto &r : radii){
            r = rd(en_game.re);
        }
        std::sort( std::begin(radii), std::end(radii) );

        // Rescale so all are [min_radius, max_radius].
        const auto curr_min = radii.front();
        const auto curr_max = radii.back();
        for(auto &r : radii){
            const auto clamped = (r - curr_min) / (curr_max - curr_min);
            r = en_game.min_radius + (en_game.max_radius - en_game.min_radius) * clamped;
        }
    }

    // Then generate placements and momentums, starting with the largest objects.
    std::reverse( std::begin(radii), std::end(radii) );

    const auto intersects_existing = [&]( const vec2<double> &pos, double rad ) -> bool {
        bool intersection = false;
        for(const auto& obj : en_game_objs){
            const auto sep = pos.distance(obj.pos);
            const auto min = rad + obj.rad;
            if(sep <= min){
                intersection = true;
                break;
            }
        }
        return intersection;
    };

    const auto intersects_wall = [&]( const vec2<double> &pos, double rad ) -> bool {
        return (pos.x <= (0.0 + rad))
            || ((en_game.box_width - rad) <= pos.x)
            || (pos.y <= (0.0 + rad))
            || ((en_game.box_height - rad) <= pos.y);
    };

    std::uniform_real_distribution<> rd_x(0.0, en_game.box_width);
    std::uniform_real_distribution<> rd_y(0.0, en_game.box_height);
    std::uniform_real_distribution<> rd_v(-0.05 * en_game.max_speed, 0.05 * en_game.max_speed);
    //auto f_x = std::bind(rd_x,re);
    //auto f_y = std::bind(rd_y,re);
    //auto f_v = std::bind(rd_v,re);
    for(const auto &r : radii){
        int64_t i = 100L;
        while(true){
            const vec2<double> pos( rd_x(en_game.re), rd_y(en_game.re) );
            const vec2<double> vel( rd_v(en_game.re), rd_v(en_game.re) );
            const auto rad = r;
//std::cout << "Attempting to place object with pos, vel, rad = " << pos << ", " << vel << ", " << rad << std::endl;
            if( !intersects_wall(pos, rad)
            &&  !intersects_existing(pos, rad) ){
                en_game_objs.emplace_back();
//std::cout << "Successfully placed." << std::endl;
                en_game_objs.back().pos = pos;
                en_game_objs.back().vel = vel;
                en_game_objs.back().rad = rad;
                en_game_objs.back().player_controlled = false;
                break;
            }
            if(--i < 0L){
//std::cout << "Unable to place." << std::endl;

                YLOGWARN("Unable to place object after 100 attempts. Ignoring object");
                break;
            }
        }
    }
    
    // Select one object to be under player control.
    {
        auto n = static_cast<int64_t>( std::round(static_cast<float>(en_game_objs.size()) * 0.75) );
        n = std::clamp<int64_t>(n, 0L, en_game_objs.size()-1L);
        en_game_objs.at(n).player_controlled = true;
    }


    // Reset the update time.
    const auto t_now = std::chrono::steady_clock::now();
    t_en_updated = t_now;
    t_en_started = t_now;

    return;
}

bool EncompassGame::Display(bool &enabled){
    if( !enabled ) return true;

    const auto pi = std::acos(-1.0);

    const auto win_width  = static_cast<int>( std::ceil(en_game.box_width) ) + 15;
    const auto win_height = static_cast<int>( std::ceil(en_game.box_height) ) + 40;
    auto flags = ImGuiWindowFlags_AlwaysAutoResize
               | ImGuiWindowFlags_NoScrollWithMouse
               | ImGuiWindowFlags_NoNavInputs
               | ImGuiWindowFlags_NoScrollbar ;
    ImGui::SetNextWindowSize(ImVec2(win_width, win_height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::Begin("Encompass", &enabled, flags );

    const auto f = ImGui::IsWindowFocused();

    // Reset the game before any game state is used.
    if( f && ImGui::IsKeyPressed(SDL_SCANCODE_R) ){
        Reset();
    }

            //const auto intersects_horiz_wall = [&]( const vec2<double> &pos, double rad ) -> bool {
            //    return (pos.y <= (0.0 + rad))
            //        || ((en_game.box_height - rad) <= pos.y);
            //};
            //const auto intersects_vert_wall = [&]( const vec2<double> &pos, double rad ) -> bool {
            //    return (pos.x <= (0.0 + rad))
            //        || ((en_game.box_width - rad) <= pos.x);
            //};

    const auto rad_to_area = [&](double rad){
        return pi * std::pow(rad, 2.0);
    };

    const auto intersects_existing = [&]( const vec2<double> &pos, double rad ) -> bool {
        bool intersection = false;
        for(const auto& obj : en_game_objs){
            const auto sep = pos.distance(obj.pos);
            const auto min = rad + obj.rad;
            if(sep <= min){
                intersection = true;
                break;
            }
        }
        return intersection;
    };

    const auto intersects_wall = [&]( const vec2<double> &pos, double rad ) -> bool {
        return (pos.x <= (0.0 + rad))
            || ((en_game.box_width - rad) <= pos.x)
            || (pos.y <= (0.0 + rad))
            || ((en_game.box_height - rad) <= pos.y);
    };

    const auto obj_intersections = [&](size_t j){
        std::vector<size_t> ints;
        const auto& obj_j = en_game_objs[j];
        for(size_t i = 0UL; i < j; ++i){
            const auto& obj_i = en_game_objs[i];
            const auto sep = obj_j.pos.distance( obj_i.pos );
            const auto min = obj_j.rad + obj_i.rad;
            if(sep <= min){
                ints.emplace_back(i);
            }
        }
        return ints;
    };
                            
    const auto attempt_to_shed = [&]( en_game_obj_t &obj,
                                      const vec2<double> &dir,
                                      double radius,
                                      decltype(en_game_objs) &l_en_game_objs ) -> bool {
        bool shed_successfully = false;
        do{
            const auto l_dir = dir.unit();
            const auto l_rad = radius;

            const auto surplus_sq_rad = std::pow(obj.rad, 2.0) - std::pow(l_rad, 2.0);
            if(surplus_sq_rad <= std::pow(en_game.min_radius, 2.0)) break;

            const auto surplus_rad = std::sqrt( surplus_sq_rad );
            if(surplus_rad < en_game.min_radius) break;
            const auto l_obj_remaining_rad = surplus_rad;

            // Should ideally do this, but then it will usually collide with the not-yet-shrunk 'obj'.
            // So this will need support from the collision check.
            //const auto l_pos = obj.pos + l_dir * (l_obj_remaining_rad + l_rad + 1.0);
            // instead, we just use the existing not-yet-shrunk radius.
            const auto l_pos = obj.pos + l_dir * (obj.rad + l_rad + 1.0);
            const auto l_vel = l_dir * en_game.max_speed;

            if( !intersects_wall(l_pos, l_rad)
            &&  !intersects_existing(l_pos, l_rad) ){

                l_en_game_objs.emplace_back();
                l_en_game_objs.back().pos = l_pos;
                l_en_game_objs.back().vel = l_vel;
                l_en_game_objs.back().rad = l_rad;
                l_en_game_objs.back().player_controlled = false;

                const auto orig_area = pi * std::pow(obj.rad, 2.0);
                const auto new_area  = pi * std::pow(l_obj_remaining_rad, 2.0);
                const auto orig_area_shed = 0.0;
                const auto new_area_shed  = pi * std::pow(l_rad, 2.0);

                //const auto d_area_i = pi * (std::pow(new_i_rad, 2.0) - std::pow(obj_i.rad, 2.0));
                //const auto d_area_j = pi * (std::pow(new_j_rad, 2.0) - std::pow(obj_j.rad, 2.0));
                //std::cout << "Radius transfer: " << (new_i_rad - obj_i.rad) << " vs " << (new_j_rad - obj_j.rad) << std::endl;
                //std::cout << "  (Transfer of mass: " << d_area_i << " vs " << d_area_j << ")" << std::endl;

                obj.vel = (obj.vel * orig_area - l_vel * new_area_shed) / (orig_area - new_area_shed);
                obj.rad = l_obj_remaining_rad;
                shed_successfully = true;
            }
        }while(false);
        return shed_successfully;
    };

    // Display.
    ImVec2 curr_pos = ImGui::GetCursorScreenPos();
    //ImVec2 window_extent = ImGui::GetContentRegionAvail();
    ImDrawList *window_draw_list = ImGui::GetWindowDrawList();

    {
        const auto c = ImColor(0.7f, 0.7f, 0.8f, 1.0f);
        window_draw_list->AddRect(curr_pos, ImVec2( curr_pos.x + en_game.box_width,
                                                    curr_pos.y + en_game.box_height ), c);
    }

    const auto t_now = std::chrono::steady_clock::now();
    const auto t_started_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_en_started).count();
    auto t_updated_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_en_updated).count();

    // Limit individual time steps to around 30 fps otherwise 'infinitesimal' updates to the system will no
    // longer be small, and the simulation will quickly break down.
    //
    // Note that this will cause the simulation to be choppy if the frame rate falls below 30 fps or so.
    if(30 < t_updated_diff) t_updated_diff = 30;

    decltype(en_game_objs) l_en_game_objs;
    for(auto &obj : en_game_objs){
        ImVec2 obj_pos = curr_pos;
        obj_pos.x = curr_pos.x + obj.pos.x;
        obj_pos.y = curr_pos.y + obj.pos.y;

        const auto rel_r = std::clamp<double>(obj.rad / 30.0, 0.0, 1.0);
        auto c = ImColor(rel_r * 1.0f, (1.0f - rel_r) * 1.0f, 0.5f, 1.0f);
        if(obj.player_controlled){
            c = ImColor(1.0f, 1.0f, 0.1f, 1.0f);
        }
        window_draw_list->AddCircle(obj_pos, obj.rad, c);

        // Implement player controls.
        if( f && obj.player_controlled){
            if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_LeftArrow)) ){
                obj.vel.x -= 1.0;
            }
            if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_RightArrow)) ){
                obj.vel.x += 1.0;
            }
            if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_UpArrow)) ){
                obj.vel.y -= 1.0;
            }
            if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_DownArrow)) ){
                obj.vel.y += 1.0;
            }
            if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_Space)) ){
                // Jettison a small object in the direction opposite of travel.
                auto l_rad = obj.rad * 0.05;
                l_rad = (l_rad < en_game.min_radius) ? en_game.min_radius : l_rad;
                auto l_dir = obj.vel * (-1.0);
                l_dir = ( 0.0 < l_dir.length() ) ? l_dir : vec2<double>(1.0, 0.0);

                attempt_to_shed( obj, l_dir, l_rad, l_en_game_objs);
            }
            if( ImGui::IsKeyPressed(SDL_SCANCODE_S) ){
                // Attempt to split into two.
                auto l_rad = std::sqrt(0.5) * obj.rad;
                auto l_dir = obj.vel * (-1.0);
                l_dir = ( 0.0 < l_dir.length() ) ? l_dir : vec2<double>(1.0, 0.0);

                decltype(en_game_objs) ll_en_game_objs;
                attempt_to_shed( obj, l_dir, l_rad, ll_en_game_objs);
                for(auto& obj : ll_en_game_objs){
                    obj.player_controlled = true;
                }
                l_en_game_objs.insert( std::begin(l_en_game_objs),
                                       std::begin(ll_en_game_objs), std::end(ll_en_game_objs) );
            }
        }

        // Limit the maximum speed.
        {
            const auto speed = obj.vel.length();
            if(en_game.max_speed < speed){
                const auto dir = obj.vel.unit();
                obj.vel = dir * en_game.max_speed;
            }
        }
    }
    en_game_objs.insert( std::begin(en_game_objs),
                         std::begin(l_en_game_objs), std::end(l_en_game_objs) );
    l_en_game_objs.clear();

    // Sort so larger objects are first.
    std::sort( std::begin(en_game_objs), std::end(en_game_objs),
               [](const en_game_obj_t &l, const en_game_obj_t &r) -> bool {
                   return (l.rad > r.rad);
               } );

    std::vector< vec2<double> > transfer_events;

    // Update the system.
    const auto N_objs = en_game_objs.size();
    for(size_t i = 0UL; i < N_objs; ++i){
        auto &obj_i = en_game_objs[i];
        bool should_move_to_cand_pos = true;
        const auto cand_pos = obj_i.pos + obj_i.vel * (static_cast<double>(t_updated_diff) / 1000.0);

        // Check for intersections with the wall.
        const bool cand_int_l_wall = (obj_i.pos.x <= (0.0 + obj_i.rad));
        const bool cand_int_r_wall = ((en_game.box_width - obj_i.rad) <= obj_i.pos.x);
        const bool cand_int_b_wall = (obj_i.pos.y <= (0.0 + obj_i.rad));
        const bool cand_int_t_wall = ((en_game.box_height - obj_i.rad) <= obj_i.pos.y);

        if(cand_int_l_wall) obj_i.vel.x =  std::abs(obj_i.vel.x);
        if(cand_int_r_wall) obj_i.vel.x = -std::abs(obj_i.vel.x);
        if(cand_int_b_wall) obj_i.vel.y =  std::abs(obj_i.vel.y);
        if(cand_int_t_wall) obj_i.vel.y = -std::abs(obj_i.vel.y);


        // Check for intersections with any of the other objects with updated positions.
        //
        // If none, then simulate spontaneous single-object events.
        const auto cand_int_objs  = obj_intersections(i);
        if(cand_int_objs.empty()){

            // Make large objects slowly disintegrate, 'leaking' a small amount of area in a mutiny event.
            //
            // Leaking is a spontaneous event with an associated probabality. The occurrence and amount of mass
            // lost are proportional to object's current area.
            //
            // Since this will be evaluated each frame, we need to scale the likelihood of each individual
            // evaluation so that the joint likelihood is as expected.

            //const auto period = 100.0; //milliseconds.
            const auto period = en_game.mutiny_period;
            const auto time_slice = static_cast<double>(t_updated_diff);
            std::uniform_real_distribution<> rd_t(0.0, period);
            const bool time_slice_selected = (rd_t(en_game.re) <= time_slice);

            const auto x = rad_to_area(obj_i.rad);
            //const auto mid = rad_to_area(20.0 * en_game.min_radius);
            //const auto slope = 1.0 / rad_to_area(5.0 * en_game.min_radius);
            const auto mid = rad_to_area(en_game.mutiny_mid);
            const auto slope = 1.0 / rad_to_area(en_game.mutiny_slope);
            const auto asympt_true = 1.0 / (1.0 + std::exp(-slope * (x - mid))); // logistic function = soft.
            std::bernoulli_distribution bd(asympt_true);
            const bool spontaneously_activated = bd(en_game.re);

            const bool try_shed = time_slice_selected && spontaneously_activated;

            if( ((5.0 * en_game.min_radius) < obj_i.rad)
            &&  try_shed ){
                const auto l_dir = vec2<double>(1.0, 0.0).rotate_around_z( rd_t(en_game.re) );
                auto l_rad = obj_i.rad * 0.05;
                l_rad = (l_rad < en_game.min_radius) ? en_game.min_radius : l_rad;

                const bool shed_successfully = attempt_to_shed( obj_i, l_dir, l_rad, l_en_game_objs);
                should_move_to_cand_pos = !shed_successfully;
            }

        // If one or more interesctions are expected, implement mass transfer or scatter or something.
        //
        // Because larger objects are first, object intersections here cause the 'i'th object
        // to transfer mass to the larger object.
        }else{
            for(auto &j : cand_int_objs){
                auto& obj_j = en_game_objs[j];
                const auto sep = obj_j.pos.distance( obj_i.pos );
                const auto min = obj_j.rad + obj_i.rad;
                if( (sep < min)
                &&  (obj_i.rad <= obj_j.rad) ){
                    // Attempt to consume enough radius so the objects are no longer overlapping.
                    double new_i_rad = obj_i.rad - (min - sep);
                    new_i_rad = std::clamp(new_i_rad, 0.0, 1.0E6);

                    // If the smaller would end up below the minimum radius, then consume the entire object.
                    if(new_i_rad < en_game.min_radius) new_i_rad = 0.0;

                    // Transfer the area to the larger object.
                    double new_j_rad = std::sqrt( std::pow(obj_j.rad, 2.0)
                                                + std::pow(obj_i.rad, 2.0)
                                                - std::pow(new_i_rad, 2.0) );
                    // If the larger object will grow beyond the bounds, reduce the amount transferred.
                    const auto max_new_j_rad_wall = std::max<double>( obj_j.rad,
                                                           std::min<double>({ obj_j.pos.x,
                                                                              obj_j.pos.y,
                                                                              en_game.box_width - obj_j.pos.x,
                                                                              en_game.box_height - obj_j.pos.y }) );
                    // Also determine whether expansion is limited by another nearby (larger) object.
                    auto max_new_j_rad_obj = new_j_rad;
                    for(size_t k = 0UL; k < j; ++k){
                        const auto& obj_k = en_game_objs[k];
                        const auto sep = obj_j.pos.distance( obj_k.pos );
                        const auto surplus = sep - obj_j.rad;
                        if(max_new_j_rad_obj < surplus) max_new_j_rad_obj = surplus;
                    }

                    const bool growth_constrained =    (max_new_j_rad_wall < new_j_rad)
                                                    || (max_new_j_rad_obj < new_j_rad);
                    if(growth_constrained){
                        should_move_to_cand_pos = false;

                        // Instead of kinematics, try 'shedding' the excess mass where it can be placed randomly.
                        // You can make relatively small objects for this to increase the likelihood of
                        // successful placement.
                        const auto can_shed = ((sqrt(2.0) * en_game.min_radius) < obj_j.rad);
                        if(can_shed){
                            std::uniform_real_distribution<> rd_t(0.0, pi*2.0);
                            {
                                int64_t iter = 100L;
                                while(true){
                                    const auto l_dir = vec2<double>(1.0, 0.0).rotate_around_z( rd_t(en_game.re) );
                                    const auto l_rad = en_game.min_radius;
                                    const bool shed_successfully = attempt_to_shed( obj_j, l_dir, l_rad, l_en_game_objs);

                                    if( shed_successfully ){
                                        break;
                                    }
                                    if(--iter < 0L){
                                        //YLOGWARN("Unable to place shed object after 100 attempts. Ignoring object");
                                        break;
                                    }
                                }
                            }
                            
                            // Make the object halt.
                            obj_j.vel = vec2<double>(0.0, 0.0);
                        }



/*
                        // Make the objects bounce apart using 2D hard sphere elastic kinematics.
                        const auto rel_pos = obj_i.pos - obj_j.pos;
                        const auto rel_vel = obj_i.vel - obj_j.vel;
                        const auto inv_mass_i = 1.0; //(1.0 / (pi * std::pow(obj_i.rad, 2.0)));
                        const auto inv_mass_j = 1.0; //(1.0 / (pi * std::pow(obj_j.rad, 2.0)));
                        
                        const auto t_a = rel_pos.Dot(rel_vel);
                        const auto t_c = rel_vel.Dot(rel_vel);
                        const auto t_d = rel_pos.Dot(rel_pos);

                        //const auto t_e = t_b - (rel_pos.Dot(rel_pos) - std::pow(obj_i.rad + obj_j.rad, 2.0));
                        const auto t_e = std::max<double>(0.0, t_a * t_a - t_c * (t_d - std::pow(obj_i.rad + obj_j.rad, 2.0)));
                        //if(t_e < 0.0) continue;
                        auto t = -(t_a + std::sqrt(t_e))/t_c;
                        t = (std::isfinite(t) && (0.0 < t)) ? t : 0.0;
                        
                        const auto impulse_dir = (rel_pos + rel_vel * t).unit();
                        const auto impulse = impulse_dir * 2.0 * impulse_dir.Dot(rel_vel) / (inv_mass_i + inv_mass_j);
                        const auto new_obj_i_vel = obj_i.vel + impulse * inv_mass_i;
                        const auto new_obj_j_vel = obj_j.vel - impulse * inv_mass_j;

//std::cout << "Collision detected between " << obj_i.pos << " and " << obj_j.pos
//          << " with velocities changing from " << obj_i.vel << " to " << new_obj_vel
//          << " and " << obj_j.vel << " to " << new_obj_j_vel << std::endl;

                        obj_i.vel = new_obj_i_vel;
                        obj_j.vel = new_obj_j_vel;
*/
                    }else{
                        const auto dir = (obj_j.pos - obj_i.pos).unit();
                        transfer_events.emplace_back( obj_i.pos + dir * obj_i.rad );

                        const auto orig_area_i = pi * std::pow(obj_i.rad, 2.0);
                        const auto new_area_i  = pi * std::pow(new_i_rad, 2.0);
                        const auto orig_area_j = pi * std::pow(obj_j.rad, 2.0);
                        const auto new_area_j  = pi * std::pow(new_j_rad, 2.0);

                        const auto d_area_i = pi * (std::pow(new_i_rad, 2.0) - std::pow(obj_i.rad, 2.0));
                        const auto d_area_j = pi * (std::pow(new_j_rad, 2.0) - std::pow(obj_j.rad, 2.0));
                        //std::cout << "Radius transfer: " << (new_i_rad - obj_i.rad) << " vs " << (new_j_rad - obj_j.rad) << std::endl;
                        //std::cout << "  (Transfer of mass: " << d_area_i << " vs " << d_area_j << ")" << std::endl;

                        obj_i.rad = new_i_rad;
                        obj_j.rad = new_j_rad;

                        obj_j.vel = (obj_j.vel * orig_area_j + obj_i.vel * d_area_j) / (orig_area_j + d_area_j);
                    }
                }
            }
        }
            

/*
            // Move this object, and adjust the other object(s) velocities.
            for(auto &j : cand_int_objs){
                auto& obj_j = en_game_objs[j];

                // 2D hard sphere elastic kinematics.
                const auto rel_pos = obj.pos - obj_j.pos;
                const auto rel_vel = obj.vel - obj_j.vel;
                const auto inv_mass_a = (1.0 / (pi * std::pow(obj.rad, 2.0)));
                const auto inv_mass_b = (1.0 / (pi * std::pow(obj_j.rad, 2.0)));
                
                const auto t_a = rel_pos.Dot(rel_vel);
                const auto t_c = rel_vel.Dot(rel_vel);
                const auto t_d = rel_pos.Dot(rel_pos);

                //const auto t_e = t_b - (rel_pos.Dot(rel_pos) - std::pow(obj.rad + obj_j.rad, 2.0));
                const auto t_e = t_a * t_a - t_c * (t_d - std::pow(obj.rad + obj_j.rad, 2.0));
                if(t_e < 0.0) continue;
                const auto t = -(t_a + std::sqrt(t_e))/t_c;
                if( !std::isfinite(t)
                ||  (t < 0.0) ) continue;
                
                const auto impulse_dir = (rel_pos + rel_vel * t).unit();
                const auto impulse = impulse_dir * 2.0 * impulse_dir.Dot(rel_vel) / (inv_mass_a + inv_mass_b);
                const auto new_obj_vel = obj.vel + impulse * inv_mass_a;
                const auto new_obj_j_vel = obj_j.vel - impulse * inv_mass_b;

std::cout << "Collision detected between " << obj.pos << " and " << obj_j.pos
  << " with velocities changing from " << obj.vel << " to " << new_obj_vel
  << " and " << obj_j.vel << " to " << new_obj_j_vel << std::endl;

                obj.vel = new_obj_vel;
                obj_j.vel = new_obj_j_vel;
            }
*/

        // Move to candidate position.
        if(should_move_to_cand_pos){
            obj_i.pos = cand_pos;
        }

        // Slowly move toward smaller objects and away from larger objects.
        if(!obj_i.player_controlled){
            const auto max_dist_between = std::hypot(en_game.box_width, en_game.box_height);
            const auto time_scale = static_cast<double>(t_updated_diff) / 5000.0;
            struct nudge_t {
                double intensity = 0.0;
                double repulsion_factor = 0.0;
                vec2<double> dir = vec2<double>(0.0, 0.0);
            };
            size_t N_nudges = 3;
            std::vector<nudge_t> nudges;

            for(size_t j = 0UL; j < N_objs; ++j){
                if(i == j) continue;
                const auto &obj_j = en_game_objs[j];

                const auto repulsion_factor = (obj_j.rad < obj_i.rad) ? 1.0 : -1.0;

                const auto rel_pos = (obj_j.pos - obj_i.pos);
                auto dir = rel_pos.unit();
                const auto dist_between = rel_pos.length() - obj_i.rad - obj_j.rad;
                const auto intensity_dist = std::pow( (max_dist_between - dist_between) / max_dist_between, 2.0 );

                // If 'i' is larger, prefer larger 'prey' even if they are slightly further away.
                // If 'j' is larger, run away from the closest object large enough to encompass you.
                const bool is_prey = (obj_i.rad < obj_j.rad);
                const auto intensity_mass = is_prey ? 1.0
                                                    : std::pow( obj_j.rad / obj_i.rad, 1.5 );
                if(is_prey){
                    dir = dir.rotate_around_z( pi*0.15 );
                }
                const auto intensity = intensity_dist * intensity_mass;

                nudges.emplace_back();
                nudges.back().intensity = intensity;
                nudges.back().repulsion_factor = repulsion_factor;
                nudges.back().dir = dir;

                std::sort( std::begin(nudges), std::end(nudges),
                           [](const nudge_t &l, const nudge_t &r) -> bool {
                               return std::abs(l.intensity) > std::abs(r.intensity);
                           } );

                if(N_nudges < nudges.size()) nudges.resize(N_nudges);
            }
            for(const auto &n : nudges){
                obj_i.vel += n.dir * en_game.max_speed * n.repulsion_factor * n.intensity * time_scale;
            }
        }

        // Limit the maximum speed.
        {
            const auto speed = obj_i.vel.length();
            if(en_game.max_speed < speed){
                const auto dir = obj_i.vel.unit();
                obj_i.vel = dir * en_game.max_speed;
            }
        }
    }
    t_en_updated = t_now;

    // Draw the transfer events.
    for(auto &p : transfer_events){
        ImVec2 obj_pos = curr_pos;
        obj_pos.x = curr_pos.x + p.x;
        obj_pos.y = curr_pos.y + p.y;

        auto c = ImColor(1.0f, 0.0f, 0.0f, 1.0f);
        window_draw_list->AddCircle(obj_pos, 1.0, c);
     }

    // Draw the velocity vectors.
    for(size_t i = 0UL; i < N_objs; ++i){
        auto &obj_i = en_game_objs[i];

        const auto obj_pos  = ImVec2(curr_pos.x + obj_i.pos.x, curr_pos.y + obj_i.pos.y);
        const auto vec_term = ImVec2(curr_pos.x + obj_i.pos.x + obj_i.vel.x, curr_pos.y + obj_i.pos.y + obj_i.vel.y);
        auto c = ImColor(1.0f, 0.0f, 0.0f, 1.0f);
        window_draw_list->AddLine(obj_pos, vec_term, c);
        //IMGUI_API void  AddTriangle(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness = 1.0f);
     }

    // Include the newly-created objects.
    en_game_objs.insert( std::begin(en_game_objs),
                         std::begin(l_en_game_objs), std::end(l_en_game_objs) );
    l_en_game_objs.clear();

    // Remove objects with a small radius.
    en_game_objs.erase( std::remove_if( std::begin(en_game_objs),
                                        std::end(en_game_objs),
                                        [&](const en_game_obj_t &obj) -> bool {
                                            return (obj.rad < en_game.min_radius);
                                        }),
                        std::end(en_game_objs) );


    ImGui::Dummy(ImVec2(en_game.box_width, en_game.box_height));
    ImGui::End();
    return true;
}

