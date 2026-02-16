// Tracks.h - A Ticket to Ride-inspired train game for DICOMautomaton.

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

// Tracks: A multiplayer train route-building game inspired by Ticket to Ride.
//
// Game overview:
//   - Players compete to build train tracks connecting cities across BC and Alberta
//   - One human player competes against 3-6 AI players
//   - Each player receives 3 objectives (city pairs to connect)
//   - Players collect cards and spend them to build track segments
//   - Points are awarded for building tracks and completing objectives
//
// Controls:
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

    // Configuration
    static constexpr int max_trains_per_player = 50;
    static constexpr int initial_hand_size = 2;
    static constexpr int num_objectives_per_player = 3;
    static constexpr int collection_size = 5;
    static constexpr double card_animation_duration = 0.3;
    static constexpr double track_animation_duration = 0.5;
    static constexpr double message_display_time = 3.0;
    static constexpr double ai_turn_delay = 0.8;

    // Track point values by length
    static constexpr int track_points[7] = {0, 1, 2, 4, 7, 10, 15};  // index = num_slots

    // Display parameters
    static constexpr float window_width = 1200.0f;
    static constexpr float window_height = 850.0f;
    static constexpr float map_width = 800.0f;
    static constexpr float map_height = 550.0f;
    static constexpr float card_width = 50.0f;
    static constexpr float card_height = 70.0f;
    static constexpr float city_radius = 12.0f;
    static constexpr float slot_width = 12.0f;
    static constexpr float slot_height = 8.0f;

    // Time tracking
    std::chrono::time_point<std::chrono::steady_clock> t_updated;
    std::chrono::time_point<std::chrono::steady_clock> t_turn_started;
    double ai_delay_timer = 0.0;
    std::mt19937 rng;
};
