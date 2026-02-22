// Sandwich.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <string>
#include <sstream>
#include <map>

#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"

#include <SDL.h>
#include "../imgui20210904/imgui.h"

#include "Sandwich.h"

SandwichGame::SandwichGame(){
    sw_game.re.seed( std::random_device()() );
    Reset();
}

void SandwichGame::Reset(){
    sw_game.round = 1;
    sw_game.total_score = 0;
    sw_game.round_score = 0;
    sw_game.phase = sw_game_phase_t::ShowOrder;
    sw_game.sandwich_layers.clear();
    sw_game.current_order.clear();
    sw_game.score_feedback.clear();
    sw_game.phase_timer = sw_game.order_display_time;
    sw_game.score_reveal_timer = 0.0;
    sw_game.feedback_items_shown = 0;
    
    const auto t_now = std::chrono::steady_clock::now();
    t_sw_updated = t_now;
    t_sw_started = t_now;
    
    sw_game.re.seed( std::random_device()() );
    
    GenerateOrder();
    return;
}

std::string SandwichGame::GetIngredientName(sw_ingredient_t ingredient){
    switch(ingredient){
        case sw_ingredient_t::BreadBottom: return "Bottom Bun";
        case sw_ingredient_t::BreadTop: return "Top Bun";
        case sw_ingredient_t::BeefPatty: return "Beef Patty";
        case sw_ingredient_t::ChickenPatty: return "Chicken Patty";
        case sw_ingredient_t::FishPatty: return "Fish Patty";
        case sw_ingredient_t::Cheddar: return "Cheddar";
        case sw_ingredient_t::Swiss: return "Swiss Cheese";
        case sw_ingredient_t::Lettuce: return "Lettuce";
        case sw_ingredient_t::Tomato: return "Tomato";
        case sw_ingredient_t::Onion: return "Onion";
        case sw_ingredient_t::Pickle: return "Pickle";
        case sw_ingredient_t::Ketchup: return "Ketchup";
        case sw_ingredient_t::Mustard: return "Mustard";
        case sw_ingredient_t::Mayo: return "Mayo";
        case sw_ingredient_t::Bacon: return "Bacon";
        case sw_ingredient_t::Egg: return "Egg";
        default: return "Unknown";
    }
}

ImColor SandwichGame::GetIngredientColor(sw_ingredient_t ingredient){
    switch(ingredient){
        case sw_ingredient_t::BreadBottom:
        case sw_ingredient_t::BreadTop: return ImColor(0.85f, 0.65f, 0.35f, 1.0f);  // Golden brown
        case sw_ingredient_t::BeefPatty: return ImColor(0.45f, 0.25f, 0.15f, 1.0f);  // Dark brown
        case sw_ingredient_t::ChickenPatty: return ImColor(0.90f, 0.75f, 0.55f, 1.0f); // Light tan
        case sw_ingredient_t::FishPatty: return ImColor(0.85f, 0.80f, 0.65f, 1.0f);   // Light golden
        case sw_ingredient_t::Cheddar: return ImColor(1.0f, 0.75f, 0.0f, 1.0f);     // Orange
        case sw_ingredient_t::Swiss: return ImColor(0.95f, 0.95f, 0.75f, 1.0f);     // Pale yellow
        case sw_ingredient_t::Lettuce: return ImColor(0.3f, 0.8f, 0.3f, 1.0f);       // Green
        case sw_ingredient_t::Tomato: return ImColor(0.9f, 0.2f, 0.15f, 1.0f);       // Red
        case sw_ingredient_t::Onion: return ImColor(0.95f, 0.9f, 0.95f, 1.0f);       // White/purple
        case sw_ingredient_t::Pickle: return ImColor(0.4f, 0.65f, 0.25f, 1.0f);      // Green
        case sw_ingredient_t::Ketchup: return ImColor(0.8f, 0.1f, 0.05f, 1.0f);      // Deep red
        case sw_ingredient_t::Mustard: return ImColor(0.9f, 0.8f, 0.1f, 1.0f);       // Yellow
        case sw_ingredient_t::Mayo: return ImColor(0.97f, 0.97f, 0.90f, 1.0f);       // Cream
        case sw_ingredient_t::Bacon: return ImColor(0.7f, 0.25f, 0.2f, 1.0f);        // Reddish brown
        case sw_ingredient_t::Egg: return ImColor(1.0f, 0.95f, 0.4f, 1.0f);          // Yellow
        default: return ImColor(0.5f, 0.5f, 0.5f, 1.0f);
    }
}

double SandwichGame::GetIngredientHeight(sw_ingredient_t ingredient){
    switch(ingredient){
        case sw_ingredient_t::BreadBottom: return 25.0;
        case sw_ingredient_t::BreadTop: return 30.0;
        case sw_ingredient_t::BeefPatty: return 20.0;
        case sw_ingredient_t::ChickenPatty: return 18.0;
        case sw_ingredient_t::FishPatty: return 16.0;
        case sw_ingredient_t::Cheddar:
        case sw_ingredient_t::Swiss: return 8.0;
        case sw_ingredient_t::Lettuce: return 12.0;
        case sw_ingredient_t::Tomato: return 10.0;
        case sw_ingredient_t::Onion: return 8.0;
        case sw_ingredient_t::Pickle: return 8.0;
        case sw_ingredient_t::Ketchup:
        case sw_ingredient_t::Mustard:
        case sw_ingredient_t::Mayo: return 6.0;
        case sw_ingredient_t::Bacon: return 10.0;
        case sw_ingredient_t::Egg: return 14.0;
        default: return 10.0;
    }
}

