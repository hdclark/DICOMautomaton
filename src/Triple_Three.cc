//Triple_Three.cc - A part of DICOMautomaton 2023. Written by hal clark.

#include <array>
#include <random>
#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include "Triple_Three.h"

tt_game_t::tt_game_t(){
    this->reset();
};

void tt_game_t::reset(){
    std::random_device rdev;
    std::mt19937 re( rdev() );

    // Randomize the cards.
    for(int64_t i = 0; i < static_cast<int64_t>(this->cards.size()); ++i){
        std::uniform_real_distribution<> frd(0.25, 0.75); // Tweak this to alter the distribution.
        const auto f1 = frd(re);
        const auto f2 = frd(re);
        const auto f3 = frd(re);
        const auto f4 = frd(re);
        const auto f_sum = f1 + f2 + f3 + f4;
        const auto max_total = 20.0;
        const int64_t max_single = 9;

        this->cards.at(i).stat_up    = std::clamp( static_cast<int64_t>(std::round( (f1/f_sum) * max_total )), static_cast<int64_t>(0), max_single );
        this->cards.at(i).stat_down  = std::clamp( static_cast<int64_t>(std::round( (f2/f_sum) * max_total )), static_cast<int64_t>(0), max_single );
        this->cards.at(i).stat_left  = std::clamp( static_cast<int64_t>(std::round( (f3/f_sum) * max_total )), static_cast<int64_t>(0), max_single );
        this->cards.at(i).stat_right = std::clamp( static_cast<int64_t>(std::round( (f4/f_sum) * max_total )), static_cast<int64_t>(0), max_single );
        this->cards.at(i).used = false;
        this->cards.at(i).owned_by_first_player = (i < 5) ? true : false;
    }

    // Reset the board.
    for(auto &i : this->board) i = -1;

    // Randomly select one player to go first.
    std::uniform_real_distribution<> frd(0.0, 1.0);
    const auto f = frd(re);
    this->first_players_turn = (f < 0.5) ? true : false;

    return;
}

bool tt_game_t::is_valid_card_num(int64_t card_num) const {
    return (0 <= card_num) && (card_num < 10);
}

bool tt_game_t::is_first_player_card_num(int64_t card_num) const {
    return (0 <= card_num) && (card_num < 5);
}

bool tt_game_t::is_valid_cell_num(int64_t cell_num) const {
    return (0 <= cell_num) && (cell_num < 9);
}

bool tt_game_t::cell_holds_valid_card(int64_t cell_num) const {
    return   this->is_valid_cell_num(cell_num)
          && this->is_valid_card_num( this->board.at(cell_num) );
}

int64_t tt_game_t::compute_score() const {
    int64_t score = 0;
    for(auto &i : this->board){
        if(this->is_valid_card_num(i)) score += (this->cards.at(i).owned_by_first_player) ? -1 : 1;
    }
    return score;
}

bool tt_game_t::is_game_complete() const {
    for(auto &i : this->board){
        if(!this->is_valid_card_num(i)) return false;
    }
    return true;
}

int64_t tt_game_t::get_cell_num(int64_t row, int64_t col) const {
    return (row * 3) + col;
}

tt_card_t& tt_game_t::get_card(int64_t card_num){
    return this->cards.at(card_num);
}

void tt_game_t::move_card( int64_t card_num, int64_t cell_num ){
    if( !this->is_valid_card_num(card_num) ){
        throw std::invalid_argument("Card invalid, refusing to overwrite");
    }
    const auto l_is_first_players_card = this->is_first_player_card_num(card_num);
    if( this->first_players_turn != l_is_first_players_card ){
        throw std::invalid_argument("Attempting to move card not owned by current player");
    }

    tt_card_t &card = this->cards.at(card_num);
    if(card.used){
        throw std::invalid_argument("Card already used, refusing to re-use it");
    }

    if( this->cell_holds_valid_card(cell_num) ){
        throw std::invalid_argument("Cell invalid or already occupied, refusing to overwrite");
    }

    // Make the move.
    this->board.at(cell_num) = card_num;
    card.used = true;
    this->first_players_turn = !this->first_players_turn;

    // Check the surrounding cards to 'flip' them.
    //
    // Grid cell numbers and adjacency:
    //   _________________
    //  |     |     |     |
    //  |  0  |  1  |  2  |
    //  |_____|_____|_____|
    //  |     |     |     |
    //  |  3  |  4  |  5  |
    //  |_____|_____|_____|
    //  |     |     |     |
    //  |  6  |  7  |  8  |
    //  |_____|_____|_____|
    //
    // Above:
    if( this->is_valid_cell_num(cell_num - 3)
    &&  (cell_num != 0)
    &&  (cell_num != 1)
    &&  (cell_num != 2)
    &&  this->cell_holds_valid_card(cell_num - 3) ){
        tt_card_t &l_card = this->cards.at( this->board.at(cell_num - 3) );
        if(l_card.stat_down < card.stat_up) l_card.owned_by_first_player = card.owned_by_first_player;
    }
    //
    // Left:
    if( this->is_valid_cell_num(cell_num - 1)
    &&  (cell_num != 0)
    &&  (cell_num != 3)
    &&  (cell_num != 6)
    &&  this->cell_holds_valid_card(cell_num - 1) ){
        tt_card_t &l_card = this->cards.at( this->board.at(cell_num - 1) );
        if(l_card.stat_right < card.stat_left) l_card.owned_by_first_player = card.owned_by_first_player;
    }
    //
    // Right:
    if( this->is_valid_cell_num(cell_num + 1)
    &&  (cell_num != 2)
    &&  (cell_num != 5)
    &&  (cell_num != 8)
    &&  this->cell_holds_valid_card(cell_num + 1) ){
        tt_card_t &l_card = this->cards.at( this->board.at(cell_num + 1) );
        if(l_card.stat_left < card.stat_right) l_card.owned_by_first_player = card.owned_by_first_player;
    }
    //
    // Below:
    if( this->is_valid_cell_num(cell_num + 3)
    &&  (cell_num != 6)
    &&  (cell_num != 7)
    &&  (cell_num != 8)
    &&  this->cell_holds_valid_card(cell_num + 3) ){
        tt_card_t &l_card = this->cards.at( this->board.at(cell_num + 3) );
        if(l_card.stat_up < card.stat_down) l_card.owned_by_first_player = card.owned_by_first_player;
    }

    return;
}

void tt_game_t::auto_move_card(){
    // This routine is meant to simulate a computer player. For the moment, it just moves the next available card into
    // the next available empty cell.

    const bool is_first_players_turn = this->first_players_turn;
    const int64_t starting_card_num = is_first_players_turn ? 0 : 5;

    for(int64_t card_num = starting_card_num; card_num < (starting_card_num + 5); ++card_num){
        const auto &card = this->get_card(card_num);
        for(int64_t cell_num = 0; cell_num < 9; ++cell_num){
            const auto cells_card_num = this->board.at(cell_num);
            if( !card.used
            &&  (card.owned_by_first_player == is_first_players_turn)
            &&  !this->is_valid_card_num( cells_card_num ) ){
                move_card( card_num, cell_num );
                return;
            }
        }
    }
    return;
}

