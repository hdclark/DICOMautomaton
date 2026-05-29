// Sandwich.h.

#pragma once

#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <string>

#include "YgorMath.h"
#include "YgorMisc.h"

#include "../imgui20210904/imgui.h"

// Sandwich Building Game
//
// Controls:
//   - Click on ingredient buttons to add them to your sandwich
//   - R key: reset the game / start new game
//   - Space: skip to next phase (during order display)
//
// Gameplay:
//   - At the beginning of each round, you're shown the customer's order for 5 seconds
//   - Orders include special requests (e.g., "extra mayo", "hold the lettuce")
//   - You have 30 seconds to build the sandwich
//   - Each round, orders get progressively more complicated
//   - Scoring provides specific feedback for improvements
//
// Visual elements:
//   - Sandwich layers stack up as you build
//   - Animations show ingredients being added
//   - Scoring breakdown shows exactly what was wrong

// Ingredient types
enum class sw_ingredient_t {
    // Breads (always at top and bottom)
    BreadBottom,
    BreadTop,
    
    // Proteins
    BeefPatty,
    ChickenPatty,
    FishPatty,
    
    // Cheese
    Cheddar,
    Swiss,
    
    // Vegetables
    Lettuce,
    Tomato,
    Onion,
    Pickle,
    
    // Condiments
    Ketchup,
    Mustard,
    Mayo,
    
    // Special
    Bacon,
    Egg
};

struct sw_layer_t {
    sw_ingredient_t ingredient;
    double y_offset;           // Current vertical offset for animation
    double target_y;           // Target vertical position
    double scale;              // Scale for animation
    bool animating;            // Whether currently animating
    std::chrono::time_point<std::chrono::steady_clock> added_time;
};

struct sw_order_requirement_t {
    sw_ingredient_t ingredient;
    int count;                 // Number required (0 means "hold" this item)
    bool is_special;           // Is this a special request (extra/hold)?
    std::string special_note;  // e.g., "extra", "hold"
};

struct sw_score_feedback_t {
    std::string message;
    int points;               // Can be negative
    ImColor color;
};

class SandwichGame {
  public:
    SandwichGame();
    bool Display(bool &enabled);
    void Reset();

  private:
    enum class sw_game_phase_t {
        ShowOrder,    // Showing the order for 5 seconds
        Building,     // Player is building the sandwich
        Scoring,      // Showing the score breakdown
        GameOver      // Game over screen
    };

    struct sw_game_t {
        double box_width = 700.0;
        double box_height = 600.0;
        
        sw_game_phase_t phase = sw_game_phase_t::ShowOrder;
        
        int64_t round = 1;
        int64_t total_score = 0;
        int64_t round_score = 0;
        int64_t high_score = 0;
        
        double phase_timer = 0.0;          // Time remaining in current phase
        double order_display_time = 5.0;   // Seconds to show order
        double build_time = 30.0;          // Seconds to build
        double scoring_display_time = 5.0; // Seconds to show scoring
        
        std::vector<sw_order_requirement_t> current_order;
        std::vector<sw_layer_t> sandwich_layers;
        std::vector<sw_score_feedback_t> score_feedback;
        
        // Animation state
        double score_reveal_timer = 0.0;
        int feedback_items_shown = 0;
        
        std::mt19937 re;
    } sw_game;
    
    std::chrono::time_point<std::chrono::steady_clock> t_sw_updated;
    std::chrono::time_point<std::chrono::steady_clock> t_sw_started;
    
    // Helper methods
    void GenerateOrder();
    void AddIngredient(sw_ingredient_t ingredient);
    void CalculateScore();
    std::string GetIngredientName(sw_ingredient_t ingredient);
    ImColor GetIngredientColor(sw_ingredient_t ingredient);
    double GetIngredientHeight(sw_ingredient_t ingredient);
    void DrawIngredient(ImDrawList* draw_list, sw_ingredient_t ingredient, 
                        ImVec2 pos, double width, double height, double scale);
    void DrawSandwich(ImDrawList* draw_list, ImVec2 base_pos, double dt);
    void StartNextRound();
    void StartBuildPhase();
};