void SandwichGame::DrawIngredient(ImDrawList* draw_list, sw_ingredient_t ingredient,
                                   ImVec2 pos, double width, double height, double scale){
    const auto color = GetIngredientColor(ingredient);
    const auto scaled_width = width * scale;
    const auto scaled_height = height * scale;
    const auto x_offset = (width - scaled_width) / 2.0;
    
    ImVec2 p1(pos.x + x_offset, pos.y);
    ImVec2 p2(pos.x + x_offset + scaled_width, pos.y + scaled_height);
    
    switch(ingredient){
        case sw_ingredient_t::BreadTop:{
            // Draw rounded top bun
            const auto center_x = pos.x + width / 2.0;
            const auto radius = scaled_width / 2.0;
            draw_list->AddCircleFilled(ImVec2(center_x, pos.y + scaled_height), radius, color, 32);
            // Add sesame seeds as small circles
            const auto seed_color = ImColor(0.95f, 0.95f, 0.85f, 1.0f);
            for(int i = 0; i < 5; ++i){
                double seed_x = center_x - radius * 0.6 + (radius * 1.2 * i / 4.0);
                double seed_y = pos.y + scaled_height - radius * 0.3;
                draw_list->AddCircleFilled(ImVec2(seed_x, seed_y), 2.0f, seed_color);
            }
            break;
        }
        case sw_ingredient_t::BreadBottom:{
            // Draw flat bottom bun
            draw_list->AddRectFilled(p1, p2, color, 5.0f);
            break;
        }
        case sw_ingredient_t::BeefPatty:
        case sw_ingredient_t::ChickenPatty:
        case sw_ingredient_t::FishPatty:{
            // Draw patty with slight rounding
            draw_list->AddRectFilled(p1, p2, color, 3.0f);
            // Add grill marks
            const auto mark_color = ImColor(0.2f, 0.15f, 0.1f, 0.4f);
            for(int i = 0; i < 3; ++i){
                double mark_y = pos.y + scaled_height * 0.25 + (scaled_height * 0.5 * i / 2.0);
                draw_list->AddLine(ImVec2(p1.x + 5, mark_y), ImVec2(p2.x - 5, mark_y), mark_color, 2.0f);
            }
            break;
        }
        case sw_ingredient_t::Cheddar:
        case sw_ingredient_t::Swiss:{
            // Draw cheese slice
            draw_list->AddRectFilled(p1, p2, color);
            // Swiss has holes (darker color to show as holes)
            if(ingredient == sw_ingredient_t::Swiss){
                const auto hole_color = ImColor(0.7f, 0.7f, 0.5f, 1.0f);  // Slightly darker than cheese
                draw_list->AddCircleFilled(ImVec2(pos.x + width * 0.3, pos.y + scaled_height * 0.5), 3, hole_color);
                draw_list->AddCircleFilled(ImVec2(pos.x + width * 0.6, pos.y + scaled_height * 0.6), 2, hole_color);
            }
            break;
        }
        case sw_ingredient_t::Lettuce:{
            // Draw wavy lettuce
            const auto num_waves = 8;
            const auto wave_height = scaled_height * 0.4;
            std::vector<ImVec2> points;
            for(int i = 0; i <= num_waves; ++i){
                double x = p1.x + (scaled_width * i / num_waves);
                double y = pos.y + scaled_height / 2.0 + std::sin(i * 1.5) * wave_height;
                points.push_back(ImVec2(x, y));
            }
            for(int i = num_waves; i >= 0; --i){
                double x = p1.x + (scaled_width * i / num_waves);
                double y = pos.y + scaled_height / 2.0 + std::sin(i * 1.5 + 1.0) * wave_height + 4;
                points.push_back(ImVec2(x, y));
            }
            draw_list->AddConvexPolyFilled(points.data(), points.size(), color);
            break;
        }
        case sw_ingredient_t::Tomato:{
            // Draw tomato slice with seeds
            draw_list->AddRectFilled(p1, p2, color, 2.0f);
            const auto seed_color = ImColor(1.0f, 0.85f, 0.7f, 1.0f);
            draw_list->AddCircleFilled(ImVec2(pos.x + width * 0.35, pos.y + scaled_height * 0.5), 2, seed_color);
            draw_list->AddCircleFilled(ImVec2(pos.x + width * 0.65, pos.y + scaled_height * 0.5), 2, seed_color);
            break;
        }
        case sw_ingredient_t::Onion:{
            // Draw onion rings
            draw_list->AddRectFilled(p1, p2, color);
            const auto ring_color = ImColor(0.85f, 0.8f, 0.85f, 1.0f);
            draw_list->AddCircle(ImVec2(pos.x + width * 0.3, pos.y + scaled_height * 0.5), 4, ring_color);
            draw_list->AddCircle(ImVec2(pos.x + width * 0.7, pos.y + scaled_height * 0.5), 5, ring_color);
            break;
        }
        case sw_ingredient_t::Pickle:{
            // Draw pickle slices
            const auto num_pickles = 4;
            for(int i = 0; i < num_pickles; ++i){
                double px = p1.x + scaled_width * 0.1 + (scaled_width * 0.8 * i / (num_pickles - 1));
                draw_list->AddCircleFilled(ImVec2(px, pos.y + scaled_height * 0.5), scaled_height * 0.4, color);
            }
            break;
        }
        case sw_ingredient_t::Ketchup:
        case sw_ingredient_t::Mustard:
        case sw_ingredient_t::Mayo:{
            // Draw condiment squiggle
            std::vector<ImVec2> points;
            const auto num_points = 12;
            for(int i = 0; i <= num_points; ++i){
                double x = p1.x + (scaled_width * i / num_points);
                double y = pos.y + scaled_height / 2.0 + std::sin(i * 1.2) * (scaled_height * 0.3);
                points.push_back(ImVec2(x, y));
            }
            for(size_t i = 0; i + 1 < points.size(); ++i){
                draw_list->AddLine(points[i], points[i+1], color, 4.0f);
            }
            break;
        }
        case sw_ingredient_t::Bacon:{
            // Draw wavy bacon strips
            const auto strip_width = scaled_width / 3.0;
            for(int s = 0; s < 3; ++s){
                double sx = p1.x + strip_width * s + strip_width * 0.1;
                for(int i = 0; i < 4; ++i){
                    double y1 = pos.y + (scaled_height * i / 4.0);
                    double y2 = pos.y + (scaled_height * (i + 1) / 4.0);
                    double x_wave = (i % 2 == 0) ? 0 : strip_width * 0.2;
                    draw_list->AddRectFilled(ImVec2(sx + x_wave, y1), ImVec2(sx + strip_width * 0.8 + x_wave, y2), color);
                }
            }
            break;
        }
        case sw_ingredient_t::Egg:{
            // Draw fried egg using rectangles since AddEllipseFilled is not available
            const auto white_color = ImColor(0.98f, 0.98f, 0.95f, 1.0f);
            const auto yolk_color = ImColor(1.0f, 0.85f, 0.2f, 1.0f);
            // Draw egg white as a rounded rectangle
            ImVec2 white_p1(pos.x + width * 0.1, pos.y + scaled_height * 0.1);
            ImVec2 white_p2(pos.x + width * 0.9, pos.y + scaled_height * 0.9);
            draw_list->AddRectFilled(white_p1, white_p2, white_color, scaled_height * 0.3f);
            // Draw yolk as a circle
            draw_list->AddCircleFilled(ImVec2(pos.x + width / 2.0, pos.y + scaled_height / 2.0), 
                                        scaled_height * 0.25, yolk_color);
            break;
        }
        default:
            draw_list->AddRectFilled(p1, p2, color);
            break;
    }
}

