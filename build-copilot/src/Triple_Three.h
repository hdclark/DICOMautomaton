// Triple_Three.h.
#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <random>
#include <optional>

struct tt_card_t {
    int64_t stat_up    = 0;
    int64_t stat_down  = 0;
    int64_t stat_left  = 0;
    int64_t stat_right = 0;

    bool used = false;
    bool owned_by_first_player = true;
};

struct tt_game_t {
    std::array<tt_card_t,10> cards;
    std::array<int64_t,9> board;

    bool first_players_turn = true; // Tracks which player can take a turn now.

    std::mt19937 rand_gen;

    tt_game_t();
    void reset(); // Randomizes cards, resets board, randomly selects first player.

    bool is_valid_card_num(int64_t card_num) const;
    bool is_first_player_card_num(int64_t card_num) const;
    bool is_valid_cell_num(int64_t cell_num) const;
    bool cell_holds_valid_card(int64_t cell_num) const;
    int64_t count_empty_cells() const;
    int64_t compute_score() const; // Negative = better for first player, positive = better for second player.
    bool is_game_complete() const;
    int64_t get_cell_num(int64_t row, int64_t col) const;
    tt_card_t& get_card(int64_t card_num);
    const tt_card_t& get_const_card(int64_t card_num) const;

    std::vector<std::pair<int64_t, int64_t>> // card_num, cell_num.
    get_possible_moves( bool shuffle );

    std::optional<std::pair<int64_t, int64_t>> // card_num, cell_num.
    get_strongest_corner_move();

    void move_card( int64_t card_num, int64_t cell_num );
    void auto_move_card(); // Move a card automatically, if possible.

    void perform_rudimentary_move(); // Use an extremely simple heuristic.
    void perform_random_move();

    // Use a heuristic based on depth-first search and game outcome averaging.
    void perform_move_search_v1( int64_t max_depth = 3,
                                 int64_t max_simulations = 50'000,
                                 bool peek_at_other_cards = false );

    [[maybe_unused]]
    std::optional<std::pair<int64_t, int64_t>> // card_num, cell_num.
    score_best_move_v1( int64_t max_depth,
                        int64_t &available_simulations,
                        bool use_score_for_first_player,
                        int64_t &games_simulated,
                        double &best_move_score,
                        double &mean_children_score );

};

