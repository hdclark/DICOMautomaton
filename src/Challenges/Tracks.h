// Tracks.h - A strategy game with global mutual exclusion mechanic.

#pragma once

#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <deque>

#include "YgorMath.h"
#include "YgorMisc.h"

#include "../imgui20210904/imgui.h"

// Tracks: A train route-building game set in Western Canada where players have to build first to accomplish their
// objectives -- or risk being blocked by others.
//
// Game overview:
//   - Players compete to build train tracks connecting cities across BC and Alberta
//   - One human player competes against 3-6 computer-controlled players
//   - Each player intially receives 3 objectives (city pairs to connect)
//   - Players collect cards and spend them to build track segments
//   - Points are awarded for building tracks and completing objectives
//
// Controls/interactions:
//   - Click cards in the collection to draw them
//   - Click "Draw Random" for a random card
//   - Click track paths to build them (if you have the right cards)
//   - R key: reset/restart the game

class TracksGame {
  public:
    TracksGame();
    bool Display(bool &enabled);
    void Reset();

  private:
    // Card colors (for payment)
    enum class card_color_t {
        White = 0,
        Black,
        Red,
        Orange,
        Yellow,
        Green,
        Blue,
        Rainbow  // Wildcard
    };

    // Player colors (distinct from card/path colors)
    enum class player_color_t {
        Crimson = 0,    // Deep red
        Navy,           // Deep blue
        Forest,         // Deep green
        Purple,         // Purple
        Teal,           // Teal
        Bronze,         // Bronze/brown
        Magenta         // Magenta
    };

    // AI personality types
    enum class ai_personality_t {
        Hoarder,        // Tends to collect many cards before building
        Builder,        // Builds tracks as soon as possible
        Strategic,      // Focuses on completing objectives
        Opportunist,    // Takes whatever seems best at the moment
        Blocker         // Tries to block other players
    };

    // A city on the map
    struct city_t {
        std::string name;
        vec2<double> pos;  // Screen position (normalized 0-1, scaled during render)
    };

    // A track path between two cities
    struct track_path_t {
        size_t city_a_idx;
        size_t city_b_idx;
        int num_slots;           // 1-6 slots
        card_color_t color;      // Required card color
        int owner_player_idx;    // -1 if unbuilt, else player index
        bool is_parallel;        // True if this is a parallel route
    };

    // An objective (pair of cities to connect)
    struct objective_t {
        size_t city_a_idx;
        size_t city_b_idx;
        int points;
        bool completed;
    };

    // A card in hand or collection
    struct card_t {
        card_color_t color;
    };

    // Animation state for a card being drawn
    struct card_animation_t {
        vec2<double> start_pos;
        vec2<double> end_pos;
        card_color_t color;
        double progress;     // 0.0 to 1.0
        double duration;     // Total duration in seconds
    };

    // Animation state for a track being built
    struct track_animation_t {
        size_t path_idx;
        double progress;     // 0.0 to 1.0
        double duration;
    };

    // Player state
    struct player_t {
        player_color_t color;
        std::string name;
        std::vector<card_t> hand;
        std::vector<objective_t> objectives;
        int trains_remaining;    // Max 50
        int score;
        bool is_human;
        ai_personality_t personality;

        // Track connectivity for objective checking
        std::map<size_t, std::set<size_t>> connections;  // city -> connected cities
    };

    // Game phases
    enum class game_phase_t {
        Setup,              // Initial setup
        SelectDifficulty,   // Player selects difficulty (number of AI players)
        DealingCards,       // Initial card dealing
        PlayerTurn_Draw,    // Player's turn to draw cards
        PlayerTurn_Build,   // Player's turn to build (optional)
        AITurn,             // AI player taking turn
        GameOver            // Game finished
    };

    // Initialize game data
    vec2<double> NormalizeLongLat(double lon, double lat);
    void InitializeCities();
    void InitializeTrackPaths();
    void InitializeDeck();
    void InitializePlayers(int num_ai_players);
    void DealInitialCards();
    void DealObjectives();
    void RefillCollection();