void SandwichGame::GenerateOrder(){
    sw_game.current_order.clear();
    
    // Base order always includes bottom bread
    sw_order_requirement_t bottom_bun;
    bottom_bun.ingredient = sw_ingredient_t::BreadBottom;
    bottom_bun.count = 1;
    bottom_bun.is_special = false;
    sw_game.current_order.push_back(bottom_bun);
    
    // Determine complexity based on round
    const int round = sw_game.round;
    
    // Protein selection
    std::uniform_int_distribution<int> protein_dist(0, 2);
    sw_ingredient_t protein_types[] = {sw_ingredient_t::BeefPatty, sw_ingredient_t::ChickenPatty, sw_ingredient_t::FishPatty};
    sw_ingredient_t selected_protein = protein_types[protein_dist(sw_game.re)];
    
    sw_order_requirement_t protein;
    protein.ingredient = selected_protein;
    protein.count = 1;
    protein.is_special = false;
    
    // Round 3+: Might have double patty
    if(round >= 3){
        std::uniform_int_distribution<int> double_dist(0, 2);
        if(double_dist(sw_game.re) == 0){
            protein.count = 2;
            protein.is_special = true;
            protein.special_note = "double";
        }
    }
    sw_game.current_order.push_back(protein);
    
    // Cheese (round 2+)
    if(round >= 2){
        std::uniform_int_distribution<int> cheese_dist(0, 2);
        const int cheese_roll = cheese_dist(sw_game.re);
        if(cheese_roll != 0){  // 66% chance of cheese
            sw_order_requirement_t cheese;
            cheese.ingredient = (cheese_roll == 1) ? sw_ingredient_t::Cheddar : sw_ingredient_t::Swiss;
            cheese.count = 1;
            cheese.is_special = false;
            sw_game.current_order.push_back(cheese);
        }
    }
    
    // Vegetables (complexity increases with rounds)
    std::vector<sw_ingredient_t> veggies = {sw_ingredient_t::Lettuce, sw_ingredient_t::Tomato, 
                                             sw_ingredient_t::Onion, sw_ingredient_t::Pickle};
    int num_veggies = std::min(round, 4);
    std::shuffle(veggies.begin(), veggies.end(), sw_game.re);
    
    for(int i = 0; i < num_veggies; ++i){
        sw_order_requirement_t veg;
        veg.ingredient = veggies[i];
        veg.count = 1;
        veg.is_special = false;
        
        // Round 4+: "Hold" requests
        if(round >= 4){
            std::uniform_int_distribution<int> hold_dist(0, 4);
            if(hold_dist(sw_game.re) == 0){
                veg.count = 0;
                veg.is_special = true;
                veg.special_note = "hold";
            }
        }
        sw_game.current_order.push_back(veg);
    }
    
    // Condiments (round 2+)
    if(round >= 2){
        std::vector<sw_ingredient_t> condiments = {sw_ingredient_t::Ketchup, sw_ingredient_t::Mustard, sw_ingredient_t::Mayo};
        int num_condiments = std::min(round - 1, 3);
        std::shuffle(condiments.begin(), condiments.end(), sw_game.re);
        
        for(int i = 0; i < num_condiments; ++i){
            sw_order_requirement_t cond;
            cond.ingredient = condiments[i];
            cond.count = 1;
            cond.is_special = false;
            
            // Round 5+: "Extra" or "light" requests
            if(round >= 5){
                std::uniform_int_distribution<int> extra_dist(0, 3);
                int roll = extra_dist(sw_game.re);
                if(roll == 0){
                    cond.count = 2;
                    cond.is_special = true;
                    cond.special_note = "extra";
                } else if(roll == 1){
                    cond.count = 0;
                    cond.is_special = true;
                    cond.special_note = "hold";
                }
            }
            sw_game.current_order.push_back(cond);
        }
    }
    
    // Special items (round 6+)
    if(round >= 6){
        std::uniform_int_distribution<int> special_dist(0, 2);
        if(special_dist(sw_game.re) == 0){
            sw_order_requirement_t bacon;
            bacon.ingredient = sw_ingredient_t::Bacon;
            bacon.count = 1;
            bacon.is_special = false;
            sw_game.current_order.push_back(bacon);
        }
        if(special_dist(sw_game.re) == 0){
            sw_order_requirement_t egg;
            egg.ingredient = sw_ingredient_t::Egg;
            egg.count = 1;
            egg.is_special = false;
            sw_game.current_order.push_back(egg);
        }
    }
    
    // Top bread always at end
    sw_order_requirement_t top_bun;
    top_bun.ingredient = sw_ingredient_t::BreadTop;
    top_bun.count = 1;
    top_bun.is_special = false;
    sw_game.current_order.push_back(top_bun);
}

