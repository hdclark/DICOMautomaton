// Werewolf.h.

#pragma once

#include <vector>
#include <random>
#include <chrono>
#include <string>
#include <array>
#include <algorithm>
#include <cmath>

#include "YgorMath.h"
#include "YgorMisc.h"

#include "../imgui20210904/imgui.h"

// Controls:
//   - Mouse: click to interact with UI elements (ask questions, respond, vote)
//   - R key: reset the game
//
// Gameplay:
//   - One player is secretly the werewolf; all others are townspeople.
//   - Each round, players ask questions and respond to assess suspicion.
//   - At the end of each round, all players vote on who they think is the werewolf.
//   - The player with the most votes is lynched.
//   - If the werewolf is lynched, the townspeople win.
//   - If a townsperson is lynched, play continues.
//   - The werewolf wins when only one townsperson remains.
//   - Players are represented as gray monoliths arranged in a circle.
//   - Each player has a unique persona with name, profession, and backstory.
//

class WerewolfGame {
  public:
    WerewolfGame();
    bool Display(bool &enabled);
    void Reset();

  private:

    // ----- Persona and backstory data -----
    struct ww_persona_t {
        std::string name;
        std::string profession;
        std::string backstory;
        std::string gossip;           // What townspeople say about this person
        int64_t years_in_town = 0;    // How long they have lived here
    };

    // ----- Question / Response data -----
    struct ww_question_t {
        std::string text;
        int64_t difficulty = 1;       // 1=easy, 2=medium, 3=hard
        // Category helps AI decide how to respond
        // 0=background, 1=alibi, 2=behavior, 3=knowledge, 4=accusation
        int64_t category = 0;
    };

    struct ww_response_t {
        std::string text;
        double suspicion_if_wolf = 0.0;    // How suspicious if responder IS the wolf
        double suspicion_if_town = 0.0;    // How suspicious if responder is NOT the wolf
    };

    // ----- Per-player state -----
    struct ww_player_t {
        ww_persona_t persona;
        bool is_werewolf = false;
        bool is_alive = true;
        bool is_human = false;         // True only for the human player

        // Suspicion tracking: index maps to player index
        std::vector<double> suspicion;  // Running suspicion of each other player

        // Per-round state
        bool has_asked_question = false;
        int64_t question_target = -1;  // Who they asked
        int64_t question_idx = -1;     // Which question
        int64_t response_idx = -1;     // Which response they gave

        int64_t vote_target = -1;      // Who they vote for

        // Animation
        double monolith_bob = 0.0;     // Bobbing animation phase
        float glow_alpha = 0.0f;       // For highlight effects
    };

    // ----- Game phase -----
    enum class ww_phase_t {
        Setup,          // Show roles, introduce players
        QuestionSelect, // Human selects a question target and question
        QuestionsAI,    // AI players ask their questions (animated)
        ResponseSelect, // Human responds to a question (if asked)
        ResponsesAI,    // AI players respond (animated)
        Discussion,     // Brief pause showing all Q&A results
        VoteSelect,     // Human votes
        VotesAI,        // AI players vote (animated)
        VoteResult,     // Reveal vote tally and lynch
        RoundEnd,       // Show result of lynch
        GameOver         // Win or lose
    };

    // ----- Animation event for the log -----
    struct ww_log_entry_t {
        std::string text;
        ImVec4 color;
    };

    // ----- Core game state -----
    struct ww_game_t {
        int64_t num_players = 7;
        int64_t round_number = 0;

        ww_phase_t phase = ww_phase_t::Setup;
        double phase_timer = 0.0;          // Seconds spent in current phase
        double action_delay = 0.0;         // Delay before next AI action
        int64_t ai_action_idx = 0;         // Which AI player is currently acting

        // Available questions and responses for this round
        std::vector<int64_t> round_question_indices;
        std::vector<int64_t> round_response_indices;

        // Vote tally
        std::vector<int64_t> vote_counts;
        int64_t lynched_player = -1;
        bool lynched_was_wolf = false;

        // UI state
        int64_t selected_target = -1;
        int64_t selected_question = -1;
        int64_t selected_response = -1;
        int64_t human_asked_by = -1;       // Which AI asked the human a question
        int64_t human_asked_question = -1; // Which question

        bool show_role_reveal = true;
        double role_reveal_timer = 0.0;

        // Log
        std::vector<ww_log_entry_t> game_log;
        
        // Last lynch info (public knowledge)
        std::string last_lynch_name;
        bool last_lynch_was_wolf = false;
        bool any_lynch_happened = false;

        std::mt19937 re;
    } ww_game;

    std::vector<ww_player_t> players;

    // Master lists
    std::vector<ww_persona_t> all_personas;
    std::vector<ww_question_t> all_questions;
    std::vector<std::vector<ww_response_t>> all_responses; // Indexed by question

    std::chrono::time_point<std::chrono::steady_clock> t_ww_updated;

    // ----- Helper methods -----
    void InitPersonas();
    void InitQuestions();
    void AssignRoles();
    void StartRound();
    void SelectRoundQuestions();
    void AIAskQuestions();
    void AIRespond();
    void AIVote();
    void TallyVotes();
    void PerformLynch();
    bool CheckGameOver();
    void AddLog(const std::string &text, const ImVec4 &color = ImVec4(1,1,1,1));

    int64_t CountAlive() const;
    int64_t CountAliveWolves() const;
    int64_t CountAliveTown() const;

    // Drawing helpers
    void DrawMonolith(ImDrawList *dl, ImVec2 center, float w, float h,
                      const std::string &name, bool alive, bool highlighted,
                      bool is_human, float glow, ImU32 extra_color = 0);
    void DrawVotingArrow(ImDrawList *dl, ImVec2 from, ImVec2 to, ImU32 color);
};
