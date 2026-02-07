// Werewolf.h - A turn-based social deduction game for DICOMautomaton.

#pragma once

#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

#include "YgorMath.h"
#include "YgorMisc.h"

#include "../imgui20210904/imgui.h"

// A Werewolf-style social deduction game:
//   - One player (the human) plays against multiple computer players
//   - One player is secretly the werewolf
//   - Each round: players ask questions, then vote to eliminate someone
//   - If the werewolf is eliminated, townspeople win
//   - If one townsperson remains with the werewolf, werewolf wins
//
// Controls:
//   - Click on monoliths to select targets for questions
//   - Click on question/response buttons to make choices
//   - Click vote buttons to cast your vote
//   - R key: reset/restart the game

class WerewolfGame {
  public:
    WerewolfGame();
    bool Display(bool &enabled);
    void Reset();

  private:
    // Persona information for each player
    struct persona_t {
        std::string name;
        std::string profession;
        std::string years_in_town;
        std::string backstory;
        std::string quirk;
        std::string secret;  // Known only to the player themselves
    };

    // Predetermined questions that can be asked
    struct question_t {
        std::string text;
        std::string category;  // "profession", "history", "personal", "accusation"
        int difficulty;        // 1-3, how hard it is to detect lies
    };

    // Predetermined responses
    struct response_t {
        std::string text;
        bool is_deflection;    // If true, might indicate werewolf behavior
        double suspicion_delta; // How much this response changes suspicion
    };

    // Player state
    struct player_t {
        persona_t persona;
        bool is_werewolf = false;
        bool is_alive = true;
        bool is_human = false;  // The player controlled by the user
        
        // AI state
        std::map<int, double> suspicion_levels;  // Index -> suspicion (0.0 to 1.0)
        int questions_asked_this_round = 0;
        int selected_vote_target = -1;
        
        // Animation state
        double bob_phase = 0.0;
        double highlight_intensity = 0.0;
    };

    // Question-answer exchange in the current round
    struct exchange_t {
        int asker_idx;
        int target_idx;
        int question_idx;
        int response_idx;
        std::chrono::time_point<std::chrono::steady_clock> timestamp;
    };

    // Game phases
    enum class game_phase_t {
        Intro,              // Show intro text
        AssignRoles,        // Reveal role to player
        Discussion,         // Players ask questions
        SelectQuestion,     // Human player selecting a question
        WaitingResponse,    // Waiting for AI response (with pause)
        Voting,             // Players vote
        VoteResults,        // Show vote results
        Elimination,        // Someone is eliminated
        GameOver            // Show winner
    };

    // Initialize persona data
    void InitializePersonas();
    void InitializeQuestions();
    void InitializeResponses();
    
    // Game logic
    void AssignRoles();
    void StartRound();
    void ProcessAITurn();
    void ProcessVoting();
    void EliminatePlayer(int idx);
    bool CheckGameOver();
    
    // AI logic
    int AISelectQuestionTarget(int asker_idx);
    int AISelectQuestion(int asker_idx, int target_idx);
    int AISelectResponse(int responder_idx, int question_idx, bool as_werewolf);
    int AISelectVoteTarget(int voter_idx);
    void UpdateSuspicions(int observer_idx, int responder_idx, int question_idx, int response_idx);
    
    // Rendering helpers
    void CalculatePlayerPosition(int player_idx, float& angle, float& radius) const;
    void DrawMonolith(ImDrawList* draw_list, ImVec2 center, float height, float width, 
                      ImU32 color, const std::string& name, bool is_selected, bool is_dead);
    void DrawSpeechBubble(ImDrawList* draw_list, ImVec2 anchor, const std::string& text, bool is_question);
    
    // Game state
    game_phase_t phase = game_phase_t::Intro;
    int round_number = 0;
    int current_player_turn = 0;
    int human_player_idx = 0;
    int werewolf_idx = -1;
    bool game_over = false;
    bool townspeople_won = false;
    
    // Selection state
    int selected_target = -1;
    int selected_question = -1;
    int hovered_player = -1;
    
    // Animation state
    double anim_timer = 0.0;
    double phase_timer = 0.0;
    double pause_duration = 0.0;
    bool waiting_for_pause = false;
    std::string current_message;
    std::string current_speaker;
    
    // Vote tracking
    std::vector<int> votes;  // votes[i] = who player i voted for
    int last_eliminated = -1;
    bool last_was_werewolf = false;
    
    // Data
    std::vector<player_t> players;
    std::vector<exchange_t> round_exchanges;
    std::vector<question_t> all_questions;
    std::vector<response_t> all_responses;
    std::vector<persona_t> persona_pool;
    std::vector<int> available_question_indices;  // Questions available this round
    
    // Configuration
    static constexpr int num_players = 7;  // Including human player
    static constexpr double discussion_time_per_player = 2.0;  // Seconds for AI turns
    static constexpr double vote_reveal_time = 1.5;
    static constexpr double elimination_time = 2.5;
    static constexpr double intro_time = 3.0;
    
    // Display parameters
    static constexpr float window_width = 900.0f;
    static constexpr float window_height = 700.0f;
    static constexpr float monolith_height = 80.0f;
    static constexpr float monolith_width = 40.0f;
    static constexpr float circle_radius = 200.0f;
    
    // Time tracking
    std::chrono::time_point<std::chrono::steady_clock> t_updated;
    std::mt19937 rng;
};