void SandwichGame::AddIngredient(sw_ingredient_t ingredient){
    sw_layer_t layer;
    layer.ingredient = ingredient;
    layer.scale = 0.0;
    layer.animating = true;
    layer.added_time = std::chrono::steady_clock::now();
    
    // Calculate target y position based on existing layers
    double total_height = 0.0;
    for(const auto& l : sw_game.sandwich_layers){
        total_height += GetIngredientHeight(l.ingredient);
    }
    layer.target_y = total_height;
    layer.y_offset = total_height + 50.0;  // Start above
    
    sw_game.sandwich_layers.push_back(layer);
}

void SandwichGame::CalculateScore(){
    sw_game.score_feedback.clear();
    sw_game.round_score = 0;
    
    // Count ingredients in the sandwich (excluding bread)
    std::map<sw_ingredient_t, int> sandwich_counts;
    for(const auto& layer : sw_game.sandwich_layers){
        sandwich_counts[layer.ingredient]++;
    }
    
    // Score each requirement
    int perfect_items = 0;
    int total_items = 0;
    
    for(const auto& req : sw_game.current_order){
        total_items++;
        int actual = sandwich_counts[req.ingredient];
        int expected = req.count;
        
        sw_score_feedback_t feedback;
        std::string ingredient_name = GetIngredientName(req.ingredient);
        
        if(actual == expected){
            // Perfect match
            if(expected == 0){
                feedback.message = "Correctly held " + ingredient_name + "!";
            } else if(req.is_special && req.special_note == "extra"){
                feedback.message = "Perfect extra " + ingredient_name + "!";
            } else if(req.is_special && req.special_note == "double"){
                feedback.message = "Perfect double " + ingredient_name + "!";
            } else {
                feedback.message = ingredient_name + ": Perfect!";
            }
            feedback.points = 100;
            feedback.color = ImColor(0.3f, 1.0f, 0.3f, 1.0f);
            perfect_items++;
        } else if(expected == 0 && actual > 0){
            // Should have held but added
            feedback.message = "Was supposed to hold " + ingredient_name + "!";
            feedback.points = -50;
            feedback.color = ImColor(1.0f, 0.3f, 0.3f, 1.0f);
        } else if(expected > 0 && actual == 0){
            // Forgot ingredient
            feedback.message = "Missing " + ingredient_name + "!";
            feedback.points = -75;
            feedback.color = ImColor(1.0f, 0.3f, 0.3f, 1.0f);
        } else if(actual > expected){
            // Too much
            if(req.ingredient == sw_ingredient_t::Mayo){
                feedback.message = "Too much mayo!";
            } else if(req.ingredient == sw_ingredient_t::Ketchup){
                feedback.message = "Too much ketchup!";
            } else if(req.ingredient == sw_ingredient_t::Mustard){
                feedback.message = "Too much mustard!";
            } else {
                feedback.message = "Too much " + ingredient_name + "!";
            }
            feedback.points = 25;  // Partial credit
            feedback.color = ImColor(1.0f, 0.8f, 0.3f, 1.0f);
        } else {
            // Too little (expected more)
            if(req.is_special && req.special_note == "extra"){
                feedback.message = "Needed MORE " + ingredient_name + "!";
            } else if(req.is_special && req.special_note == "double"){
                feedback.message = "Needed double " + ingredient_name + "!";
            } else {
                feedback.message = "Not enough " + ingredient_name + "!";
            }
            feedback.points = 25;  // Partial credit
            feedback.color = ImColor(1.0f, 0.8f, 0.3f, 1.0f);
        }
        
        sw_game.round_score += feedback.points;
        sw_game.score_feedback.push_back(feedback);
    }
    
    // Check for unwanted extras
    for(const auto& [ingredient, count] : sandwich_counts){
        bool was_ordered = false;
        for(const auto& req : sw_game.current_order){
            if(req.ingredient == ingredient){
                was_ordered = true;
                break;
            }
        }
        if(!was_ordered && count > 0){
            sw_score_feedback_t feedback;
            feedback.message = "Extra " + GetIngredientName(ingredient) + " not ordered!";
            feedback.points = -25;
            feedback.color = ImColor(1.0f, 0.5f, 0.3f, 1.0f);
            sw_game.round_score += feedback.points;
            sw_game.score_feedback.push_back(feedback);
        }
    }
    
    // Perfect bonus
    if(perfect_items == total_items){
        sw_score_feedback_t bonus;
        bonus.message = "PERFECT ORDER! Bonus!";
        bonus.points = 200;
        bonus.color = ImColor(1.0f, 0.9f, 0.2f, 1.0f);
        sw_game.round_score += bonus.points;
        sw_game.score_feedback.push_back(bonus);
    }
    
    // Speed bonus (if finished early)
    if(sw_game.phase_timer > 5.0){
        sw_score_feedback_t speed_bonus;
        speed_bonus.message = "Speed bonus! (" + std::to_string(static_cast<int>(sw_game.phase_timer)) + "s left)";
        speed_bonus.points = static_cast<int>(sw_game.phase_timer * 5);
        speed_bonus.color = ImColor(0.3f, 0.8f, 1.0f, 1.0f);
        sw_game.round_score += speed_bonus.points;
        sw_game.score_feedback.push_back(speed_bonus);
    }
    
    // Minimum score is 0
    if(sw_game.round_score < 0) sw_game.round_score = 0;
    
    sw_game.total_score += sw_game.round_score;
    if(sw_game.total_score > sw_game.high_score){
        sw_game.high_score = sw_game.total_score;
    }
}

