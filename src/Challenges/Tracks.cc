// Tracks.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <deque>
#include <queue>
#include <sstream>

#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"

#include <SDL.h>
#include "../imgui20210904/imgui.h"

#include "Tracks.h"

TracksGame::TracksGame(){
    rng.seed(std::random_device()());
    Reset();
}

void TracksGame::Reset(){
    phase = game_phase_t::SelectDifficulty;
    current_player_idx = 0;
    cards_drawn_this_turn = 0;
    has_built_this_turn = false;
    game_over = false;
    final_round = false;
    final_round_starter = -1;
    turns_until_end = 0;

    cities.clear();
    track_paths.clear();
    players.clear();
    deck.clear();
    discard_pile.clear();
    collection.clear();
    objective_deck.clear();
    card_animations.clear();
    track_animations.clear();

    hovered_path_idx = -1;
    selected_path_idx = -1;
    show_build_confirmation = false;
    message.clear();
    message_timer = 0.0;
    ai_delay_timer = 0.0;

    // New UI state
    hovered_objective_idx = -1;
    hovered_player_idx = -1;
    highlighted_cards.clear();
    selecting_objective = false;
    has_added_objective_this_turn = false;
    objective_choices.clear();

    t_updated = std::chrono::steady_clock::now();
    t_turn_started = t_updated;

    InitializeCities();
    InitializeTrackPaths();
    InitializeDeck();
}

vec2<double> TracksGame::NormalizeLongLat(double lon, double lat){
    // Helper to convert (longitude, latitude) to normalized coords
    //
    // BC and AB border min/max:
    // - lon_range: -139.1 to -110.0 (29.1 degrees)
    // - lat_range: 48.3 to 60.0 (11.7 degrees)
//    double x = (lon - (-139.1)) / 29.1;  // 0 at west, 1 at east
//    double y = (lat - 48.3) / 11.7;      // 0 at south, 1 at north
    double x = (lon - (-131.3)) / (131.3 - 109.0);  // 0 at west, 1 at east
    double y = (lat - 47.4) / 12.0;      // 0 at south, 1 at north
    return {x, y};
}


void TracksGame::InitializeCities(){
    // Position cities.
    //
    // Note: y is inverted during rendering (1-y) since screen y increases downward

    cities.clear();

    // British Columbia - Coast
    cities.push_back({"Vancouver", NormalizeLongLat(-123.1, 49.3)});      // ~49.3°N, 123.1°W
    cities.push_back({"Victoria", NormalizeLongLat(-123.4, 48.4)});       // ~48.4°N, 123.4°W
    cities.push_back({"Nanaimo", NormalizeLongLat(-123.9, 49.2)});        // ~49.2°N, 123.9°W
    cities.push_back({"Prince Rupert", NormalizeLongLat(-130.3, 54.3)});  // ~54.3°N, 130.3°W
    cities.push_back({"Kitimat", NormalizeLongLat(-128.7, 54.1)});        // ~54.1°N, 128.7°W

    // British Columbia - Interior
    cities.push_back({"Kamloops", NormalizeLongLat(-120.3, 50.7)});       // ~50.7°N, 120.3°W
    cities.push_back({"Kelowna", NormalizeLongLat(-119.5, 49.9)});        // ~49.9°N, 119.5°W
    cities.push_back({"Vernon", NormalizeLongLat(-119.3, 50.3)});         // ~50.3°N, 119.3°W
    cities.push_back({"Penticton", NormalizeLongLat(-119.6, 49.5)});      // ~49.5°N, 119.6°W
    cities.push_back({"Cranbrook", NormalizeLongLat(-115.8, 49.5)});      // ~49.5°N, 115.8°W
    cities.push_back({"Nelson", NormalizeLongLat(-117.3, 49.5)});         // ~49.5°N, 117.3°W
    cities.push_back({"Trail", NormalizeLongLat(-117.7, 49.1)});          // ~49.1°N, 117.7°W
    cities.push_back({"Revelstoke", NormalizeLongLat(-118.2, 51.0)});     // ~51.0°N, 118.2°W

    // British Columbia - North
    cities.push_back({"Prince George", NormalizeLongLat(-122.8, 53.9)});  // ~53.9°N, 122.8°W
    cities.push_back({"Quesnel", NormalizeLongLat(-122.5, 52.9)});        // ~52.9°N, 122.5°W
    cities.push_back({"Williams Lake", NormalizeLongLat(-122.1, 52.1)});  // ~52.1°N, 122.1°W
    cities.push_back({"Fort St. John", NormalizeLongLat(-120.8, 56.2)});  // ~56.2°N, 120.8°W
    cities.push_back({"Dawson Creek", NormalizeLongLat(-120.2, 55.8)});   // ~55.8°N, 120.2°W
    cities.push_back({"Fort Nelson", NormalizeLongLat(-122.7, 58.8)});    // ~58.8°N, 122.7°W

    // Alberta - South
    cities.push_back({"Calgary", NormalizeLongLat(-114.1, 51.0)});        // ~51.0°N, 114.1°W
    cities.push_back({"Lethbridge", NormalizeLongLat(-112.8, 49.7)});     // ~49.7°N, 112.8°W
    cities.push_back({"Medicine Hat", NormalizeLongLat(-110.7, 50.0)});   // ~50.0°N, 110.7°W
    cities.push_back({"Red Deer", NormalizeLongLat(-113.8, 52.3)});       // ~52.3°N, 113.8°W
    cities.push_back({"Banff", NormalizeLongLat(-115.6, 51.2)});          // ~51.2°N, 115.6°W

    // Alberta - Central/North
    cities.push_back({"Edmonton", NormalizeLongLat(-113.5, 53.5)});       // ~53.5°N, 113.5°W
    cities.push_back({"Grande Prairie", NormalizeLongLat(-118.8, 55.2)}); // ~55.2°N, 118.8°W
    cities.push_back({"Fort McMurray", NormalizeLongLat(-111.4, 56.7)});  // ~56.7°N, 111.4°W
    cities.push_back({"Jasper", NormalizeLongLat(-118.1, 52.9)});         // ~52.9°N, 118.1°W
    cities.push_back({"Lloydminster", NormalizeLongLat(-110.0, 53.3)});   // ~53.3°N, 110.0°W
    cities.push_back({"Wetaskiwin", NormalizeLongLat(-113.4, 53.0)});     // ~53.0°N, 113.4°W

    // Additional connection points
    cities.push_back({"Golden", NormalizeLongLat(-117.0, 51.3)});         // ~51.3°N, 117.0°W
    cities.push_back({"Fernie", NormalizeLongLat(-115.1, 49.5)});         // ~49.5°N, 115.1°W
    cities.push_back({"Hinton", NormalizeLongLat(-117.6, 53.4)});         // ~53.4°N, 117.6°W
    cities.push_back({"Whitecourt", NormalizeLongLat(-115.7, 54.1)});     // ~54.1°N, 115.7°W
    cities.push_back({"Slave Lake", NormalizeLongLat(-114.8, 55.3)});     // ~55.3°N, 114.8°W
    cities.push_back({"High Level", NormalizeLongLat(-117.1, 58.5)});     // ~58.5°N, 117.1°W
}