    // Game logic
    void StartGame(int num_ai_players);
    void NextTurn();
    void ProcessAITurn();
    bool CanBuildPath(int player_idx, size_t path_idx);
    void BuildPath(int player_idx, size_t path_idx);
    void UpdatePlayerConnections(int player_idx);
    bool CheckObjectiveCompleted(int player_idx, const objective_t& obj);
    void UpdateAllObjectives();
    void CalculateFinalScores();
    bool CheckGameEnd();
    int GetWinnerIdx();

    // AI logic
    void AISelectCards(int player_idx);
    size_t AISelectPathToBuild(int player_idx);  // Returns SIZE_MAX if no build
    int AIScorePath(int player_idx, size_t path_idx);
    bool IsPathUsefulForObjective(int player_idx, size_t path_idx);

    // Drawing helpers
    void DrawCard(int player_idx, card_color_t color);
    void DrawRandomCard(int player_idx);
    bool SpendCardsForPath(int player_idx, size_t path_idx);
    void ShuffleDeck();

    // Animation helpers
    void UpdateAnimations(double dt);
    void AddCardAnimation(const vec2<double>& start, const vec2<double>& end, card_color_t color);
    void AddTrackAnimation(size_t path_idx);

    // Rendering helpers
    ImU32 GetCardColor(card_color_t color) const;
    ImU32 GetPlayerColor(player_color_t color) const;
    std::string GetCardColorName(card_color_t color) const;
    std::string GetPlayerColorName(player_color_t color) const;
    void DrawCity(ImDrawList* draw_list, ImVec2 base_pos, const city_t& city, float scale);
    void DrawTrackPath(ImDrawList* draw_list, ImVec2 base_pos, const track_path_t& path, float scale);
    void DrawPlayerHand(ImDrawList* draw_list, ImVec2 pos);
    void DrawCardCollection(ImDrawList* draw_list, ImVec2 pos);
    void DrawScoreboard(ImDrawList* draw_list, ImVec2 pos);
    void DrawObjectives(ImDrawList* draw_list, ImVec2 pos);

    // Helper functions
    int CountCardsOfColor(int player_idx, card_color_t color);
    int CountWildcards(int player_idx);
    std::vector<size_t> FindPath(int player_idx, size_t from_city, size_t to_city);
    std::vector<size_t> GetCardsToSpendForPath(int player_idx, size_t path_idx);
    void DrawProvinceBoundaries(ImDrawList* draw_list, ImVec2 map_pos);
    void PresentObjectiveChoices();

    // Game state
    game_phase_t phase = game_phase_t::SelectDifficulty;
    int current_player_idx = 0;
    int cards_drawn_this_turn = 0;
    bool has_built_this_turn = false;
    bool game_over = false;
    bool final_round = false;      // Triggered when someone has <=2 trains
    int final_round_starter = -1;  // Player who triggered final round
    int turns_until_end = 0;       // Countdown to end

    // Data
    std::vector<city_t> cities;
    std::vector<track_path_t> track_paths;
    std::vector<player_t> players;
    std::vector<card_t> deck;
    std::vector<card_t> discard_pile;
    std::vector<card_t> collection;  // 5 face-up cards
    std::vector<objective_t> objective_deck;

    // Animations
    std::deque<card_animation_t> card_animations;
    std::deque<track_animation_t> track_animations;

    // UI state
    int hovered_path_idx = -1;
    int selected_path_idx = -1;
    bool show_build_confirmation = false;
    std::string message;
    double message_timer = 0.0;
    int hovered_objective_idx = -1;   // Index of objective being hovered in YOUR OBJECTIVES
    int hovered_player_idx = -1;      // Index of player being hovered in SCORES
    std::vector<size_t> highlighted_cards;  // Indices of cards to highlight when hovering buildable track

    // Objective selection state (for "add objective" feature)
    bool selecting_objective = false;
    bool has_added_objective_this_turn = false;
    std::vector<objective_t> objective_choices;  // 3 choices presented to player