void SandwichGame::StartNextRound(){
    sw_game.round++;
    sw_game.sandwich_layers.clear();
    sw_game.score_feedback.clear();
    sw_game.phase = sw_game_phase_t::ShowOrder;
    sw_game.phase_timer = sw_game.order_display_time;
    sw_game.score_reveal_timer = 0.0;
    sw_game.feedback_items_shown = 0;
    GenerateOrder();
}

void SandwichGame::StartBuildPhase(){
    sw_game.phase = sw_game_phase_t::Building;
    sw_game.phase_timer = sw_game.build_time;
    sw_game.sandwich_layers.clear();
}

void SandwichGame::DrawSandwich(ImDrawList* draw_list, ImVec2 base_pos, double dt){
    const double sandwich_width = 150.0;
    
    // Update animations
    for(auto& layer : sw_game.sandwich_layers){
        if(layer.animating){
            // Animate y position
            const double anim_speed = 200.0;
            if(layer.y_offset > layer.target_y){
                layer.y_offset -= anim_speed * dt;
                if(layer.y_offset < layer.target_y){
                    layer.y_offset = layer.target_y;
                }
            }
            // Animate scale
            const double scale_speed = 3.0;  // Scale units per second
            if(layer.scale < 1.0){
                layer.scale += scale_speed * dt;
                if(layer.scale > 1.0) layer.scale = 1.0;
            }
            // Check if animation complete
            if(std::abs(layer.y_offset - layer.target_y) < 0.1 && layer.scale >= 1.0){
                layer.animating = false;
            }
        }
    }
    
    // Draw layers from bottom to top
    for(const auto& layer : sw_game.sandwich_layers){
        double height = GetIngredientHeight(layer.ingredient);
        ImVec2 layer_pos(base_pos.x - sandwich_width / 2.0, base_pos.y - layer.y_offset - height);
        DrawIngredient(draw_list, layer.ingredient, layer_pos, sandwich_width, height, layer.scale);
    }
}