void TracksGame::InitializeTrackPaths(){
    track_paths.clear();

    // Helper to find city index by name
    auto find_city = [this](const std::string& name) -> size_t {
        for(size_t i = 0; i < cities.size(); ++i){
            if(cities[i].name == name) return i;
        }
        return SIZE_MAX;
    };

    // Define connections with slot counts and colors
    // Format: city_a, city_b, slots, color, is_parallel
    struct conn_def {
        std::string a, b;
        int slots;
        card_color_t color;
        bool parallel;
    };

    std::vector<conn_def> connections = {
        // Vancouver area connections
        {"Vancouver", "Victoria", 2, card_color_t::Blue, false},
        {"Vancouver", "Nanaimo", 1, card_color_t::White, false},
        {"Victoria", "Nanaimo", 2, card_color_t::Green, false},
        {"Vancouver", "Kamloops", 4, card_color_t::Red, false},
        {"Vancouver", "Kamloops", 4, card_color_t::Blue, true},

        // Interior BC connections
        {"Kamloops", "Kelowna", 2, card_color_t::Orange, false},
        {"Kamloops", "Vernon", 2, card_color_t::White, false},
        {"Vernon", "Kelowna", 1, card_color_t::Green, false},
        {"Kelowna", "Penticton", 1, card_color_t::Yellow, false},
        {"Penticton", "Trail", 3, card_color_t::Black, false},
        {"Trail", "Nelson", 1, card_color_t::Red, false},
        {"Nelson", "Cranbrook", 2, card_color_t::Orange, false},
        {"Cranbrook", "Fernie", 2, card_color_t::Blue, false},

        // Central BC
        {"Kamloops", "Revelstoke", 3, card_color_t::Yellow, false},
        {"Revelstoke", "Golden", 2, card_color_t::White, false},
        {"Golden", "Banff", 2, card_color_t::Green, false},
        {"Golden", "Revelstoke", 2, card_color_t::Blue, true},
        {"Revelstoke", "Jasper", 4, card_color_t::Red, false},

        // Northern BC
        {"Kamloops", "Williams Lake", 3, card_color_t::Black, false},
        {"Williams Lake", "Quesnel", 2, card_color_t::Orange, false},
        {"Quesnel", "Prince George", 2, card_color_t::White, false},
        {"Prince George", "Kitimat", 5, card_color_t::Green, false},
        {"Kitimat", "Prince Rupert", 3, card_color_t::Blue, false},
        {"Prince George", "Prince Rupert", 6, card_color_t::Yellow, false},

        // Far North BC
        {"Prince George", "Dawson Creek", 5, card_color_t::Red, false},
        {"Dawson Creek", "Fort St. John", 1, card_color_t::White, false},
        {"Fort St. John", "Fort Nelson", 4, card_color_t::Black, false},
        {"Fort Nelson", "High Level", 5, card_color_t::Orange, false},

        // BC to Alberta main corridors
        {"Jasper", "Hinton", 1, card_color_t::Yellow, false},
        {"Hinton", "Edmonton", 4, card_color_t::Red, false},
        {"Hinton", "Edmonton", 4, card_color_t::Blue, true},
        {"Banff", "Calgary", 2, card_color_t::Green, false},
        {"Banff", "Calgary", 2, card_color_t::White, true},
        {"Fernie", "Lethbridge", 4, card_color_t::Orange, false},

        // Alberta connections
        {"Calgary", "Red Deer", 2, card_color_t::Black, false},
        {"Calgary", "Red Deer", 2, card_color_t::Yellow, true},
        {"Red Deer", "Edmonton", 2, card_color_t::Red, false},
        {"Red Deer", "Edmonton", 2, card_color_t::White, true},
        {"Edmonton", "Wetaskiwin", 1, card_color_t::Green, false},
        {"Wetaskiwin", "Red Deer", 1, card_color_t::Blue, false},

        {"Calgary", "Lethbridge", 3, card_color_t::Blue, false},
        {"Lethbridge", "Medicine Hat", 3, card_color_t::Red, false},
        {"Medicine Hat", "Calgary", 4, card_color_t::Yellow, false},

        // Northern Alberta
        {"Edmonton", "Whitecourt", 2, card_color_t::Orange, false},
        {"Whitecourt", "Grande Prairie", 3, card_color_t::White, false},
        {"Grande Prairie", "Dawson Creek", 2, card_color_t::Green, false},
        {"Edmonton", "Slave Lake", 3, card_color_t::Black, false},
        {"Slave Lake", "Fort McMurray", 4, card_color_t::Red, false},
        {"Slave Lake", "High Level", 5, card_color_t::Blue, false},
        {"Edmonton", "Lloydminster", 3, card_color_t::Yellow, false},
        {"Edmonton", "Fort McMurray", 5, card_color_t::Orange, false},

        // Additional connections for redundancy
        {"Jasper", "Prince George", 5, card_color_t::Black, false},
        {"Golden", "Cranbrook", 3, card_color_t::Red, false},
        {"Hinton", "Jasper", 1, card_color_t::White, true},
        {"Whitecourt", "Hinton", 2, card_color_t::Green, false},
        {"Grande Prairie", "Fort St. John", 3, card_color_t::Blue, false},
    };

    for(const auto& conn : connections){
        size_t a = find_city(conn.a);
        size_t b = find_city(conn.b);
        if(a == SIZE_MAX || b == SIZE_MAX){
            continue;
        }
        track_path_t path;
        path.city_a_idx = a;
        path.city_b_idx = b;
        path.num_slots = conn.slots;
        path.color = conn.color;
        path.owner_player_idx = -1;
        path.is_parallel = conn.parallel;
        track_paths.push_back(path);
    }
}


void TracksGame::InitializeDeck(){
    deck.clear();
    discard_pile.clear();

    // Create cards: 12 of each color, 14 wildcards
    for(int c = 0; c <= static_cast<int>(card_color_t::Blue); ++c){
        for(int i = 0; i < 12; ++i){
            deck.push_back({static_cast<card_color_t>(c)});
        }
    }
    for(int i = 0; i < 14; ++i){
        deck.push_back({card_color_t::Rainbow});
    }

    ShuffleDeck();
}

void TracksGame::ShuffleDeck(){
    std::shuffle(deck.begin(), deck.end(), rng);
}

void TracksGame::InitializePlayers(int num_ai_players){
    players.clear();

    // Human player first
    player_t human;
    human.color = player_color_t::Crimson;
    human.name = "You";
    human.trains_remaining = max_trains_per_player;
    human.score = 0;
    human.is_human = true;
    human.personality = ai_personality_t::Strategic;  // Not used for human
    players.push_back(human);

    // AI players
    std::vector<player_color_t> ai_colors = {
        player_color_t::Navy,
        player_color_t::Forest,
        player_color_t::Purple,
        player_color_t::Teal,
        player_color_t::Bronze,
        player_color_t::Magenta
    };

    std::vector<std::string> ai_names = {
        "Alice", "Bob", "Carol", "Dave", "Eve", "Frank"
    };

    std::vector<ai_personality_t> personalities = {
        ai_personality_t::Hoarder,
        ai_personality_t::Builder,
        ai_personality_t::Strategic,
        ai_personality_t::Opportunist,
        ai_personality_t::Blocker,
        ai_personality_t::Strategic
    };

    for(int i = 0; i < num_ai_players && i < 6; ++i){
        player_t ai;
        ai.color = ai_colors[i];
        ai.name = ai_names[i];
        ai.trains_remaining = max_trains_per_player;
        ai.score = 0;
        ai.is_human = false;
        ai.personality = personalities[i];
        players.push_back(ai);
    }
}

void TracksGame::DealInitialCards(){
    // Deal 2 non-wildcard cards to each player
    for(auto& player : players){
        player.hand.clear();
        int dealt = 0;
        int attempts = 0;
        const int max_attempts = 100;  // Prevent infinite loop
        while(dealt < initial_hand_size && !deck.empty() && attempts < max_attempts){
            attempts++;
            auto& card = deck.back();
            if(card.color != card_color_t::Rainbow){
                player.hand.push_back(card);
                deck.pop_back();
                dealt++;
            } else {
                // Put wildcard in discard pile and shuffle deck to get new cards
                discard_pile.push_back(card);
                deck.pop_back();
                // Reshuffle if deck is running low to avoid getting stuck
                if(deck.size() < 5 && !discard_pile.empty()){
                    for(auto& c : discard_pile){
                        deck.push_back(c);
                    }
                    discard_pile.clear();
                    ShuffleDeck();
                }
            }
        }
    }
    RefillCollection();
}

void TracksGame::DealObjectives(){
    // Create objective deck based on city pairs
    objective_deck.clear();

    // Generate objectives with points based on approximate distance
    auto calc_points = [this](size_t a, size_t b) -> int {
        double dx = cities[a].pos.x - cities[b].pos.x;
        double dy = cities[a].pos.y - cities[b].pos.y;
        double dist = std::sqrt(dx*dx + dy*dy);
        // Scale: 0.1 distance = 3 points, 0.8 distance = 20 points
        int pts = static_cast<int>(3 + dist * 22);
        return std::clamp(pts, 3, 20);
    };

    // Define meaningful objective pairs (not adjacent cities)
    std::vector<std::pair<std::string, std::string>> obj_pairs = {
        {"Vancouver", "Calgary"},
        {"Vancouver", "Edmonton"},
        {"Victoria", "Banff"},
        {"Vancouver", "Prince George"},
        {"Prince Rupert", "Calgary"},
        {"Prince Rupert", "Edmonton"},
        {"Vancouver", "Fort McMurray"},
        {"Victoria", "Edmonton"},
        {"Kelowna", "Edmonton"},
        {"Cranbrook", "Prince George"},
        {"Nelson", "Calgary"},
        {"Kamloops", "Medicine Hat"},
        {"Prince George", "Lethbridge"},
        {"Dawson Creek", "Vancouver"},
        {"Fort Nelson", "Calgary"},
        {"Fort St. John", "Kelowna"},
        {"Kitimat", "Calgary"},
        {"Prince Rupert", "Medicine Hat"},
        {"Grande Prairie", "Vancouver"},
        {"Fort McMurray", "Vancouver"},
        {"High Level", "Victoria"},
        {"Lloydminster", "Prince Rupert"},
        {"Nanaimo", "Jasper"},
        {"Williams Lake", "Lethbridge"},
        {"Revelstoke", "Fort St. John"},
    };

    auto find_city = [this](const std::string& name) -> size_t {
        for(size_t i = 0; i < cities.size(); ++i){
            if(cities[i].name == name) return i;
        }
        return SIZE_MAX;
    };

    for(const auto& [a_name, b_name] : obj_pairs){
        size_t a = find_city(a_name);
        size_t b = find_city(b_name);
        if(a != SIZE_MAX && b != SIZE_MAX){
            objective_t obj;
            obj.city_a_idx = a;
            obj.city_b_idx = b;
            obj.points = calc_points(a, b);
            obj.completed = false;
            objective_deck.push_back(obj);
        }
    }

    std::shuffle(objective_deck.begin(), objective_deck.end(), rng);

    // Deal 3 objectives to each player
    for(auto& player : players){
        player.objectives.clear();
        for(int i = 0; i < num_objectives_per_player && !objective_deck.empty(); ++i){
            player.objectives.push_back(objective_deck.back());
            objective_deck.pop_back();
        }
    }
}