    // Configuration
    static constexpr int max_trains_per_player = 50;
    static constexpr int initial_hand_size = 3;
    static constexpr int num_objectives_per_player = 3;
    static constexpr int collection_size = 5;
    static constexpr double card_animation_duration = 1.3;
    static constexpr double track_animation_duration = 1.5;
    static constexpr double message_display_time = 5.0;
    static constexpr double ai_turn_delay = 3.0;

    // Track point values by length
    static constexpr int track_points[7] = {0, 1, 2, 4, 7, 10, 15};  // index = num_slots

    // Display parameters
    static constexpr float window_width = 1200.0f;
    static constexpr float window_height = 850.0f;
    static constexpr float map_width = 800.0f;
    static constexpr float map_height = 550.0f;
    static constexpr float card_width = 80.0f;
    static constexpr float card_height = 100.0f;
    static constexpr float city_radius = 5.0f;
    static constexpr float slot_width = 16.0f;
    static constexpr float slot_height = 10.0f;

    // UI interaction constants
    static constexpr float mouse_track_hover_dist = 10.0f;

    // UI position constants
    static constexpr float map_offset_x = 10.0f;
    static constexpr float map_offset_y = 10.0f;
    static constexpr float panel_offset_x = 20.0f;  // Distance from map to right panel
    static constexpr float turn_indicator_height = 25.0f;
    static constexpr float phase_indicator_height = 25.0f;
    static constexpr float score_header_height = 20.0f;
    static constexpr float score_line_height = 18.0f;
    static constexpr float score_section_spacing = 15.0f;
    static constexpr float objective_header_height = 20.0f;
    static constexpr float objective_line_height = 18.0f;
    static constexpr float objective_section_spacing = 15.0f;
    static constexpr float message_height = 25.0f;
    static constexpr float cards_section_offset_y = 20.0f;
    static constexpr float cards_header_height = 20.0f;
    static constexpr float cards_hand_offset_y = 30.0f;
    static constexpr float card_spacing = 10.0f;
    static constexpr float end_turn_button_offset_y = 50.0f;
    static constexpr float end_turn_button_width = 100.0f;
    static constexpr float end_turn_button_height = 35.0f;
    static constexpr float difficulty_title_y = 80.0f;
    static constexpr float difficulty_subtitle_y = 110.0f;
    static constexpr float difficulty_text_y = 200.0f;
    static constexpr float difficulty_buttons_y = 250.0f;
    static constexpr float difficulty_button_width = 120.0f;
    static constexpr float difficulty_button_height = 40.0f;
    static constexpr float instructions_start_y = 350.0f;
    static constexpr float instruction_line_height = 20.0f;
    static constexpr float build_info_offset_y = 30.0f;
    static constexpr float game_over_overlay_alpha = 180;
    static constexpr float game_over_title_y = 150.0f;
    static constexpr float game_over_winner_y = 200.0f;
    static constexpr float game_over_scores_header_y = 260.0f;
    static constexpr float game_over_scores_line_height = 30.0f;
    static constexpr float game_over_score_line_height = 25.0f;
    static constexpr float game_over_objectives_offset_y = 20.0f;
    static constexpr float game_over_objective_line_height = 22.0f;
    static constexpr float restart_button_offset_y = 30.0f;
    static constexpr float restart_button_width = 120.0f;
    static constexpr float restart_button_height = 40.0f;
    static constexpr float add_objective_button_width = 120.0f;
    static constexpr float add_objective_button_height = 30.0f;
    static constexpr float objective_choice_button_width = 350.0f;
    static constexpr float objective_choice_button_height = 35.0f;
    static constexpr float objective_choice_spacing = 10.0f;
    static constexpr float difficulty_buttons_start_x = 200.0f;  // X offset for difficulty buttons
    static constexpr float objective_selection_subtitle_y = 180.0f;
    static constexpr float objective_selection_choices_y = 230.0f;