bool SandwichGame::Display(bool &enabled){
    if( !enabled ) return true;

    const auto win_width  = static_cast<int>( std::ceil(sw_game.box_width) ) + 15;
    const auto win_height = static_cast<int>( std::ceil(sw_game.box_height) ) + 60;
    auto flags = ImGuiWindowFlags_NoResize
               | ImGuiWindowFlags_NoScrollWithMouse
               | ImGuiWindowFlags_NoNavInputs
               | ImGuiWindowFlags_NoScrollbar ;
    ImGui::SetNextWindowSize(ImVec2(win_width, win_height), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::Begin("Sandwich Shop", &enabled, flags );

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
        const auto c = ImColor(0.5f, 0.35f, 0.2f, 1.0f);  // Brown border
        window_draw_list->AddRect(curr_pos, ImVec2( curr_pos.x + sw_game.box_width, 
                                                    curr_pos.y + sw_game.box_height ), c, 0.0f, 0, 2.0f);
    }

    const auto t_now = std::chrono::steady_clock::now();
    auto t_updated_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_sw_updated).count();
    
    // Limit time steps
    if(30 < t_updated_diff) t_updated_diff = 30;
    const auto dt = static_cast<double>(t_updated_diff) / 1000.0;

    // Update phase timer
    sw_game.phase_timer -= dt;

    // Draw title bar with round and score
    {
        std::stringstream ss;
        ss << "Round " << sw_game.round << "  |  Score: " << sw_game.total_score;
        ss << "  |  High Score: " << sw_game.high_score;
        const auto title_text = ss.str();
        const ImVec2 title_pos(curr_pos.x + 10, curr_pos.y + 5);
        window_draw_list->AddText(title_pos, ImColor(1.0f, 1.0f, 1.0f, 1.0f), title_text.c_str());
    }

    const double content_y = curr_pos.y + 30;

    // Handle different phases
    if(sw_game.phase == sw_game_phase_t::ShowOrder){
        // Show the order
        const auto header_text = "CUSTOMER ORDER:";
        ImVec2 header_pos(curr_pos.x + sw_game.box_width / 2.0 - 70, content_y + 20);
        window_draw_list->AddText(header_pos, ImColor(1.0f, 0.9f, 0.3f, 1.0f), header_text);
        
        double y_offset = content_y + 50;
        for(const auto& req : sw_game.current_order){
            std::stringstream ss;
            if(req.is_special){
                if(req.special_note == "hold"){
                    ss << "NO " << GetIngredientName(req.ingredient);
                } else if(req.special_note == "extra"){
                    ss << "EXTRA " << GetIngredientName(req.ingredient);
                } else if(req.special_note == "double"){
                    ss << "DOUBLE " << GetIngredientName(req.ingredient);
                }
            } else {
                if(req.count > 1){
                    ss << req.count << "x " << GetIngredientName(req.ingredient);
                } else {
                    ss << GetIngredientName(req.ingredient);
                }
            }
            
            ImColor text_color = req.is_special ? ImColor(1.0f, 0.5f, 0.2f, 1.0f) : ImColor(1.0f, 1.0f, 1.0f, 1.0f);
            ImVec2 item_pos(curr_pos.x + 50, y_offset);
            window_draw_list->AddText(item_pos, text_color, ss.str().c_str());
            y_offset += 25;
        }
        
        // Distracting animations on the right side (don't obscure the order)
        {
            const double anim_time = 5.0 - sw_game.phase_timer;  // Time elapsed since phase start
            const double distraction_x = curr_pos.x + sw_game.box_width * 0.65;
            const double distraction_y = content_y + 80;
            
            // Bouncing circles
            for(int i = 0; i < 5; ++i){
                const double phase_offset = i * 1.2;
                const double bounce = std::abs(std::sin((anim_time + phase_offset) * 3.0)) * 60.0;
                const double x = distraction_x + i * 35.0;
                const double y = distraction_y + 100 - bounce;
                const float hue = std::fmod((anim_time * 0.3f + i * 0.2f), 1.0f);
                ImColor color = ImColor::HSV(hue, 0.7f, 0.9f);
                window_draw_list->AddCircleFilled(ImVec2(x, y), 12.0f, color);
            }
            
            // Spinning rectangle
            {
                const double spin_cx = distraction_x + 85;
                const double spin_cy = distraction_y + 200;
                const double angle = anim_time * 2.5;
                const double size = 25.0;
                ImVec2 corners[4];
                const double pi = std::acos(-1.0);
                for(int i = 0; i < 4; ++i){
                    double corner_angle = angle + i * (pi / 2.0);
                    corners[i] = ImVec2(spin_cx + std::cos(corner_angle) * size,
                                        spin_cy + std::sin(corner_angle) * size);
                }
                ImColor spin_color = ImColor::HSV(std::fmod(anim_time * 0.5f, 1.0f), 0.6f, 0.8f);
                window_draw_list->AddQuadFilled(corners[0], corners[1], corners[2], corners[3], spin_color);
            }
            
            // Pulsating circles
            for(int i = 0; i < 3; ++i){
                const double pulse_x = distraction_x + 30 + i * 60;
                const double pulse_y = distraction_y + 300;
                const double pulse_phase = anim_time * 4.0 + i * 1.0;
                const double pulse_size = 8.0 + 8.0 * std::sin(pulse_phase);
                const float alpha = 0.4f + 0.4f * std::sin(pulse_phase);
                ImColor pulse_color = ImColor(0.8f, 0.5f, 0.9f, alpha);
                window_draw_list->AddCircleFilled(ImVec2(pulse_x, pulse_y), pulse_size, pulse_color);
            }
            
            // Zigzag line
            {
                const double zigzag_x = distraction_x;
                const double zigzag_y = distraction_y + 380;
                const int num_segments = 8;
                ImVec2 prev_point(zigzag_x, zigzag_y);
                for(int i = 1; i <= num_segments; ++i){
                    const double t = anim_time * 2.0 + i * 0.5;
                    const double zig = (i % 2 == 0) ? 15.0 : -15.0;
                    ImVec2 next_point(zigzag_x + i * 20, zigzag_y + zig * std::sin(t));
                    ImColor line_color = ImColor::HSV(std::fmod(t * 0.1f, 1.0f), 0.8f, 0.9f);
                    window_draw_list->AddLine(prev_point, next_point, line_color, 3.0f);
                    prev_point = next_point;
                }
            }
        }
        
        // Timer display
        {
            std::stringstream ss;
            ss << "Memorize! " << std::max(0, static_cast<int>(std::ceil(sw_game.phase_timer))) << "s";
            const auto timer_text = ss.str();
            ImVec2 timer_pos(curr_pos.x + sw_game.box_width / 2.0 - 50, curr_pos.y + sw_game.box_height - 40);
            
            // Pulsing effect
            float pulse = 0.7f + 0.3f * std::sin(sw_game.phase_timer * 6.0);
            window_draw_list->AddText(timer_pos, ImColor(1.0f, pulse, pulse, 1.0f), timer_text.c_str());
        }
        
        // Skip hint
        {
            const auto hint_text = "Press SPACE to start building";
            ImVec2 hint_pos(curr_pos.x + sw_game.box_width / 2.0 - 90, curr_pos.y + sw_game.box_height - 20);
            window_draw_list->AddText(hint_pos, ImColor(0.6f, 0.6f, 0.6f, 1.0f), hint_text);
        }
        
        // Check for phase transition
        if(sw_game.phase_timer <= 0 || (f && ImGui::IsKeyPressed(SDL_SCANCODE_SPACE))){
            StartBuildPhase();
        }
        
    } else if(sw_game.phase == sw_game_phase_t::Building){
        // Split screen: ingredients on left, sandwich on right
        const double split_x = sw_game.box_width * 0.4;
        
        // Draw ingredient buttons on the left
        ImGui::SetCursorScreenPos(ImVec2(curr_pos.x + 10, content_y + 10));
        ImGui::BeginGroup();
        
        // Bread
        ImGui::Text("Bread:");
        if(ImGui::Button("Bottom Bun")) AddIngredient(sw_ingredient_t::BreadBottom);
        ImGui::SameLine();
        if(ImGui::Button("Top Bun")) AddIngredient(sw_ingredient_t::BreadTop);
        
        // Proteins
        ImGui::Text("Proteins:");
        if(ImGui::Button("Beef")) AddIngredient(sw_ingredient_t::BeefPatty);
        ImGui::SameLine();
        if(ImGui::Button("Chicken")) AddIngredient(sw_ingredient_t::ChickenPatty);
        ImGui::SameLine();
        if(ImGui::Button("Fish")) AddIngredient(sw_ingredient_t::FishPatty);
        
        // Cheese
        ImGui::Text("Cheese:");
        if(ImGui::Button("Cheddar")) AddIngredient(sw_ingredient_t::Cheddar);
        ImGui::SameLine();
        if(ImGui::Button("Swiss")) AddIngredient(sw_ingredient_t::Swiss);
        
        // Vegetables
        ImGui::Text("Veggies:");
        if(ImGui::Button("Lettuce")) AddIngredient(sw_ingredient_t::Lettuce);
        ImGui::SameLine();
        if(ImGui::Button("Tomato")) AddIngredient(sw_ingredient_t::Tomato);
        if(ImGui::Button("Onion")) AddIngredient(sw_ingredient_t::Onion);
        ImGui::SameLine();
        if(ImGui::Button("Pickle")) AddIngredient(sw_ingredient_t::Pickle);
        
        // Condiments
        ImGui::Text("Condiments:");
        if(ImGui::Button("Ketchup")) AddIngredient(sw_ingredient_t::Ketchup);
        ImGui::SameLine();
        if(ImGui::Button("Mustard")) AddIngredient(sw_ingredient_t::Mustard);
        ImGui::SameLine();
        if(ImGui::Button("Mayo")) AddIngredient(sw_ingredient_t::Mayo);
        
        // Special
        ImGui::Text("Special:");
        if(ImGui::Button("Bacon")) AddIngredient(sw_ingredient_t::Bacon);
        ImGui::SameLine();
        if(ImGui::Button("Egg")) AddIngredient(sw_ingredient_t::Egg);
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        // Undo button
        if(ImGui::Button("Undo Last") && !sw_game.sandwich_layers.empty()){
            sw_game.sandwich_layers.pop_back();
        }
        ImGui::SameLine();
        if(ImGui::Button("Clear All")){
            sw_game.sandwich_layers.clear();
        }
        
        ImGui::Spacing();
        if(ImGui::Button("DONE - Submit Order")){
            CalculateScore();
            sw_game.phase = sw_game_phase_t::Scoring;
            sw_game.phase_timer = sw_game.scoring_display_time;
            sw_game.score_reveal_timer = 0.0;
            sw_game.feedback_items_shown = 0;
        }
        
        ImGui::EndGroup();
        
        // Draw sandwich preview on the right
        ImVec2 sandwich_base(curr_pos.x + split_x + (sw_game.box_width - split_x) / 2.0, 
                             curr_pos.y + sw_game.box_height - 60);
        DrawSandwich(window_draw_list, sandwich_base, dt);
        
        // Timer bar at bottom
        {
            double timer_pct = (sw_game.build_time > 0.0) 
                             ? std::clamp(sw_game.phase_timer / sw_game.build_time, 0.0, 1.0) 
                             : 0.0;
            ImVec2 bar_start(curr_pos.x + 10, curr_pos.y + sw_game.box_height - 25);
            ImVec2 bar_end(curr_pos.x + sw_game.box_width - 10, curr_pos.y + sw_game.box_height - 10);
            
            // Background
            window_draw_list->AddRectFilled(bar_start, bar_end, ImColor(0.2f, 0.2f, 0.2f, 1.0f), 3.0f);
            
            // Progress
            ImColor bar_color = (timer_pct > 0.3) ? ImColor(0.3f, 0.8f, 0.3f, 1.0f) : 
                                (timer_pct > 0.1) ? ImColor(0.9f, 0.7f, 0.2f, 1.0f) : 
                                                    ImColor(0.9f, 0.2f, 0.2f, 1.0f);
            ImVec2 progress_end(bar_start.x + (bar_end.x - bar_start.x) * timer_pct, bar_end.y);
            window_draw_list->AddRectFilled(bar_start, progress_end, bar_color, 3.0f);
            
            // Timer text
            std::stringstream ss;
            ss << static_cast<int>(std::ceil(sw_game.phase_timer)) << "s";
            ImVec2 timer_text_pos(curr_pos.x + sw_game.box_width / 2.0 - 10, bar_start.y - 2);
            window_draw_list->AddText(timer_text_pos, ImColor(1.0f, 1.0f, 1.0f, 1.0f), ss.str().c_str());
        }
        
        // Time's up
        if(sw_game.phase_timer <= 0){
            CalculateScore();
            sw_game.phase = sw_game_phase_t::Scoring;
            sw_game.phase_timer = sw_game.scoring_display_time;
            sw_game.score_reveal_timer = 0.0;
            sw_game.feedback_items_shown = 0;
        }
        
    } else if(sw_game.phase == sw_game_phase_t::Scoring){
        // Animated score reveal
        sw_game.score_reveal_timer += dt;
        
        const auto header_text = "ORDER SCORE:";
        ImVec2 header_pos(curr_pos.x + sw_game.box_width / 2.0 - 50, content_y + 10);
        window_draw_list->AddText(header_pos, ImColor(1.0f, 0.9f, 0.3f, 1.0f), header_text);
        
        // Reveal feedback items one at a time
        const double reveal_delay = 0.4;  // seconds between reveals
        int items_to_show = static_cast<int>(sw_game.score_reveal_timer / reveal_delay);
        items_to_show = std::min(items_to_show, static_cast<int>(sw_game.score_feedback.size()));
        
        double y_offset = content_y + 40;
        
        for(int i = 0; i < items_to_show; ++i){
            const auto& feedback = sw_game.score_feedback[i];
            
            // Calculate animation for this item
            double item_age = sw_game.score_reveal_timer - (i * reveal_delay);
            float alpha = std::min(1.0f, static_cast<float>(item_age * 2.0));
            
            // Draw feedback text
            std::stringstream ss;
            ss << feedback.message;
            if(feedback.points >= 0){
                ss << " +" << feedback.points;
            } else {
                ss << " " << feedback.points;
            }
            
            ImVec2 text_pos(curr_pos.x + 30, y_offset);
            
            ImColor color = feedback.color;
            color.Value.w = alpha;
            
            window_draw_list->AddText(text_pos, color, ss.str().c_str());
            
            y_offset += 28;
        }
        
        // Show total when all items revealed
        if(items_to_show >= static_cast<int>(sw_game.score_feedback.size())){
            ImGui::SetCursorScreenPos(ImVec2(curr_pos.x + 30, y_offset + 20));
            
            std::stringstream ss;
            ss << "Round Score: " << sw_game.round_score;
            ImVec2 total_pos(curr_pos.x + 30, y_offset + 10);
            window_draw_list->AddText(total_pos, ImColor(1.0f, 1.0f, 0.5f, 1.0f), ss.str().c_str());
            
            ss.str("");
            ss << "Total Score: " << sw_game.total_score;
            ImVec2 total_pos2(curr_pos.x + 30, y_offset + 35);
            window_draw_list->AddText(total_pos2, ImColor(1.0f, 1.0f, 1.0f, 1.0f), ss.str().c_str());
            
            // Continue button
            ImGui::SetCursorScreenPos(ImVec2(curr_pos.x + sw_game.box_width / 2.0 - 60, 
                                              curr_pos.y + sw_game.box_height - 50));
            if(ImGui::Button("Next Customer >>")){
                StartNextRound();
            }
        }
        
        // Draw the completed sandwich on the right
        ImVec2 sandwich_pos(curr_pos.x + sw_game.box_width * 0.75, curr_pos.y + sw_game.box_height - 100);
        DrawSandwich(window_draw_list, sandwich_pos, dt);
        
    } else if(sw_game.phase == sw_game_phase_t::GameOver){
        // This phase isn't really used in this version, but kept for potential future use
        const auto game_over_text = "Thanks for playing!";
        const auto text_size = ImGui::CalcTextSize(game_over_text);
        const ImVec2 text_pos(curr_pos.x + sw_game.box_width/2.0 - text_size.x/2.0,
                              curr_pos.y + sw_game.box_height/2.0 - text_size.y/2.0);
        window_draw_list->AddText(text_pos, ImColor(1.0f, 1.0f, 1.0f, 1.0f), game_over_text);
        
        ImGui::SetCursorScreenPos(ImVec2(curr_pos.x + sw_game.box_width / 2.0 - 40, 
                                          curr_pos.y + sw_game.box_height / 2.0 + 30));
        if(ImGui::Button("Play Again")){
            Reset();
        }
    }

    t_sw_updated = t_now;

    ImGui::Dummy(ImVec2(sw_game.box_width, sw_game.box_height));
    ImGui::End();
    return true;
}