void TracksGame::RefillCollection(){
    // First check if we need to reshuffle the discard pile into the deck
    size_t cards_needed = collection_size - collection.size();
    if(deck.size() < cards_needed && !discard_pile.empty()){
        // Shuffle discard pile back into deck before attempting refill
        for(auto& c : discard_pile){
            deck.push_back(c);
        }
        discard_pile.clear();
        ShuffleDeck();
    }

    // Now refill the collection
    while(collection.size() < collection_size && !deck.empty()){
        collection.push_back(deck.back());
        deck.pop_back();
    }
}


void TracksGame::StartGame(int num_ai_players){
    InitializePlayers(num_ai_players);
    DealInitialCards();
    DealObjectives();
    phase = game_phase_t::PlayerTurn_Draw;
    current_player_idx = 0;
    cards_drawn_this_turn = 0;
    has_built_this_turn = false;
    has_added_objective_this_turn = false;
    selecting_objective = false;
    objective_choices.clear();
    t_turn_started = std::chrono::steady_clock::now();
    message = "Your turn! Select 1 card OR draw 2 random.";
    message_timer = message_display_time;
}

void TracksGame::NextTurn(){
    cards_drawn_this_turn = 0;
    has_built_this_turn = false;
    has_added_objective_this_turn = false;
    selecting_objective = false;
    objective_choices.clear();
    highlighted_cards.clear();

    // Check for game end
    if(CheckGameEnd()){
        CalculateFinalScores();
        phase = game_phase_t::GameOver;
        game_over = true;
        return;
    }

    // Move to next player
    current_player_idx = (current_player_idx + 1) % static_cast<int>(players.size());
    t_turn_started = std::chrono::steady_clock::now();

    if(final_round){
        turns_until_end--;
        if(turns_until_end <= 0){
            CalculateFinalScores();
            phase = game_phase_t::GameOver;
            game_over = true;
            return;
        }
    }

    if(players[current_player_idx].is_human){
        phase = game_phase_t::PlayerTurn_Draw;
        message = "Your turn! Select 1 card OR draw 2 random.";
        message_timer = message_display_time;
    } else {
        phase = game_phase_t::AITurn;
        ai_delay_timer = ai_turn_delay;
    }
}

bool TracksGame::CheckGameEnd(){
    // Check if any player has low trains (triggers final round)
    for(size_t i = 0; i < players.size(); ++i){
        if(players[i].trains_remaining <= 2){
            if(!final_round){
                final_round = true;
                final_round_starter = static_cast<int>(i);
                turns_until_end = static_cast<int>(players.size());
                message = players[i].name + " has triggered the final round!";
                message_timer = message_display_time;
            }
        }
    }

    // Check if all tracks have been built (game should end)
    bool all_tracks_built = true;
    for(const auto& path : track_paths){
        if(path.owner_player_idx == -1){
            all_tracks_built = false;
            break;
        }
    }
    if(all_tracks_built && !final_round){
        final_round = true;
        turns_until_end = static_cast<int>(players.size());
        message = "All tracks have been built! Final round!";
        message_timer = message_display_time;
    }

    return game_over;
}

void TracksGame::ProcessAITurn(){
    auto& player = players[current_player_idx];

    // AI draws cards
    AISelectCards(current_player_idx);

    // AI tries to build
    size_t path_to_build = AISelectPathToBuild(current_player_idx);
    if(path_to_build != SIZE_MAX){
        BuildPath(current_player_idx, path_to_build);
    }

    NextTurn();
}

void TracksGame::AISelectCards(int player_idx){
    auto& player = players[player_idx];

    // Simple strategy: prefer cards that match objectives or existing routes
    std::map<card_color_t, int> useful_colors;

    // Count colors needed for objectives - only consider paths that touch objective endpoints
    for(const auto& obj : player.objectives){
        if(obj.completed) continue;
        // Find paths that could help this objective (paths adjacent to either endpoint)
        for(size_t pi = 0; pi < track_paths.size(); ++pi){
            const auto& path = track_paths[pi];
            if(path.owner_player_idx != -1) continue;

            // A path is relevant if it touches either endpoint city of the objective
            bool relevant = (path.city_a_idx == obj.city_a_idx ||
                           path.city_a_idx == obj.city_b_idx ||
                           path.city_b_idx == obj.city_a_idx ||
                           path.city_b_idx == obj.city_b_idx);
            if(relevant){
                useful_colors[path.color]++;
            }
        }
    }

    // Draw 2 cards (or 1 random based on personality)
    if(player.personality == ai_personality_t::Hoarder){
        // Hoarder draws from collection if possible
        for(int i = 0; i < 2 && !collection.empty(); ++i){
            // Find best card in collection
            int best_idx = 0;
            int best_score = -1;
            for(size_t ci = 0; ci < collection.size(); ++ci){
                int score = useful_colors[collection[ci].color];
                if(collection[ci].color == card_color_t::Rainbow) score += 5;
                if(score > best_score){
                    best_score = score;
                    best_idx = static_cast<int>(ci);
                }
            }
            DrawCard(player_idx, collection[best_idx].color);
            collection.erase(collection.begin() + best_idx);
            RefillCollection();
        }
    } else {
        // Other personalities: mix of collection and random
        std::uniform_int_distribution<> dist(0, 2);
        if(dist(rng) == 0 || collection.empty()){
            DrawRandomCard(player_idx);
        } else {
            // Draw from collection
            std::uniform_int_distribution<> col_dist(0, static_cast<int>(collection.size()) - 1);
            int idx = col_dist(rng);
            DrawCard(player_idx, collection[idx].color);
            collection.erase(collection.begin() + idx);
            RefillCollection();

            if(dist(rng) == 0 || collection.empty()){
                DrawRandomCard(player_idx);
            } else {
                std::uniform_int_distribution<> col_dist2(0, static_cast<int>(collection.size()) - 1);
                idx = col_dist2(rng);
                DrawCard(player_idx, collection[idx].color);
                collection.erase(collection.begin() + idx);
                RefillCollection();
            }
        }
    }
}

size_t TracksGame::AISelectPathToBuild(int player_idx){
    auto& player = players[player_idx];

    // Score each buildable path
    std::vector<std::pair<size_t, int>> scored_paths;

    for(size_t pi = 0; pi < track_paths.size(); ++pi){
        if(CanBuildPath(player_idx, pi)){
            int score = AIScorePath(player_idx, pi);
            scored_paths.push_back({pi, score});
        }
    }

    if(scored_paths.empty()) return SIZE_MAX;

    // Sort by score descending
    std::sort(scored_paths.begin(), scored_paths.end(),
        [](const auto& a, const auto& b){ return a.second > b.second; });

    // Personality affects decision
    switch(player.personality){
        case ai_personality_t::Builder:
            // Always build if possible
            return scored_paths[0].first;

        case ai_personality_t::Hoarder:
            // Only build high-value paths
            if(scored_paths[0].second >= 10 || player.hand.size() > 15){
                return scored_paths[0].first;
            }
            return SIZE_MAX;

        case ai_personality_t::Strategic:
            // Build if it helps objectives
            if(IsPathUsefulForObjective(player_idx, scored_paths[0].first)){
                return scored_paths[0].first;
            }
            if(scored_paths[0].second >= 8){
                return scored_paths[0].first;
            }
            return SIZE_MAX;

        case ai_personality_t::Opportunist:
            // Build good paths
            if(scored_paths[0].second >= 5){
                return scored_paths[0].first;
            }
            return SIZE_MAX;

        case ai_personality_t::Blocker:
            // Look for paths that block others
            for(const auto& [pi, score] : scored_paths){
                const auto& path = track_paths[pi];
                // Prefer paths in busy areas
                if(path.num_slots >= 3){
                    return pi;
                }
            }
            if(!scored_paths.empty()){
                return scored_paths[0].first;
            }
            return SIZE_MAX;

        default:
            return scored_paths[0].first;
    }
}

int TracksGame::AIScorePath(int player_idx, size_t path_idx){
    const auto& path = track_paths[path_idx];
    int score = track_points[path.num_slots];

    // Bonus if helps objective
    if(IsPathUsefulForObjective(player_idx, path_idx)){
        score += 5;
    }

    // Bonus for connecting to existing network
    const auto& player = players[player_idx];
    if(player.connections.count(path.city_a_idx) || player.connections.count(path.city_b_idx)){
        score += 3;
    }

    return score;
}

bool TracksGame::IsPathUsefulForObjective(int player_idx, size_t path_idx){
    const auto& player = players[player_idx];
    const auto& path = track_paths[path_idx];

    for(const auto& obj : player.objectives){
        if(obj.completed) continue;

        // Simple check: does this path involve objective cities or help connect?
        if(path.city_a_idx == obj.city_a_idx || path.city_b_idx == obj.city_a_idx ||
           path.city_a_idx == obj.city_b_idx || path.city_b_idx == obj.city_b_idx){
            return true;
        }
    }
    return false;
}


