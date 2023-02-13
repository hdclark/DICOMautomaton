// Triple_Three.h.
#pragma once

#include <array>
#include <cstdint>

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

    tt_game_t();
    void reset(); // Randomizes cards, resets board, randomly selects first player.

    bool is_valid_card_num(int64_t card_num) const;
    bool is_first_player_card_num(int64_t card_num) const;
    bool is_valid_cell_num(int64_t cell_num) const;
    bool cell_holds_valid_card(int64_t cell_num) const;
    int64_t count_empty_cells() const;
    int64_t compute_score() const;
    bool is_game_complete() const;
    int64_t get_cell_num(int64_t row, int64_t col) const;
    tt_card_t& get_card(int64_t card_num);

    void move_card( int64_t card_num, int64_t cell_num );
    void auto_move_card(); // Move a card automatically, if possible.
    void perform_rudimentary_move(); // Use an extremely simple heuristic.
    void perform_move_search_v1( int64_t max_depth,  // Use a more complicated heuristic.
                                 int64_t max_simulations,
                                 bool peek_at_other_cards );
};

