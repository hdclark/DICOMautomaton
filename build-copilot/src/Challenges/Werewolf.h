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
        bool is_seer = false;
        bool is_alive = true;
        bool is_human = false;  // The player controlled by the user
        bool was_lynched = false;
        bool was_attacked = false;
        
        // AI state
        std::map<int, double> suspicion_levels;  // Index -> suspicion (0.0 to 1.0)
        int questions_asked_this_round = 0;
        int selected_vote_target = -1;
        int firm_suspicion_target = -1;
        bool made_announcement_this_round = false;
        std::string gossip;
        
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

    struct suspicion_change_t {
        int observer_idx;
        int responder_idx;
        double delta;
        std::chrono::time_point<std::chrono::steady_clock> timestamp;
    };

    // Game phases
    enum class game_phase_t {
        Intro,              // Show intro text
        AssignRoles,        // Reveal role to player
        Discussion,         // Players ask questions
        SelectQuestion,     // Human player selecting a question
        WaitingResponse,    // Waiting for AI response (with pause)
        SelectResponse,     // Human player selecting a response
        SelectAnnouncement, // Human player selecting announcement
        Announcement,       // Showing an announcement
        AIQuestion,         // Showing AI's question
        AIResponse,         // Showing AI's response
        Voting,             // Players vote
        SelectAttack,       // Human werewolf selects attack target
        SelectSeerTarget,   // Human seer selects vision target
        SeerVision,         // Show seer vision result
        VoteResults,        // Show vote results
        Elimination,        // Someone is eliminated
        GameOver            // Show winner
    };

    enum class speech_kind_t {
        Question,
        Response,
        Announcement
    };

    // Initialize persona data
    void InitializePersonas();
    void InitializeQuestions();
    void InitializeResponses();
    void BuildResponseCompatibility();
    
    // Game logic
    void AssignRoles();
    void StartRound();
    void ProcessAITurn();
    void ProcessVoting();
    void EliminatePlayer(int idx, bool attacked);
    void ResolveWerewolfAttack(int forced_target = -1, bool force_skip = false);
    void ProceedAfterWerewolfAttack();
    void AdjustSuspicionsAfterAttack(int attacked_idx);
    bool CheckGameOver();
    void AssignRoundGossip();
    void LogEvent(const std::string& entry);
    void RecordExchange(int asker_idx, int target_idx, int question_idx, int response_idx);
    
    // AI logic
    int AISelectQuestionTarget(int asker_idx);
    int AISelectQuestion(int asker_idx, int target_idx);
    int AISelectResponse(int responder_idx, int question_idx, bool as_werewolf);
    int AISelectVoteTarget(int voter_idx);
    int AISelectSeerTarget(int seer_idx);
    void ResolveSeerVision(int seer_idx, int target_idx, bool reveal_to_player);
    void UpdateSuspicions(int observer_idx, int responder_idx, int question_idx, int response_idx,
                          double display_delay = 0.0);
    void UpdateFirmSuspicionTarget(int observer_idx);
    int SelectAnnouncementTarget(int announcer_idx);
    std::string BuildAnnouncementText(int announcer_idx, int target_idx);
    void ApplyAnnouncementEffects(int announcer_idx, int target_idx, bool is_werewolf);
    void QueueSuspicionChange(int observer_idx, int responder_idx, double delta, double display_delay);
    
    // Rendering helpers
    void CalculatePlayerPosition(int player_idx, float& angle, float& radius) const;
    void DrawMonolith(ImDrawList* draw_list, ImVec2 center, float height, float width,
                      ImU32 color, const std::string& name, bool is_selected,
                      float lynch_progress, float attack_progress, bool is_dead);
    void DrawSpeechBubble(ImDrawList* draw_list, ImVec2 anchor, const std::string& text, speech_kind_t kind);
    std::string GetPhaseName(game_phase_t phase) const;
    
    // Game state
    game_phase_t phase = game_phase_t::Intro;
    int round_number = 0;
    int current_player_turn = 0;
    int human_player_idx = 0;
    int werewolf_idx = -1;
    int seer_idx = -1;
    bool game_over = false;
    bool townspeople_won = false;
    
    // Selection state
    int selected_target = -1;
    int selected_question = -1;
    int selected_response = -1;
    int selected_announcement_target = -1;
    int selected_attack_target = -1;
    int selected_seer_target = -1;
    int hovered_player = -1;
    
    // Animation state
    double anim_timer = 0.0;
    double phase_timer = 0.0;
    double pause_duration = 0.0;
    bool waiting_for_pause = false;
    bool log_scroll_to_bottom = false;
    std::string current_message;
    std::string current_speaker;
    speech_kind_t current_message_kind = speech_kind_t::Question;
    bool waiting_response_from_human_question = false;
    
    // Pending AI exchange (for showing question then response)
    int pending_asker_idx = -1;
    int pending_question_idx = -1;
    int pending_target_idx = -1;
    int pending_response_idx = -1;
    
    // Vote tracking
    std::vector<int> votes;  // votes[i] = who player i voted for
    int last_eliminated = -1;
    bool last_was_werewolf = false;
    int last_attacked = -1;
    int last_attack_round = -1;
    
    // Data
    std::vector<player_t> players;
    std::vector<exchange_t> round_exchanges;
    std::vector<question_t> all_questions;
    std::vector<response_t> all_responses;
    std::vector<std::vector<int>> compatible_responses;
    std::vector<std::string> event_log;
    std::vector<persona_t> persona_pool;
    std::vector<std::vector<int>> per_player_question_options;
    std::vector<suspicion_change_t> suspicion_changes;
    int active_asker_idx = -1;
    int active_target_idx = -1;
    
    // Configuration
    static constexpr int num_players = 7;  // Including human player
    static constexpr double discussion_time_per_player = 4.0;  // Seconds for AI turns
    static constexpr double vote_reveal_time = 7.0;
    static constexpr double elimination_time = 5.0;
    static constexpr double intro_time = 15.0;
    static constexpr double ai_message_display_time = 5.0;  // Seconds to display AI Q&A bubbles
    static constexpr double elimination_indicator_fade_time = 1.1;
    static constexpr double attack_indicator_delay = 1.4;
    static constexpr double suspicion_change_duration = 5.0;
    static constexpr double werewolf_skip_attack_probability = 0.18;
    static constexpr double accused_innocent_suspicion_delta = 0.08;
    static constexpr double accused_werewolf_suspicion_delta = -0.05;
    static constexpr size_t max_questions_per_player = 5;
    
    // Display parameters
    static constexpr float window_width = 1000.0f;
    static constexpr float window_height = 950.0f;
    static constexpr float interaction_panel_height = 400.0f;
    static constexpr float monolith_height = 70.0f;
    static constexpr float monolith_width = 35.0f;
    static constexpr float circle_radius = 180.0f;
    static constexpr size_t max_log_entries = 1000;
    // Controls horizontal spacing for suspicion-change annotations above a player.
    static constexpr float suspicion_change_horizontal_offset = 8.0f;
    static constexpr int suspicion_change_offset_slots = 3;

    static inline const ImVec4 instruction_text_color = ImVec4(1.0f, 1.0f, 0.3f, 1.0f);
    static inline const ImVec4 role_werewolf_text_color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    static inline const ImVec4 role_town_text_color = ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
    static inline const ImVec4 warning_text_color = ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
    static inline const ImVec4 success_text_color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
    static inline const ImVec4 night_attack_text_color = ImVec4(0.7f, 0.6f, 1.0f, 1.0f);
    static inline const ImVec4 suspicion_increase_color = role_werewolf_text_color;
    static inline const ImVec4 suspicion_decrease_color = success_text_color;

    static constexpr ImU32 background_color = IM_COL32(20, 25, 35, 255);
    static constexpr ImU32 monolith_human_color = IM_COL32(100, 120, 140, 255);
    static constexpr ImU32 monolith_ai_color = IM_COL32(110, 110, 110, 255);
    static constexpr ImU32 monolith_dead_color = IM_COL32(80, 80, 80, 200);
    static constexpr ImU32 monolith_outline_color = IM_COL32(40, 40, 40, 255);
    static constexpr ImU32 monolith_outline_selected_color = IM_COL32(255, 255, 0, 255);
    static constexpr ImU32 monolith_name_color = IM_COL32(255, 255, 255, 255);
    static constexpr ImU32 lynch_line_color = IM_COL32(180, 0, 0, 255);
    static constexpr ImU32 attack_line_color = IM_COL32(120, 100, 200, 255);
    static constexpr ImU32 werewolf_label_color = IM_COL32(255, 100, 100, 255);
    static constexpr ImU32 speech_response_color = IM_COL32(60, 120, 60, 230);
    static constexpr ImU32 speech_question_color = IM_COL32(60, 60, 120, 230);
    static constexpr ImU32 speech_announcement_color = IM_COL32(120, 90, 40, 235);
    static constexpr ImU32 speech_border_color = IM_COL32(200, 200, 200, 255);
    
    // Time tracking
    std::chrono::time_point<std::chrono::steady_clock> t_updated;
    std::mt19937 rng;
};