bool TracksGame::CanBuildPath(int player_idx, size_t path_idx){
    if(path_idx >= track_paths.size()) return false;

    const auto& path = track_paths[path_idx];
    const auto& player = players[player_idx];

    // Already built?
    if(path.owner_player_idx != -1) return false;

    // Enough trains?
    if(player.trains_remaining < path.num_slots) return false;

    // Count matching cards + wildcards
    int matching = CountCardsOfColor(player_idx, path.color);
    int wildcards = CountWildcards(player_idx);

    return (matching + wildcards) >= path.num_slots;
}

void TracksGame::BuildPath(int player_idx, size_t path_idx){
    if(!CanBuildPath(player_idx, path_idx)) return;

    auto& path = track_paths[path_idx];
    auto& player = players[player_idx];

    // Spend cards
    SpendCardsForPath(player_idx, path_idx);

    // Claim path
    path.owner_player_idx = player_idx;
    player.trains_remaining -= path.num_slots;

    // Award points
    player.score += track_points[path.num_slots];

    // Update connections
    UpdatePlayerConnections(player_idx);

    // Check objectives
    UpdateAllObjectives();

    // Add animation
    AddTrackAnimation(path_idx);

    has_built_this_turn = true;
    message = player.name + " built " + cities[path.city_a_idx].name + " to " +
              cities[path.city_b_idx].name + " (+" + std::to_string(track_points[path.num_slots]) + " pts)";
    message_timer = message_display_time;
}

bool TracksGame::SpendCardsForPath(int player_idx, size_t path_idx){
    const auto& path = track_paths[path_idx];
    auto& player = players[player_idx];

    int needed = path.num_slots;
    std::vector<size_t> to_remove;

    // First use matching color cards
    for(size_t i = 0; i < player.hand.size() && needed > 0; ++i){
        if(player.hand[i].color == path.color){
            to_remove.push_back(i);
            needed--;
        }
    }

    // Then use wildcards
    for(size_t i = 0; i < player.hand.size() && needed > 0; ++i){
        if(player.hand[i].color == card_color_t::Rainbow){
            bool already_removed = false;
            for(size_t r : to_remove){
                if(r == i){ already_removed = true; break; }
            }
            if(!already_removed){
                to_remove.push_back(i);
                needed--;
            }
        }
    }

    // Remove cards (in reverse order to preserve indices)
    std::sort(to_remove.begin(), to_remove.end(), std::greater<size_t>());
    for(size_t idx : to_remove){
        discard_pile.push_back(player.hand[idx]);
        player.hand.erase(player.hand.begin() + static_cast<long>(idx));
    }

    return true;
}

void TracksGame::UpdatePlayerConnections(int player_idx){
    auto& player = players[player_idx];
    player.connections.clear();

    for(const auto& path : track_paths){
        if(path.owner_player_idx == player_idx){
            player.connections[path.city_a_idx].insert(path.city_b_idx);
            player.connections[path.city_b_idx].insert(path.city_a_idx);
        }
    }
}

bool TracksGame::CheckObjectiveCompleted(int player_idx, const objective_t& obj){
    const auto& player = players[player_idx];

    // BFS to find if cities are connected
    if(player.connections.empty()) return false;

    std::set<size_t> visited;
    std::queue<size_t> queue;
    queue.push(obj.city_a_idx);
    visited.insert(obj.city_a_idx);

    while(!queue.empty()){
        size_t current = queue.front();
        queue.pop();

        if(current == obj.city_b_idx) return true;

        auto it = player.connections.find(current);
        if(it != player.connections.end()){
            for(size_t neighbor : it->second){
                if(visited.find(neighbor) == visited.end()){
                    visited.insert(neighbor);
                    queue.push(neighbor);
                }
            }
        }
    }
    return false;
}

void TracksGame::UpdateAllObjectives(){
    for(size_t player_idx = 0; player_idx < players.size(); ++player_idx){
        auto& player = players[player_idx];
        for(auto& obj : player.objectives){
            if(!obj.completed){
                obj.completed = CheckObjectiveCompleted(static_cast<int>(player_idx), obj);
            }
        }
    }
}

void TracksGame::CalculateFinalScores(){
    for(auto& player : players){
        for(const auto& obj : player.objectives){
            if(obj.completed){
                player.score += obj.points;
            } else {
                player.score -= obj.points;
            }
        }
    }
}

int TracksGame::GetWinnerIdx(){
    int winner = 0;
    int best_score = players[0].score;
    for(size_t i = 1; i < players.size(); ++i){
        if(players[i].score > best_score){
            best_score = players[i].score;
            winner = static_cast<int>(i);
        }
    }
    return winner;
}

void TracksGame::DrawCard(int player_idx, card_color_t color){
    players[player_idx].hand.push_back({color});
}

void TracksGame::DrawRandomCard(int player_idx){
    if(deck.empty()){
        if(!discard_pile.empty()){
            deck = std::move(discard_pile);
            discard_pile.clear();
            ShuffleDeck();
        } else {
            return;
        }
    }
    players[player_idx].hand.push_back(deck.back());
    deck.pop_back();
}

int TracksGame::CountCardsOfColor(int player_idx, card_color_t color){
    int count = 0;
    for(const auto& card : players[player_idx].hand){
        if(card.color == color) count++;
    }
    return count;
}

int TracksGame::CountWildcards(int player_idx){
    return CountCardsOfColor(player_idx, card_color_t::Rainbow);
}

std::vector<size_t> TracksGame::GetCardsToSpendForPath(int player_idx, size_t path_idx){
    std::vector<size_t> result;
    if(path_idx >= track_paths.size()) return result;

    const auto& path = track_paths[path_idx];
    const auto& player = players[player_idx];

    int needed = path.num_slots;

    // First collect matching color cards
    for(size_t i = 0; i < player.hand.size() && needed > 0; ++i){
        if(player.hand[i].color == path.color){
            result.push_back(i);
            needed--;
        }
    }

    // Then collect wildcards
    for(size_t i = 0; i < player.hand.size() && needed > 0; ++i){
        if(player.hand[i].color == card_color_t::Rainbow){
            bool already_added = false;
            for(size_t r : result){
                if(r == i){ already_added = true; break; }
            }
            if(!already_added){
                result.push_back(i);
                needed--;
            }
        }
    }

    return result;
}

void TracksGame::DrawProvinceBoundaries(ImDrawList* draw_list, ImVec2 map_pos){
    // Helper lambda to draw a polyline
    auto draw_polyline = [&](const std::pair<double, double>* points, size_t count){
        if(count < 2) return;
        for(size_t i = 0; i < count - 1; ++i){
            // Convert normalized coordinates to screen coordinates
            // Note: The boundary data has y increasing northward, but our screen y increases downward
            // So we invert: screen_y = (1.0 - normalized_y) * map_height
            const auto pos1 = NormalizeLongLat(points[i].first, points[i].second);
            const auto pos2 = NormalizeLongLat(points[i+1].first, points[i+1].second);
            float x1 = map_pos.x + static_cast<float>(pos1.x) * map_width;
            float y1 = map_pos.y + static_cast<float>(1.0 - pos1.y) * map_height;
            float x2 = map_pos.x + static_cast<float>(pos2.x) * map_width;
            float y2 = map_pos.y + static_cast<float>(1.0 - pos2.y) * map_height;
            draw_list->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), color_province_border, 1.5f);
        }
    };

    // Draw BC mainland
    draw_polyline(bc_mainland, bc_mainland_size);

    // Draw Vancouver Island
    draw_polyline(bc_vancouver_island, bc_vancouver_island_size);

    // Draw Haida Gwaii
    draw_polyline(bc_haida_gwaii, bc_haida_gwaii_size);

    // Draw Alberta
    draw_polyline(alberta, alberta_size);
}

void TracksGame::PresentObjectiveChoices(){
    objective_choices.clear();

    // Present 3 random objectives from the deck
    std::vector<size_t> available_indices;
    for(size_t i = 0; i < objective_deck.size(); ++i){
        available_indices.push_back(i);
    }

    std::shuffle(available_indices.begin(), available_indices.end(), rng);

    int count = std::min(3, static_cast<int>(available_indices.size()));
    for(int i = 0; i < count; ++i){
        objective_choices.push_back(objective_deck[available_indices[i]]);
    }

    selecting_objective = true;
}

void TracksGame::UpdateAnimations(double dt){
    // Update card animations
    for(auto it = card_animations.begin(); it != card_animations.end();){
        it->progress += dt / it->duration;
        if(it->progress >= 1.0){
            it = card_animations.erase(it);
        } else {
            ++it;
        }
    }

    // Update track animations
    for(auto it = track_animations.begin(); it != track_animations.end();){
        it->progress += dt / it->duration;
        if(it->progress >= 1.0){
            it = track_animations.erase(it);
        } else {
            ++it;
        }
    }
}

void TracksGame::AddCardAnimation(const vec2<double>& start, const vec2<double>& end, card_color_t color){
    card_animation_t anim;
    anim.start_pos = start;
    anim.end_pos = end;
    anim.color = color;
    anim.progress = 0.0;
    anim.duration = card_animation_duration;
    card_animations.push_back(anim);
}