    // Color constants (ImU32 values) - Card colors
    static constexpr ImU32 color_card_white   = IM_COL32(240, 240, 240, 255);
    static constexpr ImU32 color_card_black   = IM_COL32(40, 40, 40, 255);
    static constexpr ImU32 color_card_red     = IM_COL32(220, 50, 50, 255);
    static constexpr ImU32 color_card_orange  = IM_COL32(240, 140, 40, 255);
    static constexpr ImU32 color_card_yellow  = IM_COL32(240, 220, 40, 255);
    static constexpr ImU32 color_card_green   = IM_COL32(50, 180, 50, 255);
    static constexpr ImU32 color_card_blue    = IM_COL32(50, 100, 220, 255);
    static constexpr ImU32 color_card_rainbow = IM_COL32(200, 100, 200, 255);
    static constexpr ImU32 color_card_unknown = IM_COL32(128, 128, 128, 255);

    // Color constants - Player colors
    static constexpr ImU32 color_player_crimson = IM_COL32(180, 30, 30, 255);
    static constexpr ImU32 color_player_navy    = IM_COL32(30, 50, 140, 255);
    static constexpr ImU32 color_player_forest  = IM_COL32(30, 100, 30, 255);
    static constexpr ImU32 color_player_purple  = IM_COL32(120, 40, 160, 255);
    static constexpr ImU32 color_player_teal    = IM_COL32(30, 140, 140, 255);
    static constexpr ImU32 color_player_bronze  = IM_COL32(160, 100, 40, 255);
    static constexpr ImU32 color_player_magenta = IM_COL32(180, 50, 130, 255);
    static constexpr ImU32 color_player_unknown = IM_COL32(128, 128, 128, 255);

    // Color constants - UI colors
    static constexpr ImU32 color_background         = IM_COL32(40, 50, 60, 255);
    static constexpr ImU32 color_map_background     = IM_COL32(60, 80, 70, 255);
    static constexpr ImU32 color_map_border         = IM_COL32(100, 120, 110, 255);
    static constexpr ImU32 color_difficulty_bg      = IM_COL32(30, 40, 50, 255);
    static constexpr ImU32 color_title              = IM_COL32(255, 220, 100, 255);
    static constexpr ImU32 color_subtitle           = IM_COL32(180, 180, 180, 255);
    static constexpr ImU32 color_text               = IM_COL32(220, 220, 220, 255);
    static constexpr ImU32 color_text_dim           = IM_COL32(180, 180, 180, 255);
    static constexpr ImU32 color_instructions       = IM_COL32(200, 200, 200, 255);
    static constexpr ImU32 color_city_fill          = IM_COL32(220, 200, 180, 255);
    static constexpr ImU32 color_city_border        = IM_COL32(60, 40, 30, 255);
    static constexpr ImU32 color_city_name          = IM_COL32(255, 255, 255, 220);
    static constexpr ImU32 color_slot_hover         = IM_COL32(255, 255, 150, 255);
    static constexpr ImU32 color_slot_border        = IM_COL32(0, 0, 0, 180);
    static constexpr ImU32 color_message            = IM_COL32(255, 255, 150, 255);
    static constexpr ImU32 color_objective_complete = IM_COL32(100, 255, 100, 255);
    static constexpr ImU32 color_objective_pending  = IM_COL32(200, 200, 200, 255);
    static constexpr ImU32 color_objective_failed   = IM_COL32(255, 100, 100, 255);
    static constexpr ImU32 color_button_disabled    = IM_COL32(80, 80, 80, 255);
    static constexpr ImU32 color_text_disabled      = IM_COL32(140, 140, 140, 255);
    static constexpr ImU32 color_button_random      = IM_COL32(100, 100, 100, 255);
    static constexpr ImU32 color_card_border        = IM_COL32(0, 0, 0, 200);
    static constexpr ImU32 color_game_over_overlay  = IM_COL32(0, 0, 0, 180);
    static constexpr ImU32 color_win_text           = IM_COL32(100, 255, 100, 255);
    static constexpr ImU32 color_lose_text          = IM_COL32(255, 100, 100, 255);
    static constexpr ImU32 color_build_info         = IM_COL32(255, 255, 200, 255);
    static constexpr ImU32 color_card_highlight     = IM_COL32(255, 255, 0, 200);
    static constexpr ImU32 color_track_dimmed       = IM_COL32(60, 60, 60, 150);
    static constexpr ImU32 color_city_dimmed        = IM_COL32(80, 80, 80, 100);
    static constexpr ImU32 color_city_highlighted   = IM_COL32(255, 255, 100, 255);
    static constexpr ImU32 color_province_border    = IM_COL32(180, 160, 140, 120);
    static constexpr ImU32 color_text_light_bg      = IM_COL32(0, 0, 0, 255);
    static constexpr ImU32 color_text_dark_bg       = IM_COL32(255, 255, 255, 255);
    static constexpr ImU32 color_hover_background   = IM_COL32(100, 100, 100, 80);

