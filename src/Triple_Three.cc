//Triple_Three.cc - A part of DICOMautomaton 2023. Written by hal clark.

#include <array>
#include <random>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <limits>

#include "YgorMisc.h"
#include "YgorLog.h"

#include "Triple_Three.h"

tt_game_t::tt_game_t(){
    this->reset();
};

void tt_game_t::reset(){
    std::random_device rdev;
    this->rand_gen.seed( rdev() );

    // Randomize the cards.
    for(int64_t i = 0; i < static_cast<int64_t>(this->cards.size()); ++i){
        std::uniform_real_distribution<> frd(0.25, 0.75); // Tweak this to alter the distribution.
        const auto max_total = 20.0;
        const int64_t max_single = 9;
        const auto f1 = frd(this->rand_gen);
        const auto f2 = frd(this->rand_gen);
        const auto f3 = frd(this->rand_gen);
        const auto f4 = frd(this->rand_gen);
        const auto f_sum = f1 + f2 + f3 + f4;

        this->cards.at(i).stat_up    = std::clamp<int64_t>( static_cast<int64_t>(std::round( (f1/f_sum) * max_total )), static_cast<int64_t>(0), max_single );
        this->cards.at(i).stat_down  = std::clamp<int64_t>( static_cast<int64_t>(std::round( (f2/f_sum) * max_total )), static_cast<int64_t>(0), max_single );
        this->cards.at(i).stat_left  = std::clamp<int64_t>( static_cast<int64_t>(std::round( (f3/f_sum) * max_total )), static_cast<int64_t>(0), max_single );
        this->cards.at(i).stat_right = std::clamp<int64_t>( static_cast<int64_t>(std::round( (f4/f_sum) * max_total )), static_cast<int64_t>(0), max_single );
        this->cards.at(i).used = false;
        this->cards.at(i).owned_by_first_player = ((i < 5) ? true : false);
    }

    // Reset the board.
    for(auto &i : this->board) i = -1;

    // Randomly select one player to go first.
    std::uniform_real_distribution<> frd(0.0, 1.0);
    const auto f = frd(this->rand_gen);
    this->first_players_turn = ((f < 0.5) ? true : false);

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

int64_t tt_game_t::count_empty_cells() const {
    int64_t count = 0;
    for(auto &i : this->board){
        count += (this->is_valid_card_num(i) ? 0 : 1);
    }
    return count;
}

int64_t tt_game_t::compute_score() const {
    int64_t score = 0;
    for(auto &i : this->board){
        if(this->is_valid_card_num(i)) score += ((this->cards.at(i).owned_by_first_player) ? -1 : 1);
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

const tt_card_t& tt_game_t::get_const_card(int64_t card_num) const {
    return this->cards.at(card_num);
}

std::vector<std::pair<int64_t, int64_t>>
tt_game_t::get_possible_moves( bool shuffle ){
    const bool is_first_players_turn = this->first_players_turn;
    const int64_t starting_card_num = (is_first_players_turn ? 0 : 5);

    // Enumerate all available single-move combinations.
    std::vector<std::pair<int64_t, int64_t>> possible_moves; // card_num, cell_num.
    possible_moves.reserve(9*5); // Maximum possible: 9 grid cells, 5 cards.

    for(int64_t card_num = starting_card_num; card_num < (starting_card_num + 5); ++card_num){
        const auto &card = this->get_const_card(card_num);
        for(int64_t cell_num = 0; cell_num < 9; ++cell_num){
            const auto cells_card_num = this->board.at(cell_num);
            if( !card.used
            &&  (card.owned_by_first_player == is_first_players_turn)
            &&  !this->is_valid_card_num( cells_card_num ) ){
                possible_moves.emplace_back( card_num, cell_num );
            }
        }
    }

    if(shuffle){
        // Shuffle the available moves, so we don't always only consider, e.g., the first card for near-top-left moves.
        std::shuffle( std::begin(possible_moves),
                      std::end(possible_moves),
                      this->rand_gen );
    }
    return possible_moves;
}

std::optional<std::pair<int64_t, int64_t>> // card_num, cell_num.
tt_game_t::get_strongest_corner_move(){
    // Search for the 'strongest' card to place in a corner.
    //
    // Note: considers the card with the minimum exposed stat as the 'strongest.'
    std::optional<std::pair<int64_t, int64_t>> move;

    const bool is_first_players_turn = this->first_players_turn;
    const int64_t starting_card_num = (is_first_players_turn ? 0 : 5);

    int64_t current_best_score = 0;
    int64_t current_best_card = -1;
    int64_t current_best_cell = 0;
    const bool tl_empty = !this->cell_holds_valid_card(0);
    const bool tr_empty = !this->cell_holds_valid_card(2);
    const bool bl_empty = !this->cell_holds_valid_card(6);
    const bool br_empty = !this->cell_holds_valid_card(8);
    for(int64_t card_num = starting_card_num; card_num < (starting_card_num + 5); ++card_num){
        const auto &card = this->get_card(card_num);
        if(!card.used){
            const auto tl = std::min<int64_t>(card.stat_down, card.stat_right);
            const auto tr = std::min<int64_t>(card.stat_down, card.stat_left);
            const auto bl = std::min<int64_t>(card.stat_up, card.stat_right);
            const auto br = std::min<int64_t>(card.stat_up, card.stat_left);
            if( tl_empty
            &&  (current_best_score < tl) ){
                current_best_score = tl;
                current_best_card = card_num;
                current_best_cell = 0;
            }
            if( tr_empty
            &&  (current_best_score < tr) ){
                current_best_score = tr;
                current_best_card = card_num;
                current_best_cell = 2;
            }
            if( bl_empty
            &&  (current_best_score < bl) ){
                current_best_score = bl;
                current_best_card = card_num;
                current_best_cell = 6;
            }
            if( br_empty
            &&  (current_best_score < br) ){
                current_best_score = br;
                current_best_card = card_num;
                current_best_cell = 8;
            }
        }
    }
    if( this->is_valid_card_num(current_best_card)
    &&  this->is_valid_cell_num(current_best_cell) ){
        move = std::make_pair(current_best_card, current_best_cell);
    }
    return move;
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
    // This routine is meant to simulate a computer player.
    this->perform_move_search_v1();
    return;
}


void tt_game_t::perform_rudimentary_move(){
    // This routine performs a simplisitic move -- the first possible move, if any.
    const bool is_first_players_turn = this->first_players_turn;
    const int64_t starting_card_num = (is_first_players_turn ? 0 : 5);

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

void tt_game_t::perform_random_move(){
    const bool shuffle = true;
    const auto moves = this->get_possible_moves(shuffle);
    if(!moves.empty()){
        this->move_card( moves.front().first, moves.front().second );
    }
    return;
}

void tt_game_t::perform_move_search_v1( int64_t max_depth,
                                        int64_t max_simulations,
                                        bool peek_at_other_cards ){

    // This routine simulates games (up to a given number of cards played), optionally obscuring the opposing user's
    // cards. Due to the complexity, this routine uses a simple heuristic for first few cards placed on the board.
    const bool is_first_players_turn = this->first_players_turn;
    const int64_t starting_card_num = (is_first_players_turn ? 0 : 5);
    
    const auto remaining_cells = this->count_empty_cells();
    if(6 < remaining_cells){
        std::uniform_real_distribution<> frd(0.0, 1.0);
        const auto f1 = frd(this->rand_gen);
        bool move_performed = false;

        if(f1 < 0.75){
            if(auto corner_move = this->get_strongest_corner_move(); corner_move){
                YLOGINFO("Using 'strongest corners' pruning heuristic to prune search space");
                this->move_card(corner_move.value().first, corner_move.value().second);
                move_performed = true;
            }
        }
        if(!move_performed){
            YLOGINFO("Using 'random' heuristic to prune search space");
            this->perform_random_move();
            move_performed = true;
        }

    }else if(0 == remaining_cells){
        // Nothing more to do, so do nothing.

    }else if(1 == remaining_cells){
        // There is only one cell remaining, so move one card.

        // TODO: make this random? Or maybe still use depth-traversal?
        this->perform_rudimentary_move();

    }else if(max_depth == 0){
        // Treat this as an override; do not simulate moves, just return any move quickly.
        this->perform_rudimentary_move();

    }else{
        YLOGINFO("Using depth-first search heuristic to simulate moves");
        // Make a working copy to simplify move simulation.
        auto base_game = *this;
        const int64_t other_starting_card_num = (is_first_players_turn ? 5 : 0);

        // Obscure the opposing player's unused cards for the simulation.
        if(!peek_at_other_cards){
            for(int64_t offset = 0; offset < 5; ++offset){
                const auto &self_card = base_game.get_card(starting_card_num + offset);
                auto &other_card = base_game.get_card(other_starting_card_num + offset);
                if(!other_card.used){
                    // Copy from the current player's card, but rotate the stats in an ergodic way (i.e., the standard
                    // four-wheel vehicle tire rotation pattern). This isn't needed, but will help avoid some bias.
                    other_card.stat_up    = self_card.stat_right;
                    other_card.stat_down  = self_card.stat_up;
                    other_card.stat_left  = self_card.stat_down;
                    other_card.stat_right = self_card.stat_left;
                }
            }
        }

        const bool use_score_for_first_player = is_first_players_turn;
        int64_t l_available_simulations = max_simulations;
        int64_t games_simulated = 0;
        double mean_children_score = 0.0;
        double best_move_score = 0.0;
        const auto l_move_opt = base_game.score_best_move_v1(max_depth,
                                                             l_available_simulations,
                                                             use_score_for_first_player,
                                                             games_simulated,
                                                             best_move_score,
                                                             mean_children_score );
        if(l_move_opt){
            const auto l_best_move_score = best_move_score * ((is_first_players_turn) ? -1.0 : 1.0);
            YLOGINFO("Selecting move based on " << games_simulated << " simulations with predicted score " << l_best_move_score );
            this->move_card( l_move_opt.value().first, l_move_opt.value().second );
        }else{
            YLOGWARN("Unable to find move, performing fallback");
            this->perform_rudimentary_move();
        }
    }
    return;
}


[[maybe_unused]]
std::optional<std::pair<int64_t, int64_t>> // card_num, cell_num.
tt_game_t::score_best_move_v1( int64_t max_depth,
                               int64_t &available_simulations,
                               bool use_score_for_first_player,
                               int64_t &games_simulated,
                               double &best_move_score,
                               double &mean_children_score ){

    std::optional<std::pair<int64_t, int64_t>> best_move;
    if(this->is_game_complete()){
        // Not possible to make any moves, so nothing to do.
        return best_move;
    }

    // Otherwise, we evaluate the moves available.
    const bool should_shuffle = true;
    auto possible_moves = this->get_possible_moves( should_shuffle );
    const auto N_possible_moves = static_cast<int64_t>(possible_moves.size());
    if(N_possible_moves == 0){
        throw std::logic_error("No moves available, unable to continue");
    }
    const int64_t simulations_allotment = std::max<int64_t>(static_cast<int64_t>(1), available_simulations / N_possible_moves);
    int64_t surplus_move_simulations = 0;


    // For each possibility, try make the move, recurse, and see how the score changes.
    games_simulated = 0;
    std::vector<std::pair<int64_t, double>> move_score_changes; // # of simulations, mean score of all games simulated.
    for(const auto& possible_move : possible_moves){
        move_score_changes.emplace_back(0, 0.0);

        auto l_game = *this;
        l_game.move_card( possible_move.first, possible_move.second );

        // Leaf nodes / terminating moves.
        if( (max_depth <= 0)
        ||  l_game.is_game_complete() ){

            // Normalize score such the 'best' score is a maximum (as opposed to a minimum).
            const int64_t curr_score = l_game.compute_score() * (use_score_for_first_player ? -1 : 1);
            const int64_t l_games_simulated = 1;

            move_score_changes.back().first = l_games_simulated;
            move_score_changes.back().second = static_cast<double>(curr_score);
            games_simulated += l_games_simulated;
            --available_simulations;

        // Parent nodes.
        }else{
            // We are doing depth-first search, but have a 'budget' of simulations we can perform. Since we want to
            // collect statistics on these simulations, and the simulations might terminate early, we have to restrict
            // the balance of simulations available to each move. This will help ensure each move is represented fairly.
            // Additionally, since we might not use all simulations (e.g., because the budget its too high for the
            // number of possible moves), we create a pool of 'surplus' that we can draw on to maximize use of simulations.
            // So each possible move gets a fixed budget
            int64_t l_available_simulations = simulations_allotment;
            l_available_simulations += ((0 < surplus_move_simulations) ? surplus_move_simulations : 0);
            surplus_move_simulations = 0;
            const int64_t l_initial_available_simulations = l_available_simulations;

            int64_t l_games_simulated = 0;
            double l_mean_children_score = 0.0;
            double l_best_move_score = 0.0;
            // Note: we ignore the best move suggested by children here since these suggestions are predicated on the
            // above move first. They do not have knowledge of sibling moves, so are not useful.
            l_game.score_best_move_v1(max_depth - 1,
                                      l_available_simulations,
                                      use_score_for_first_player,
                                      l_games_simulated,
                                      l_best_move_score,
                                      l_mean_children_score);

            move_score_changes.back().first = l_games_simulated;
            move_score_changes.back().second = l_mean_children_score;
            games_simulated += l_games_simulated;

            const int64_t l_used_simulations = l_initial_available_simulations - l_available_simulations;
            available_simulations -= l_used_simulations;
            surplus_move_simulations = ((0 < l_available_simulations) ? l_available_simulations : 0);
        }

        // Stop processing if we've run out of our game simulation 'budget.'
        // Note that even if we run out, we try to simulate at least one game to give partially useful statistics.
        // Therefore we only check *after* running at least one child.
        if(available_simulations <= 0){
            break;
        }
    }

    // Generate statistics from the games.
    best_move_score = -(std::numeric_limits<double>::infinity());
    mean_children_score = 0.0;
    for(size_t i = 0UL; i < move_score_changes.size(); ++i){
        const auto& pm = possible_moves.at(i);
        const auto& mc = move_score_changes.at(i);

        const auto& l_games_simulated = mc.first;
        const auto& l_mean_children_score = mc.second;

        // Compute the mean score from recursive children simulations.
        const auto weight = static_cast<double>(l_games_simulated) / static_cast<double>(games_simulated);
        mean_children_score += l_mean_children_score * weight;

        // Identify the best next move.
        if(best_move_score < l_mean_children_score){
            best_move_score = l_mean_children_score;
            best_move = pm;
        }
    }

    return best_move;
}