void TracksGame::AddTrackAnimation(size_t path_idx){
    track_animation_t anim;
    anim.path_idx = path_idx;
    anim.progress = 0.0;
    anim.duration = track_animation_duration;
    track_animations.push_back(anim);
}


ImU32 TracksGame::GetCardColor(card_color_t color) const {
    switch(color){
        case card_color_t::White:   return color_card_white;
        case card_color_t::Black:   return color_card_black;
        case card_color_t::Red:     return color_card_red;
        case card_color_t::Orange:  return color_card_orange;
        case card_color_t::Yellow:  return color_card_yellow;
        case card_color_t::Green:   return color_card_green;
        case card_color_t::Blue:    return color_card_blue;
        case card_color_t::Rainbow: return color_card_rainbow;
        default:                    return color_card_unknown;
    }
}

ImU32 TracksGame::GetPlayerColor(player_color_t color) const {
    switch(color){
        case player_color_t::Crimson: return color_player_crimson;
        case player_color_t::Navy:    return color_player_navy;
        case player_color_t::Forest:  return color_player_forest;
        case player_color_t::Purple:  return color_player_purple;
        case player_color_t::Teal:    return color_player_teal;
        case player_color_t::Bronze:  return color_player_bronze;
        case player_color_t::Magenta: return color_player_magenta;
        default:                      return color_player_unknown;
    }
}

std::string TracksGame::GetCardColorName(card_color_t color) const {
    switch(color){
        case card_color_t::White:   return "White";
        case card_color_t::Black:   return "Black";
        case card_color_t::Red:     return "Red";
        case card_color_t::Orange:  return "Orange";
        case card_color_t::Yellow:  return "Yellow";
        case card_color_t::Green:   return "Green";
        case card_color_t::Blue:    return "Blue";
        case card_color_t::Rainbow: return "Wild";
        default:                    return "?";
    }
}

std::string TracksGame::GetPlayerColorName(player_color_t color) const {
    switch(color){
        case player_color_t::Crimson: return "Crimson";
        case player_color_t::Navy:    return "Navy";
        case player_color_t::Forest:  return "Forest";
        case player_color_t::Purple:  return "Purple";
        case player_color_t::Teal:    return "Teal";
        case player_color_t::Bronze:  return "Bronze";
        case player_color_t::Magenta: return "Magenta";
        default:                      return "?";
    }
}