    // Province boundary polylines for BC and Alberta
    // Longitude range: -139.1°W to -110.0°W, Latitude range: 48.3°N to 60.0°N
    // Note: y is inverted during rendering since screen y increases downward
    static constexpr std::pair<double, double> bc_mainland[] = {
        {-114.0, 49.0}, {-114.5, 49.5}, {-115.0, 50.1}, {-115.5, 50.6}, 
        {-116.2, 51.3}, {-116.8, 51.8}, {-117.5, 52.2}, {-118.2, 52.8}, 
        {-118.8, 53.1}, {-119.5, 53.4}, {-120.0, 53.7}, {-120.0, 60.0}, 
        {-139.1, 60.0}, {-138.0, 59.5}, {-136.5, 59.3}, {-135.5, 58.8}, 
        {-134.5, 58.2}, {-133.5, 57.5}, {-132.0, 56.5}, {-130.5, 55.5}, 
        {-130.0, 55.0}, {-130.5, 54.4}, {-129.5, 54.0}, {-128.8, 53.2}, 
        {-128.2, 52.4}, {-127.8, 51.5}, {-127.2, 50.8}, {-126.5, 50.4}, 
        {-124.8, 50.1}, {-123.8, 49.5}, {-123.2, 49.1}, {-123.0, 49.0}, 
        {-114.0, 49.0}
    };
    static constexpr size_t bc_mainland_size = sizeof(bc_mainland) / sizeof(bc_mainland[0]);

    static constexpr std::pair<double, double> bc_vancouver_island[] = {
        {-128.4, 50.8}, {-127.5, 50.5}, {-126.5, 50.0}, {-125.8, 49.5}, 
        {-124.5, 49.2}, {-123.5, 48.5}, {-123.3, 48.4}, {-123.8, 48.3}, 
        {-124.8, 48.4}, {-125.8, 48.8}, {-127.0, 49.5}, {-128.0, 50.2}, 
        {-128.4, 50.8}
    };
    static constexpr size_t bc_vancouver_island_size = sizeof(bc_vancouver_island) / sizeof(bc_vancouver_island[0]);

    static constexpr std::pair<double, double> bc_haida_gwaii[] = {
        {-133.0, 54.2}, {-132.0, 54.1}, {-131.6, 53.5}, {-131.1, 52.8}, 
        {-131.0, 52.0}, {-131.3, 52.0}, {-132.0, 52.6}, {-133.0, 53.5}, 
        {-133.3, 54.0}, {-133.0, 54.2}
    };
    static constexpr size_t bc_haida_gwaii_size = sizeof(bc_haida_gwaii) / sizeof(bc_haida_gwaii[0]);

    static constexpr std::pair<double, double> alberta[] = {
        {-110.0, 49.0}, {-110.0, 60.0}, {-120.0, 60.0}, {-120.0, 53.7}, 
        {-119.5, 53.4}, {-118.8, 53.1}, {-118.2, 52.8}, {-117.5, 52.2}, 
        {-116.8, 51.8}, {-116.2, 51.3}, {-115.5, 50.6}, {-115.0, 50.1}, 
        {-114.5, 49.5}, {-114.0, 49.0}, {-110.0, 49.0}
    };
    static constexpr size_t alberta_size = sizeof(alberta) / sizeof(alberta[0]);

    // Time tracking
    std::chrono::time_point<std::chrono::steady_clock> t_updated;
    std::chrono::time_point<std::chrono::steady_clock> t_turn_started;
    double ai_delay_timer = 0.0;
    std::mt19937 rng;
};