bool TracksGame::Display(bool &enabled){
    if(!enabled) return true;

    const auto win_width  = static_cast<int>(std::ceil(window_width)) + 15;
    const auto win_height = static_cast<int>(std::ceil(window_height)) + 60;
    //auto flags = ImGuiWindowFlags_AlwaysAutoResize
    //           | ImGuiWindowFlags_NoScrollWithMouse
    //           | ImGuiWindowFlags_NoNavInputs
    //           | ImGuiWindowFlags_NoScrollbar;
    auto flags = ImGuiWindowFlags_NoScrollWithMouse
               | ImGuiWindowFlags_NoNavInputs
               | ImGuiWindowFlags_NoScrollbar;
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(win_width), static_cast<float>(win_height)), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    ImGui::Begin("Tracks", &enabled, flags);

    const auto f = ImGui::IsWindowFocused();

    // Reset game with R key
    if(f && ImGui::IsKeyPressed(SDL_SCANCODE_R)){
        Reset();
    }

    ImVec2 curr_pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    const auto t_now = std::chrono::steady_clock::now();
    auto t_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_updated).count();
    if(t_diff > 50) t_diff = 50;
    const double dt = static_cast<double>(t_diff) / 1000.0;
    t_updated = t_now;

    // Update message timer
    if(message_timer > 0.0){
        message_timer -= dt;
        if(message_timer <= 0.0) message.clear();
    }

    // Update animations
    UpdateAnimations(dt);

    // ==================== DIFFICULTY SELECTION ====================
    if(phase == game_phase_t::SelectDifficulty){
        // Draw background
        draw_list->AddRectFilled(curr_pos,
            ImVec2(curr_pos.x + window_width, curr_pos.y + window_height),
            color_difficulty_bg);

        // Title
        const char* title = "TRACKS";
        ImVec2 title_size = ImGui::CalcTextSize(title);
        draw_list->AddText(ImVec2(curr_pos.x + window_width/2 - title_size.x/2, curr_pos.y + difficulty_title_y),
            color_title, title);

        const char* subtitle = "A train route-building game";
        ImVec2 sub_size = ImGui::CalcTextSize(subtitle);
        draw_list->AddText(ImVec2(curr_pos.x + window_width/2 - sub_size.x/2, curr_pos.y + difficulty_subtitle_y),
            color_subtitle, subtitle);

        // Difficulty buttons
        const char* diff_text = "Select difficulty (number of AI opponents):";
        ImVec2 diff_size = ImGui::CalcTextSize(diff_text);
        draw_list->AddText(ImVec2(curr_pos.x + window_width/2 - diff_size.x/2, curr_pos.y + difficulty_text_y),
            color_text, diff_text);

        // Draw buttons using ImGui
        ImGui::SetCursorScreenPos(ImVec2(curr_pos.x + difficulty_buttons_start_x, curr_pos.y + difficulty_buttons_y));
        if(ImGui::Button("Easy (3 AI)", ImVec2(difficulty_button_width, difficulty_button_height))){
            StartGame(3);
        }
        ImGui::SameLine();
        if(ImGui::Button("Medium (4 AI)", ImVec2(difficulty_button_width, difficulty_button_height))){
            StartGame(4);
        }
        ImGui::SameLine();
        if(ImGui::Button("Hard (5 AI)", ImVec2(difficulty_button_width, difficulty_button_height))){
            StartGame(5);
        }
        ImGui::SameLine();
        if(ImGui::Button("Expert (6 AI)", ImVec2(difficulty_button_width, difficulty_button_height))){
            StartGame(6);
        }

        // Instructions
        const char* instr[] = {
            "How to play:",
            "- Connect cities by building train tracks",
            "- Collect cards and spend them to claim routes",
            "- Complete your secret objectives for bonus points",
            "- The player with the most points wins!",
            "",
            "Controls:",
            "- Select 1 card from collection OR draw 2 random",
            "- Click track routes on the map to build them",
            "- Add objectives during your turn for bonus points",
            "- Press R to restart the game"
        };

        float y = curr_pos.y + instructions_start_y;
        for(const char* line : instr){
            ImVec2 line_size = ImGui::CalcTextSize(line);
            draw_list->AddText(ImVec2(curr_pos.x + window_width/2 - line_size.x/2, y),
                color_instructions, line);
            y += instruction_line_height;
        }

        ImGui::Dummy(ImVec2(window_width, window_height));
        ImGui::End();
        return true;
    }

    // ==================== MAIN GAME DISPLAY ====================

    // Background
    draw_list->AddRectFilled(curr_pos,
        ImVec2(curr_pos.x + window_width, curr_pos.y + window_height),
        color_background);

    // Map area background
    ImVec2 map_pos = ImVec2(curr_pos.x + map_offset_x, curr_pos.y + map_offset_y);
    draw_list->AddRectFilled(map_pos,
        ImVec2(map_pos.x + map_width, map_pos.y + map_height),
        color_map_background);
    draw_list->AddRect(map_pos,
        ImVec2(map_pos.x + map_width, map_pos.y + map_height),
        color_map_border, 0.0f, 0, 2.0f);

    // ==================== HOVER DETECTION (before rendering to avoid 1-frame lag) ====================
    float panel_x = map_pos.x + map_width + panel_offset_x;
    float panel_y_for_hover = curr_pos.y + map_offset_y;
    ImVec2 mouse = ImGui::GetMousePos();

    // Reset hover states
    hovered_player_idx = -1;
    hovered_objective_idx = -1;

    // Detect player hover (SCORES section)
    if(!game_over && current_player_idx >= 0 && current_player_idx < static_cast<int>(players.size())){
        panel_y_for_hover += turn_indicator_height + phase_indicator_height;
    }
    panel_y_for_hover += score_header_height;  // Skip "SCORES" header

    for(size_t player_i = 0; player_i < players.size(); ++player_i){
        const auto& player = players[player_i];
        std::stringstream ss;
        ss << player.name << ": " << player.score << " pts (" << player.trains_remaining << " trains)";
        std::string score_text = ss.str();
        ImVec2 text_size = ImGui::CalcTextSize(score_text.c_str());

        ImVec2 line_tl(panel_x, panel_y_for_hover);
        ImVec2 line_br(panel_x + text_size.x, panel_y_for_hover + score_line_height);
        if(mouse.x >= line_tl.x && mouse.x <= line_br.x &&
           mouse.y >= line_tl.y && mouse.y <= line_br.y){
            hovered_player_idx = static_cast<int>(player_i);
        }
        panel_y_for_hover += score_line_height;
    }
    panel_y_for_hover += score_section_spacing + objective_header_height;  // Skip spacing and "YOUR OBJECTIVES" header

    // Detect objective hover (YOUR OBJECTIVES section)
    if(!players.empty()){
        for(size_t obj_i = 0; obj_i < players[0].objectives.size(); ++obj_i){
            const auto& obj = players[0].objectives[obj_i];
            std::stringstream ss;
            ss << cities[obj.city_a_idx].name << " - " << cities[obj.city_b_idx].name;
            ss << " (+" << obj.points << ")";
            if(obj.completed) ss << " [DONE]";
            std::string obj_text = ss.str();
            ImVec2 obj_size = ImGui::CalcTextSize(obj_text.c_str());

            ImVec2 obj_tl(panel_x, panel_y_for_hover);
            ImVec2 obj_br(panel_x + obj_size.x, panel_y_for_hover + objective_line_height);
            if(mouse.x >= obj_tl.x && mouse.x <= obj_br.x &&
               mouse.y >= obj_tl.y && mouse.y <= obj_br.y){
                hovered_objective_idx = static_cast<int>(obj_i);
            }
            panel_y_for_hover += objective_line_height;
        }
    }

    // Draw province boundaries
    DrawProvinceBoundaries(draw_list, map_pos);

    // Determine which cities are part of the currently hovered objective route (if any)
    std::set<size_t> highlighted_objective_cities;
    if(hovered_objective_idx >= 0 && !players.empty() &&
       hovered_objective_idx < static_cast<int>(players[0].objectives.size())){
        const auto& obj = players[0].objectives[hovered_objective_idx];
        highlighted_objective_cities.insert(obj.city_a_idx);
        highlighted_objective_cities.insert(obj.city_b_idx);
    }

    // Draw track paths
    hovered_path_idx = -1;
    highlighted_cards.clear();

    for(size_t pi = 0; pi < track_paths.size(); ++pi){
        const auto& path = track_paths[pi];
        const auto& city_a = cities[path.city_a_idx];
        const auto& city_b = cities[path.city_b_idx];

        // Note: y is inverted (1-y) since city coords have y=0 at south, y=1 at north
        ImVec2 pos_a(map_pos.x + static_cast<float>(city_a.pos.x) * map_width,
                     map_pos.y + static_cast<float>(1.0 - city_a.pos.y) * map_height);
        ImVec2 pos_b(map_pos.x + static_cast<float>(city_b.pos.x) * map_width,
                     map_pos.y + static_cast<float>(1.0 - city_b.pos.y) * map_height);

        // Offset parallel routes
        if(path.is_parallel){
            float dx = pos_b.x - pos_a.x;
            float dy = pos_b.y - pos_a.y;
            float len = std::sqrt(dx*dx + dy*dy);
            if(len > 0.01f){
                float nx = -dy / len * 6.0f;
                float ny = dx / len * 6.0f;
                pos_a.x += nx; pos_a.y += ny;
                pos_b.x += nx; pos_b.y += ny;
            }
        }

        // Determine if this track should be dimmed
        bool should_dim = false;

        // Dim ALL tracks if hovering over an objective (only cities are highlighted)
        if(hovered_objective_idx >= 0){
            should_dim = true;
        }

        // Dim if hovering over a player and this path is not owned by them
        if(hovered_player_idx >= 0){
            if(path.owner_player_idx != hovered_player_idx){
                should_dim = true;
            }
        }

        // Determine color
        ImU32 line_color;
        if(path.owner_player_idx >= 0){
            line_color = GetPlayerColor(players[path.owner_player_idx].color);
        } else {
            line_color = GetCardColor(path.color);
        }

        // Apply dimming if needed
        if(should_dim){
            line_color = color_track_dimmed;
        }

        // Draw track line
        float thickness = (path.owner_player_idx >= 0) ? 4.0f : 2.0f;
        // Emphasize highlighted tracks (only for player hover, not objective hover)
        if(hovered_player_idx >= 0 && !should_dim){
            thickness += 1.0f;
        }
        draw_list->AddLine(pos_a, pos_b, line_color, thickness);

        // Draw slots along the track
        float dx = pos_b.x - pos_a.x;
        float dy = pos_b.y - pos_a.y;
        float len = std::sqrt(dx*dx + dy*dy);
        if(len < 0.01f) continue;

        float ndx = dx / len;
        float ndy = dy / len;

        // Check if mouse is hovering this path
        ImVec2 mouse = ImGui::GetMousePos();
        bool hovering = false;
        if(path.owner_player_idx == -1 && !selecting_objective){
            // Check distance from mouse to line segment
            float t = std::clamp(((mouse.x - pos_a.x) * dx + (mouse.y - pos_a.y) * dy) / (len * len), 0.0f, 1.0f);
            float closest_x = pos_a.x + t * dx;
            float closest_y = pos_a.y + t * dy;
            float dist = std::sqrt((mouse.x - closest_x) * (mouse.x - closest_x) +
                                   (mouse.y - closest_y) * (mouse.y - closest_y));
            if(dist < 15.0f){
                hovering = true;
                hovered_path_idx = static_cast<int>(pi);

                // If in build phase and can build this path, highlight the cards that would be spent
                if(phase == game_phase_t::PlayerTurn_Build && CanBuildPath(0, pi)){
                    highlighted_cards = GetCardsToSpendForPath(0, pi);
                }
            }
        }

        // Draw slot rectangles
        float slot_spacing = len / (path.num_slots + 1);
        for(int s = 0; s < path.num_slots; ++s){
            float t_slot = (s + 1) * slot_spacing;
            ImVec2 slot_center(pos_a.x + ndx * t_slot, pos_a.y + ndy * t_slot);

            // Animation progress for this path
            float anim_progress = 1.0f;
            for(const auto& anim : track_animations){
                if(anim.path_idx == pi){
                    anim_progress = static_cast<float>(anim.progress);
                    break;
                }
            }

            // Only draw slots up to animation progress
            float slot_t = static_cast<float>(s + 1) / (path.num_slots + 1);
            if(path.owner_player_idx >= 0 && slot_t > anim_progress) continue;

            ImVec2 slot_half(slot_width/2, slot_height/2);
            ImVec2 slot_tl(slot_center.x - slot_half.x, slot_center.y - slot_half.y);
            ImVec2 slot_br(slot_center.x + slot_half.x, slot_center.y + slot_half.y);

            ImU32 slot_color = line_color;
            if(hovering && path.owner_player_idx == -1){
                slot_color = color_slot_hover;
            }
            draw_list->AddRectFilled(slot_tl, slot_br, slot_color);
            draw_list->AddRect(slot_tl, slot_br, color_slot_border, 0.0f, 0, 1.0f);
        }
    }

    // Draw cities
    for(size_t ci = 0; ci < cities.size(); ++ci){
        const auto& city = cities[ci];
        // Note: y is inverted (1-y) since city coords have y=0 at south, y=1 at north
        ImVec2 pos(map_pos.x + static_cast<float>(city.pos.x) * map_width,
                   map_pos.y + static_cast<float>(1.0 - city.pos.y) * map_height);

        // Determine if city should be highlighted or dimmed (for objective hover)
        bool city_highlighted = (highlighted_objective_cities.count(ci) > 0);
        bool city_dimmed = (hovered_objective_idx >= 0 && !city_highlighted);

        float radius = city_radius;
        ImU32 fill_color = color_city_fill;
        ImU32 border_color = color_city_border;
        ImU32 name_color = color_city_name;

        if(city_highlighted){
            radius = city_radius + 4.0f;
            fill_color = color_city_highlighted;
            border_color = IM_COL32(80, 60, 0, 255);  // Darker border for highlighted
        } else if(city_dimmed){
            fill_color = color_city_dimmed;
            border_color = color_city_dimmed;
            name_color = IM_COL32(100, 100, 100, 150);
        }

        draw_list->AddCircleFilled(pos, radius, fill_color);
        draw_list->AddCircle(pos, radius, border_color, 0, 2.0f);

        // City name (skip for dimmed cities to reduce clutter, show for highlighted)
        if(!city_dimmed || city_highlighted){
            ImVec2 name_size = ImGui::CalcTextSize(city.name.c_str());
            draw_list->AddText(ImVec2(pos.x - name_size.x/2, pos.y + radius + 2),
                name_color, city.name.c_str());
        }
    }


    // ==================== RIGHT PANEL ====================
    // Note: panel_x already calculated during hover detection above
    float panel_y = curr_pos.y + map_offset_y;

    // Current player indicator
    if(!game_over && current_player_idx >= 0 && current_player_idx < static_cast<int>(players.size())){
        const auto& curr_player = players[current_player_idx];
        std::string turn_text = (curr_player.is_human) ? "Your turn" : (curr_player.name + "'s Turn");
        draw_list->AddText(ImVec2(panel_x, panel_y), GetPlayerColor(curr_player.color), turn_text.c_str());
        panel_y += turn_indicator_height;

        // Phase indicator
        std::string phase_text;
        switch(phase){
            case game_phase_t::PlayerTurn_Draw: phase_text = "Select 1 card or Draw 2 random"; break;
            case game_phase_t::PlayerTurn_Build: phase_text = "Build or End Turn"; break;
            case game_phase_t::AITurn: phase_text = "Thinking..."; break;
            default: break;
        }
        draw_list->AddText(ImVec2(panel_x, panel_y), color_text_dim, phase_text.c_str());
        panel_y += phase_indicator_height;
    }

    // Scoreboard (hover already detected above)
    draw_list->AddText(ImVec2(panel_x, panel_y), color_title, "SCORES");
    panel_y += score_header_height;

    for(size_t player_i = 0; player_i < players.size(); ++player_i){
        const auto& player = players[player_i];
        std::stringstream ss;
        ss << player.name << ": " << player.score << " pts (" << player.trains_remaining << " trains)";
        std::string score_text = ss.str();
        ImVec2 text_size = ImGui::CalcTextSize(score_text.c_str());

        // Draw highlight background if hovering
        if(hovered_player_idx == static_cast<int>(player_i)){
            draw_list->AddRectFilled(
                ImVec2(panel_x - 2, panel_y - 1),
                ImVec2(panel_x + text_size.x + 2, panel_y + score_line_height - 1),
                color_hover_background);
        }

        draw_list->AddText(ImVec2(panel_x, panel_y), GetPlayerColor(player.color), score_text.c_str());
        panel_y += score_line_height;
    }
    panel_y += score_section_spacing;

    // Human player's objectives (hover already detected above)
    draw_list->AddText(ImVec2(panel_x, panel_y), color_title, "YOUR OBJECTIVES");
    panel_y += objective_header_height;

    if(!players.empty()){
        for(size_t obj_i = 0; obj_i < players[0].objectives.size(); ++obj_i){
            const auto& obj = players[0].objectives[obj_i];
            std::stringstream ss;
            ss << cities[obj.city_a_idx].name << " - " << cities[obj.city_b_idx].name;
            ss << " (+" << obj.points << ")";

            ImU32 obj_color;
            if(obj.completed){
                obj_color = color_objective_complete;
                ss << " [DONE]";
            } else {
                obj_color = color_objective_pending;
            }

            std::string obj_text = ss.str();
            ImVec2 obj_size = ImGui::CalcTextSize(obj_text.c_str());

            // Draw highlight background if hovering
            if(hovered_objective_idx == static_cast<int>(obj_i)){
                draw_list->AddRectFilled(
                    ImVec2(panel_x - 2, panel_y - 1),
                    ImVec2(panel_x + obj_size.x + 2, panel_y + objective_line_height - 1),
                    color_hover_background);
            }

            draw_list->AddText(ImVec2(panel_x, panel_y), obj_color, obj_text.c_str());
            panel_y += objective_line_height;
        }
    }
    panel_y += objective_section_spacing;

    // Add Objective button (only during player's turn, once per turn)
    if((phase == game_phase_t::PlayerTurn_Draw || phase == game_phase_t::PlayerTurn_Build) &&
       !has_added_objective_this_turn && !selecting_objective && !objective_deck.empty()){
        ImGui::SetCursorScreenPos(ImVec2(panel_x, panel_y));
        if(ImGui::Button("Add Objective", ImVec2(add_objective_button_width, add_objective_button_height))){
            PresentObjectiveChoices();
        }
        panel_y += add_objective_button_height + 10;
    }

    // Message display
    if(!message.empty()){
        draw_list->AddText(ImVec2(panel_x, panel_y), color_message, message.c_str());
        panel_y += message_height;
    }

    // ==================== BOTTOM PANEL - CARDS ====================
    float cards_y = map_pos.y + map_height + cards_section_offset_y;

    // Card collection (face-up cards)
    std::string collection_label = "CARD COLLECTION";
    if(phase == game_phase_t::PlayerTurn_Draw && cards_drawn_this_turn == 0){
        collection_label += " (select 1 ends draw)";
    }
    draw_list->AddText(ImVec2(curr_pos.x + map_offset_x, cards_y), color_title, collection_label.c_str());
    cards_y += cards_header_height;

    // FIX for issue #2: Player can EITHER select 1 card from collection OR draw 2 random
    // Selecting from collection immediately ends the draw phase
    // Also block card selection when objective selection overlay is active (modal)
    bool can_select_from_collection = (phase == game_phase_t::PlayerTurn_Draw && cards_drawn_this_turn == 0 && !selecting_objective);
    bool can_draw_random = (phase == game_phase_t::PlayerTurn_Draw && !selecting_objective);

    ImGui::SetCursorScreenPos(ImVec2(curr_pos.x + map_offset_x, cards_y));
    for(size_t ci = 0; ci < collection.size(); ++ci){
        const auto& card = collection[ci];
        ImU32 card_col = GetCardColor(card.color);

        std::string btn_label = GetCardColorName(card.color) + "##col" + std::to_string(ci);

        if(can_select_from_collection){
            // Normal button colors
            ImGui::PushStyleColor(ImGuiCol_Button, card_col);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(
                std::min(255, static_cast<int>((card_col >> 0) & 0xFF) + 40),
                std::min(255, static_cast<int>((card_col >> 8) & 0xFF) + 40),
                std::min(255, static_cast<int>((card_col >> 16) & 0xFF) + 40),
                255));
            ImGui::PushStyleColor(ImGuiCol_Text,
                (card.color == card_color_t::White || card.color == card_color_t::Yellow) ?
                    color_text_light_bg : color_text_dark_bg);

            if(ImGui::Button(btn_label.c_str(), ImVec2(card_width, card_height))){
                DrawCard(0, card.color);
                collection.erase(collection.begin() + static_cast<long>(ci));
                RefillCollection();
                // Selecting 1 card from collection ends the draw phase
                phase = game_phase_t::PlayerTurn_Build;
                message = "You selected 1 card.\nYou may build a track or end your turn.";
                message_timer = message_display_time;
            }
        } else {
            // Disabled button - grayed out with matching hover state
            ImGui::PushStyleColor(ImGuiCol_Button, color_button_disabled);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color_button_disabled);
            ImGui::PushStyleColor(ImGuiCol_Text, color_text_disabled);
            ImGui::Button(btn_label.c_str(), ImVec2(card_width, card_height));
        }

        ImGui::PopStyleColor(3);
        ImGui::SameLine();
    }

    // Random draw button - draws up to 2 random cards
    if(can_draw_random && cards_drawn_this_turn < 2){
        ImGui::PushStyleColor(ImGuiCol_Button, color_button_random);
        std::string random_label = (cards_drawn_this_turn == 0) ? "Draw\nRandom" : "Draw 1\nMore";
        if(ImGui::Button(random_label.c_str(), ImVec2(card_width, card_height))){
            DrawRandomCard(0);
            cards_drawn_this_turn++;
            if(cards_drawn_this_turn >= 2){
                phase = game_phase_t::PlayerTurn_Build;
                message = "You drew 2 random cards.\nYou may build a track or end your turn.";
                message_timer = message_display_time;
            } else {
                message = "Drew 1 random card.\nClick 'Draw 1 More' for your second card.";
                message_timer = message_display_time;
            }
        }
        ImGui::PopStyleColor();
    }

    // Player's hand
    cards_y += card_height + cards_hand_offset_y;
    draw_list->AddText(ImVec2(curr_pos.x + map_offset_x, cards_y), color_title, "YOUR HAND");
    cards_y += cards_header_height;

    // Count cards by color and track which would be highlighted
    std::map<card_color_t, int> hand_counts;
    std::map<card_color_t, int> highlight_counts;  // How many of each color to highlight
    if(!players.empty()){
        for(const auto& card : players[0].hand){
            hand_counts[card.color]++;
        }
        // Count highlighted cards by color
        for(size_t idx : highlighted_cards){
            if(idx < players[0].hand.size()){
                highlight_counts[players[0].hand[idx].color]++;
            }
        }
    }

    ImGui::SetCursorScreenPos(ImVec2(curr_pos.x + map_offset_x, cards_y));
    for(int c = 0; c <= static_cast<int>(card_color_t::Rainbow); ++c){
        card_color_t color = static_cast<card_color_t>(c);
        int count = hand_counts[color];
        if(count == 0) continue;

        ImU32 card_col = GetCardColor(color);
        int hl_count = highlight_counts[color];

        // Build label showing total and highlighted count
        std::string label;
        if(hl_count > 0){
            label = GetCardColorName(color) + ": " + std::to_string(count) + " (" + std::to_string(hl_count) + ")";
        } else {
            label = GetCardColorName(color) + ": " + std::to_string(count);
        }

        ImVec2 card_pos = ImGui::GetCursorScreenPos();

        // Draw highlight border if cards of this color would be spent
        if(hl_count > 0){
            draw_list->AddRect(
                ImVec2(card_pos.x - 2, card_pos.y - 2),
                ImVec2(card_pos.x + card_width + 2, card_pos.y + card_height/2 + 2),
                color_card_highlight, 0.0f, 0, 3.0f);
        }

        draw_list->AddRectFilled(
            card_pos,
            ImVec2(card_pos.x + card_width, card_pos.y + card_height/2),
            card_col);
        draw_list->AddRect(
            card_pos,
            ImVec2(card_pos.x + card_width, card_pos.y + card_height/2),
            color_card_border);
        draw_list->AddText(
            ImVec2(card_pos.x + 3, card_pos.y + 5),
            (color == card_color_t::White || color == card_color_t::Yellow) ?
                color_text_light_bg : color_text_dark_bg,
            label.c_str());

        ImGui::SetCursorScreenPos(ImVec2(card_pos.x + card_width + card_spacing, cards_y));
    }


    // ==================== BUILD INTERACTION ====================
    if(phase == game_phase_t::PlayerTurn_Build && !selecting_objective){
        // Show hovered path info
        if(hovered_path_idx >= 0 && hovered_path_idx < static_cast<int>(track_paths.size())){
            const auto& path = track_paths[hovered_path_idx];
            std::stringstream info;
            info << cities[path.city_a_idx].name << " - " << cities[path.city_b_idx].name;
            info << " | " << path.num_slots << " " << GetCardColorName(path.color) << " cards";
            info << " | +" << track_points[path.num_slots] << " pts";

            if(CanBuildPath(0, hovered_path_idx)){
                info << " [Click to build]";
            } else {
                info << " [Not enough cards]";
            }

            draw_list->AddText(ImVec2(curr_pos.x + map_offset_x, curr_pos.y + window_height - build_info_offset_y),
                color_build_info, info.str().c_str());

            // Build on click
            if(ImGui::IsMouseClicked(0) && CanBuildPath(0, hovered_path_idx)){
                BuildPath(0, hovered_path_idx);
            } else if(ImGui::IsMouseClicked(0) && !CanBuildPath(0, hovered_path_idx)){
                message = "Not enough cards to build this route!";
                message_timer = message_display_time;
            }
        }

        // End turn button
        ImGui::SetCursorScreenPos(ImVec2(panel_x, curr_pos.y + window_height - end_turn_button_offset_y));
        if(ImGui::Button("End Turn", ImVec2(end_turn_button_width, end_turn_button_height))){
            NextTurn();
        }
    }

    // ==================== OBJECTIVE SELECTION OVERLAY ====================
    if(selecting_objective && !objective_choices.empty()){
        // Semi-transparent overlay
        draw_list->AddRectFilled(curr_pos,
            ImVec2(curr_pos.x + window_width, curr_pos.y + window_height),
            color_game_over_overlay);

        // Title
        const char* select_title = "SELECT AN OBJECTIVE";
        ImVec2 title_size = ImGui::CalcTextSize(select_title);
        draw_list->AddText(ImVec2(curr_pos.x + window_width/2 - title_size.x/2, curr_pos.y + game_over_title_y),
            color_title, select_title);

        const char* select_subtitle = "You must choose one of these objectives:";
        ImVec2 sub_size = ImGui::CalcTextSize(select_subtitle);
        draw_list->AddText(ImVec2(curr_pos.x + window_width/2 - sub_size.x/2, curr_pos.y + objective_selection_subtitle_y),
            color_text, select_subtitle);

        // Display 3 objective choices as buttons
        float choice_y = curr_pos.y + objective_selection_choices_y;
        for(size_t i = 0; i < objective_choices.size(); ++i){
            const auto& obj = objective_choices[i];
            std::stringstream ss;
            ss << cities[obj.city_a_idx].name << " to " << cities[obj.city_b_idx].name;
            ss << " (+" << obj.points << " points)";
            // Ensure ImGui widget IDs are unique even if labels collide
            std::string button_label = ss.str();
            button_label += "##";
            button_label += std::to_string(i);

            ImGui::SetCursorScreenPos(ImVec2(curr_pos.x + window_width/2 - objective_choice_button_width/2, choice_y));
            if(ImGui::Button(button_label.c_str(), ImVec2(objective_choice_button_width, objective_choice_button_height))){
                // Player selected this objective
                if(!players.empty()){
                    players[0].objectives.push_back(obj);
                    // Remove from objective deck
                    for(auto it = objective_deck.begin(); it != objective_deck.end(); ++it){
                        if(it->city_a_idx == obj.city_a_idx && it->city_b_idx == obj.city_b_idx){
                            objective_deck.erase(it);
                            break;
                        }
                    }
                    // Check if the newly added objective is already completed
                    UpdateAllObjectives();
                }
                selecting_objective = false;
                has_added_objective_this_turn = true;
                objective_choices.clear();
                message = "New objective added!";
                message_timer = message_display_time;
            }
            choice_y += objective_choice_button_height + objective_choice_spacing;
        }
    }

    // ==================== AI TURN PROCESSING ====================
    if(phase == game_phase_t::AITurn){
        ai_delay_timer -= dt;
        if(ai_delay_timer <= 0.0){
            ProcessAITurn();
        }
    }


    // ==================== GAME OVER DISPLAY ====================
    if(phase == game_phase_t::GameOver){
        // Semi-transparent overlay
        draw_list->AddRectFilled(curr_pos,
            ImVec2(curr_pos.x + window_width, curr_pos.y + window_height),
            color_game_over_overlay);

        // Game Over text
        const char* go_text = "GAME OVER";
        ImVec2 go_size = ImGui::CalcTextSize(go_text);
        draw_list->AddText(ImVec2(curr_pos.x + window_width/2 - go_size.x/2, curr_pos.y + game_over_title_y),
            color_title, go_text);

        // Winner announcement
        int winner = GetWinnerIdx();
        std::string winner_text;
        ImU32 winner_color;
        if(winner == 0){
            winner_text = "YOU WIN!";
            winner_color = color_win_text;
        } else {
            winner_text = players[winner].name + " wins!";
            winner_color = color_lose_text;
        }
        ImVec2 wt_size = ImGui::CalcTextSize(winner_text.c_str());
        draw_list->AddText(ImVec2(curr_pos.x + window_width/2 - wt_size.x/2, curr_pos.y + game_over_winner_y),
            winner_color, winner_text.c_str());

        // Final scores
        float score_y = curr_pos.y + game_over_scores_header_y;
        const char* fs_text = "Final Scores:";
        ImVec2 fs_size = ImGui::CalcTextSize(fs_text);
        draw_list->AddText(ImVec2(curr_pos.x + window_width/2 - fs_size.x/2, score_y),
            color_text, fs_text);
        score_y += game_over_scores_line_height;

        // Sort players by score for display
        std::vector<std::pair<int, int>> sorted_scores;
        for(size_t i = 0; i < players.size(); ++i){
            sorted_scores.push_back({static_cast<int>(i), players[i].score});
        }
        std::sort(sorted_scores.begin(), sorted_scores.end(),
            [](const auto& a, const auto& b){ return a.second > b.second; });

        for(const auto& [idx, score] : sorted_scores){
            std::stringstream ss;
            ss << players[idx].name << ": " << score << " points";
            ImVec2 line_size = ImGui::CalcTextSize(ss.str().c_str());
            draw_list->AddText(
                ImVec2(curr_pos.x + window_width/2 - line_size.x/2, score_y),
                GetPlayerColor(players[idx].color),
                ss.str().c_str());
            score_y += game_over_score_line_height;
        }


        // Objective details for human player
        score_y += game_over_objectives_offset_y;
        const char* obj_header = "Your Objectives:";
        ImVec2 oh_size = ImGui::CalcTextSize(obj_header);
        draw_list->AddText(ImVec2(curr_pos.x + window_width/2 - oh_size.x/2, score_y),
            color_text, obj_header);
        score_y += game_over_score_line_height;

        if(!players.empty()){
            for(const auto& obj : players[0].objectives){
                std::stringstream ss;
                ss << cities[obj.city_a_idx].name << " - " << cities[obj.city_b_idx].name;
                if(obj.completed){
                    ss << " [COMPLETED +" << obj.points << "]";
                } else {
                    ss << " [FAILED -" << obj.points << "]";
                }
                ImVec2 obj_size = ImGui::CalcTextSize(ss.str().c_str());
                draw_list->AddText(
                    ImVec2(curr_pos.x + window_width/2 - obj_size.x/2, score_y),
                    obj.completed ? color_objective_complete : color_objective_failed,
                    ss.str().c_str());
                score_y += game_over_objective_line_height;
            }
        }

        // Restart button
        ImGui::SetCursorScreenPos(ImVec2(curr_pos.x + window_width/2 - restart_button_width/2, score_y + restart_button_offset_y));
        if(ImGui::Button("Play Again", ImVec2(restart_button_width, restart_button_height))){
            Reset();
        }
    }

    // Draw card animations
    for(const auto& anim : card_animations){
        float t = static_cast<float>(anim.progress);
        float x = static_cast<float>(anim.start_pos.x + (anim.end_pos.x - anim.start_pos.x) * t);
        float y = static_cast<float>(anim.start_pos.y + (anim.end_pos.y - anim.start_pos.y) * t);
        float scale = 1.0f + 0.2f * std::sin(t * 3.14159f);

        ImVec2 card_tl(x - card_width/2 * scale, y - card_height/2 * scale);
        ImVec2 card_br(x + card_width/2 * scale, y + card_height/2 * scale);
        draw_list->AddRectFilled(card_tl, card_br, GetCardColor(anim.color));
        draw_list->AddRect(card_tl, card_br, color_card_border, 0.0f, 0, 2.0f);
    }

    ImGui::Dummy(ImVec2(window_width, window_height));
    ImGui::End();
    return true;
}

