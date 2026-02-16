// Tracks.cc - A part of DICOMautomaton 2026. Written by hal clark.
// A Ticket to Ride-inspired train game set in British Columbia and Alberta.

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

    t_updated = std::chrono::steady_clock::now();
    t_turn_started = t_updated;

    InitializeCities();
    InitializeTrackPaths();
    InitializeDeck();
}

void TracksGame::InitializeCities(){
    // Cities positioned to reflect geography of BC and Alberta
    // Coordinates are normalized (0-1) and will be scaled during rendering
    // X increases eastward, Y increases southward

    cities.clear();

    // British Columbia - Coast
    cities.push_back({"Vancouver", {0.12, 0.72}});
    cities.push_back({"Victoria", {0.08, 0.82}});
    cities.push_back({"Nanaimo", {0.10, 0.68}});
    cities.push_back({"Prince Rupert", {0.05, 0.22}});
    cities.push_back({"Kitimat", {0.10, 0.28}});

    // British Columbia - Interior
    cities.push_back({"Kamloops", {0.25, 0.62}});
    cities.push_back({"Kelowna", {0.30, 0.70}});
    cities.push_back({"Vernon", {0.32, 0.64}});
    cities.push_back({"Penticton", {0.30, 0.76}});
    cities.push_back({"Cranbrook", {0.45, 0.78}});
    cities.push_back({"Nelson", {0.40, 0.75}});
    cities.push_back({"Trail", {0.38, 0.80}});
    cities.push_back({"Revelstoke", {0.38, 0.58}});

    // British Columbia - North
    cities.push_back({"Prince George", {0.22, 0.38}});
    cities.push_back({"Quesnel", {0.20, 0.45}});
    cities.push_back({"Williams Lake", {0.18, 0.52}});
    cities.push_back({"Fort St. John", {0.45, 0.15}});
    cities.push_back({"Dawson Creek", {0.48, 0.18}});
    cities.push_back({"Fort Nelson", {0.42, 0.05}});

    // Alberta - South
    cities.push_back({"Calgary", {0.62, 0.65}});
    cities.push_back({"Lethbridge", {0.65, 0.80}});
    cities.push_back({"Medicine Hat", {0.78, 0.78}});
    cities.push_back({"Red Deer", {0.60, 0.52}});
    cities.push_back({"Banff", {0.52, 0.62}});

    // Alberta - Central/North
    cities.push_back({"Edmonton", {0.65, 0.40}});
    cities.push_back({"Grande Prairie", {0.55, 0.22}});
    cities.push_back({"Fort McMurray", {0.75, 0.18}});
    cities.push_back({"Jasper", {0.48, 0.48}});
    cities.push_back({"Lloydminster", {0.82, 0.42}});
    cities.push_back({"Wetaskiwin", {0.63, 0.46}});

    // Additional connection points
    cities.push_back({"Golden", {0.42, 0.60}});
    cities.push_back({"Fernie", {0.48, 0.75}});
    cities.push_back({"Hinton", {0.52, 0.45}});
    cities.push_back({"Whitecourt", {0.55, 0.35}});
    cities.push_back({"Slave Lake", {0.62, 0.28}});
    cities.push_back({"High Level", {0.58, 0.08}});
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
    t_turn_started = std::chrono::steady_clock::now();
    message = "Your turn! Draw 2 cards or 1 random card.";
    message_timer = message_display_time;
}

void TracksGame::NextTurn(){
    cards_drawn_this_turn = 0;
    has_built_this_turn = false;

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
        message = "Your turn! Draw 2 cards or 1 random card.";
        message_timer = message_display_time;
    } else {
        phase = game_phase_t::AITurn;
        ai_delay_timer = ai_turn_delay;
    }
}

bool TracksGame::CheckGameEnd(){
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
        case card_color_t::White:   return IM_COL32(240, 240, 240, 255);
        case card_color_t::Black:   return IM_COL32(40, 40, 40, 255);
        case card_color_t::Red:     return IM_COL32(220, 50, 50, 255);
        case card_color_t::Orange:  return IM_COL32(240, 140, 40, 255);
        case card_color_t::Yellow:  return IM_COL32(240, 220, 40, 255);
        case card_color_t::Green:   return IM_COL32(50, 180, 50, 255);
        case card_color_t::Blue:    return IM_COL32(50, 100, 220, 255);
        case card_color_t::Rainbow: return IM_COL32(200, 100, 200, 255);
        default:                    return IM_COL32(128, 128, 128, 255);
    }
}

ImU32 TracksGame::GetPlayerColor(player_color_t color) const {
    switch(color){
        case player_color_t::Crimson: return IM_COL32(180, 30, 30, 255);
        case player_color_t::Navy:    return IM_COL32(30, 50, 140, 255);
        case player_color_t::Forest:  return IM_COL32(30, 100, 30, 255);
        case player_color_t::Purple:  return IM_COL32(120, 40, 160, 255);
        case player_color_t::Teal:    return IM_COL32(30, 140, 140, 255);
        case player_color_t::Bronze:  return IM_COL32(160, 100, 40, 255);
        case player_color_t::Magenta: return IM_COL32(180, 50, 130, 255);
        default:                      return IM_COL32(128, 128, 128, 255);
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
    auto flags = ImGuiWindowFlags_AlwaysAutoResize
               | ImGuiWindowFlags_NoScrollWithMouse
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
            IM_COL32(30, 40, 50, 255));

        // Title
        const char* title = "TRACKS";
        ImVec2 title_size = ImGui::CalcTextSize(title);
        draw_list->AddText(ImVec2(curr_pos.x + window_width/2 - title_size.x/2, curr_pos.y + 80),
            IM_COL32(255, 220, 100, 255), title);

        const char* subtitle = "A train route-building game";
        ImVec2 sub_size = ImGui::CalcTextSize(subtitle);
        draw_list->AddText(ImVec2(curr_pos.x + window_width/2 - sub_size.x/2, curr_pos.y + 110),
            IM_COL32(180, 180, 180, 255), subtitle);

        // Difficulty buttons
        const char* diff_text = "Select difficulty (number of AI opponents):";
        ImVec2 diff_size = ImGui::CalcTextSize(diff_text);
        draw_list->AddText(ImVec2(curr_pos.x + window_width/2 - diff_size.x/2, curr_pos.y + 200),
            IM_COL32(220, 220, 220, 255), diff_text);

        // Draw buttons using ImGui
        ImGui::SetCursorScreenPos(ImVec2(curr_pos.x + 200, curr_pos.y + 250));
        if(ImGui::Button("Easy (3 AI)", ImVec2(120, 40))){
            StartGame(3);
        }
        ImGui::SameLine();
        if(ImGui::Button("Medium (4 AI)", ImVec2(120, 40))){
            StartGame(4);
        }
        ImGui::SameLine();
        if(ImGui::Button("Hard (5 AI)", ImVec2(120, 40))){
            StartGame(5);
        }
        ImGui::SameLine();
        if(ImGui::Button("Expert (6 AI)", ImVec2(120, 40))){
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
            "- Click cards in the collection to draw them",
            "- Click 'Draw Random' for a random card",
            "- Click track routes on the map to build them",
            "- Press R to restart the game"
        };

        float y = curr_pos.y + 350;
        for(const char* line : instr){
            ImVec2 line_size = ImGui::CalcTextSize(line);
            draw_list->AddText(ImVec2(curr_pos.x + window_width/2 - line_size.x/2, y),
                IM_COL32(200, 200, 200, 255), line);
            y += 20;
        }

        ImGui::Dummy(ImVec2(window_width, window_height));
        ImGui::End();
        return true;
    }

    // ==================== MAIN GAME DISPLAY ====================

    // Background
    draw_list->AddRectFilled(curr_pos,
        ImVec2(curr_pos.x + window_width, curr_pos.y + window_height),
        IM_COL32(40, 50, 60, 255));

    // Map area background
    ImVec2 map_pos = ImVec2(curr_pos.x + 10, curr_pos.y + 10);
    draw_list->AddRectFilled(map_pos,
        ImVec2(map_pos.x + map_width, map_pos.y + map_height),
        IM_COL32(60, 80, 70, 255));
    draw_list->AddRect(map_pos,
        ImVec2(map_pos.x + map_width, map_pos.y + map_height),
        IM_COL32(100, 120, 110, 255), 0.0f, 0, 2.0f);

    // Draw track paths
    hovered_path_idx = -1;
    for(size_t pi = 0; pi < track_paths.size(); ++pi){
        const auto& path = track_paths[pi];
        const auto& city_a = cities[path.city_a_idx];
        const auto& city_b = cities[path.city_b_idx];

        ImVec2 pos_a(map_pos.x + static_cast<float>(city_a.pos.x) * map_width,
                     map_pos.y + static_cast<float>(city_a.pos.y) * map_height);
        ImVec2 pos_b(map_pos.x + static_cast<float>(city_b.pos.x) * map_width,
                     map_pos.y + static_cast<float>(city_b.pos.y) * map_height);

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

        // Determine color
        ImU32 line_color;
        if(path.owner_player_idx >= 0){
            line_color = GetPlayerColor(players[path.owner_player_idx].color);
        } else {
            line_color = GetCardColor(path.color);
        }

        // Draw track line
        float thickness = (path.owner_player_idx >= 0) ? 4.0f : 2.0f;
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
        if(path.owner_player_idx == -1){
            // Check distance from mouse to line segment
            float t = std::clamp(((mouse.x - pos_a.x) * dx + (mouse.y - pos_a.y) * dy) / (len * len), 0.0f, 1.0f);
            float closest_x = pos_a.x + t * dx;
            float closest_y = pos_a.y + t * dy;
            float dist = std::sqrt((mouse.x - closest_x) * (mouse.x - closest_x) +
                                   (mouse.y - closest_y) * (mouse.y - closest_y));
            if(dist < 15.0f){
                hovering = true;
                hovered_path_idx = static_cast<int>(pi);
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
                slot_color = IM_COL32(255, 255, 150, 255);
            }
            draw_list->AddRectFilled(slot_tl, slot_br, slot_color);
            draw_list->AddRect(slot_tl, slot_br, IM_COL32(0, 0, 0, 180), 0.0f, 0, 1.0f);
        }
    }

    // Draw cities
    for(const auto& city : cities){
        ImVec2 pos(map_pos.x + static_cast<float>(city.pos.x) * map_width,
                   map_pos.y + static_cast<float>(city.pos.y) * map_height);
        draw_list->AddCircleFilled(pos, city_radius, IM_COL32(220, 200, 180, 255));
        draw_list->AddCircle(pos, city_radius, IM_COL32(60, 40, 30, 255), 0, 2.0f);

        // City name
        ImVec2 name_size = ImGui::CalcTextSize(city.name.c_str());
        draw_list->AddText(ImVec2(pos.x - name_size.x/2, pos.y + city_radius + 2),
            IM_COL32(255, 255, 255, 220), city.name.c_str());
    }


    // ==================== RIGHT PANEL ====================
    float panel_x = map_pos.x + map_width + 20;
    float panel_y = curr_pos.y + 10;

    // Current player indicator
    if(!game_over && current_player_idx >= 0 && current_player_idx < static_cast<int>(players.size())){
        const auto& curr_player = players[current_player_idx];
        std::string turn_text = curr_player.name + "'s Turn";
        draw_list->AddText(ImVec2(panel_x, panel_y), GetPlayerColor(curr_player.color), turn_text.c_str());
        panel_y += 25;

        // Phase indicator
        std::string phase_text;
        switch(phase){
            case game_phase_t::PlayerTurn_Draw: phase_text = "Draw cards"; break;
            case game_phase_t::PlayerTurn_Build: phase_text = "Build or End Turn"; break;
            case game_phase_t::AITurn: phase_text = "Thinking..."; break;
            default: break;
        }
        draw_list->AddText(ImVec2(panel_x, panel_y), IM_COL32(180, 180, 180, 255), phase_text.c_str());
        panel_y += 25;
    }

    // Scoreboard
    draw_list->AddText(ImVec2(panel_x, panel_y), IM_COL32(255, 220, 100, 255), "SCORES");
    panel_y += 20;
    for(const auto& player : players){
        std::stringstream ss;
        ss << player.name << ": " << player.score << " pts (" << player.trains_remaining << " trains)";
        draw_list->AddText(ImVec2(panel_x, panel_y), GetPlayerColor(player.color), ss.str().c_str());
        panel_y += 18;
    }
    panel_y += 15;

    // Human player's objectives
    draw_list->AddText(ImVec2(panel_x, panel_y), IM_COL32(255, 220, 100, 255), "YOUR OBJECTIVES");
    panel_y += 20;
    if(!players.empty()){
        for(const auto& obj : players[0].objectives){
            std::stringstream ss;
            ss << cities[obj.city_a_idx].name << " - " << cities[obj.city_b_idx].name;
            ss << " (" << (obj.points > 0 ? "+" : "") << obj.points << ")";

            ImU32 obj_color;
            if(obj.completed){
                obj_color = IM_COL32(100, 255, 100, 255);
                ss << " [DONE]";
            } else {
                obj_color = IM_COL32(200, 200, 200, 255);
            }
            draw_list->AddText(ImVec2(panel_x, panel_y), obj_color, ss.str().c_str());
            panel_y += 18;
        }
    }
    panel_y += 15;

    // Message display
    if(!message.empty()){
        draw_list->AddText(ImVec2(panel_x, panel_y), IM_COL32(255, 255, 150, 255), message.c_str());
        panel_y += 25;
    }

    // ==================== BOTTOM PANEL - CARDS ====================
    float cards_y = map_pos.y + map_height + 20;

    // Card collection (face-up cards)
    draw_list->AddText(ImVec2(curr_pos.x + 10, cards_y), IM_COL32(255, 220, 100, 255), "CARD COLLECTION");
    cards_y += 20;

    bool can_draw = (phase == game_phase_t::PlayerTurn_Draw && cards_drawn_this_turn < 2);

    ImGui::SetCursorScreenPos(ImVec2(curr_pos.x + 10, cards_y));
    for(size_t ci = 0; ci < collection.size(); ++ci){
        const auto& card = collection[ci];
        ImU32 card_col = GetCardColor(card.color);

        // Gray out cards when player can't draw
        if(!can_draw){
            card_col = IM_COL32(80, 80, 80, 200);
        }

        std::string btn_label = GetCardColorName(card.color) + "##col" + std::to_string(ci);
        ImGui::PushStyleColor(ImGuiCol_Button, card_col);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, can_draw ? IM_COL32(
            std::min(255, static_cast<int>((card_col >> 0) & 0xFF) + 40),
            std::min(255, static_cast<int>((card_col >> 8) & 0xFF) + 40),
            std::min(255, static_cast<int>((card_col >> 16) & 0xFF) + 40),
            255) : card_col);
        ImGui::PushStyleColor(ImGuiCol_Text,
            can_draw && (card.color == card_color_t::White || card.color == card_color_t::Yellow) ?
                IM_COL32(0, 0, 0, 255) : IM_COL32(255, 255, 255, 255));

        if(can_draw){
            if(ImGui::Button(btn_label.c_str(), ImVec2(card_width, card_height))){
                DrawCard(0, card.color);
                collection.erase(collection.begin() + static_cast<long>(ci));
                RefillCollection();
                cards_drawn_this_turn++;
                if(cards_drawn_this_turn >= 2){
                    phase = game_phase_t::PlayerTurn_Build;
                    message = "You may build a track or end your turn.";
                    message_timer = message_display_time;
                }
            }
        } else {
            // Render disabled button (no click action)
            ImGui::Button(btn_label.c_str(), ImVec2(card_width, card_height));
        }

        ImGui::PopStyleColor(3);
        ImGui::SameLine();
    }

    // Random draw button
    if(can_draw){
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(100, 100, 100, 255));
        if(ImGui::Button("Random", ImVec2(card_width, card_height))){
            DrawRandomCard(0);
            cards_drawn_this_turn += 2;  // Random counts as 2 draws
            phase = game_phase_t::PlayerTurn_Build;
            message = "You drew a random card. You may build a track or end your turn.";
            message_timer = message_display_time;
        }
        ImGui::PopStyleColor();
    }

    // Player's hand
    cards_y += card_height + 30;
    draw_list->AddText(ImVec2(curr_pos.x + 10, cards_y), IM_COL32(255, 220, 100, 255), "YOUR HAND");
    cards_y += 20;

    // Count cards by color
    std::map<card_color_t, int> hand_counts;
    if(!players.empty()){
        for(const auto& card : players[0].hand){
            hand_counts[card.color]++;
        }
    }

    ImGui::SetCursorScreenPos(ImVec2(curr_pos.x + 10, cards_y));
    for(int c = 0; c <= static_cast<int>(card_color_t::Rainbow); ++c){
        card_color_t color = static_cast<card_color_t>(c);
        int count = hand_counts[color];
        if(count == 0) continue;

        ImU32 card_col = GetCardColor(color);
        std::string label = GetCardColorName(color) + ": " + std::to_string(count);

        draw_list->AddRectFilled(
            ImGui::GetCursorScreenPos(),
            ImVec2(ImGui::GetCursorScreenPos().x + card_width,
                   ImGui::GetCursorScreenPos().y + card_height/2),
            card_col);
        draw_list->AddRect(
            ImGui::GetCursorScreenPos(),
            ImVec2(ImGui::GetCursorScreenPos().x + card_width,
                   ImGui::GetCursorScreenPos().y + card_height/2),
            IM_COL32(0, 0, 0, 200));
        draw_list->AddText(
            ImVec2(ImGui::GetCursorScreenPos().x + 3, ImGui::GetCursorScreenPos().y + 5),
            (color == card_color_t::White || color == card_color_t::Yellow) ?
                IM_COL32(0, 0, 0, 255) : IM_COL32(255, 255, 255, 255),
            label.c_str());

        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x + card_width + 10,
                                          cards_y));
    }


    // ==================== BUILD INTERACTION ====================
    if(phase == game_phase_t::PlayerTurn_Build){
        // Show hovered path info
        if(hovered_path_idx >= 0 && hovered_path_idx < static_cast<int>(track_paths.size())){
            const auto& path = track_paths[hovered_path_idx];
            std::stringstream info;
            info << cities[path.city_a_idx].name << " - " << cities[path.city_b_idx].name;
            info << " | " << path.num_slots << " " << GetCardColorName(path.color) << " cards";
            info << " | +" << track_points[path.num_slots] << " pts";

            draw_list->AddText(ImVec2(curr_pos.x + 10, curr_pos.y + window_height - 30),
                IM_COL32(255, 255, 200, 255), info.str().c_str());

            // Build on click
            if(ImGui::IsMouseClicked(0) && CanBuildPath(0, hovered_path_idx)){
                BuildPath(0, hovered_path_idx);
            } else if(ImGui::IsMouseClicked(0) && !CanBuildPath(0, hovered_path_idx)){
                message = "Not enough cards to build this route!";
                message_timer = message_display_time;
            }
        }

        // End turn button
        ImGui::SetCursorScreenPos(ImVec2(panel_x, curr_pos.y + window_height - 50));
        if(ImGui::Button("End Turn", ImVec2(100, 35))){
            NextTurn();
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
            IM_COL32(0, 0, 0, 180));

        // Game Over text
        const char* go_text = "GAME OVER";
        ImVec2 go_size = ImGui::CalcTextSize(go_text);
        draw_list->AddText(ImVec2(curr_pos.x + window_width/2 - go_size.x/2, curr_pos.y + 150),
            IM_COL32(255, 220, 100, 255), go_text);

        // Winner announcement
        int winner = GetWinnerIdx();
        std::string winner_text;
        ImU32 winner_color;
        if(winner == 0){
            winner_text = "YOU WIN!";
            winner_color = IM_COL32(100, 255, 100, 255);
        } else {
            winner_text = players[winner].name + " wins!";
            winner_color = IM_COL32(255, 100, 100, 255);
        }
        ImVec2 wt_size = ImGui::CalcTextSize(winner_text.c_str());
        draw_list->AddText(ImVec2(curr_pos.x + window_width/2 - wt_size.x/2, curr_pos.y + 200),
            winner_color, winner_text.c_str());

        // Final scores
        float score_y = curr_pos.y + 260;
        const char* fs_text = "Final Scores:";
        ImVec2 fs_size = ImGui::CalcTextSize(fs_text);
        draw_list->AddText(ImVec2(curr_pos.x + window_width/2 - fs_size.x/2, score_y),
            IM_COL32(220, 220, 220, 255), fs_text);
        score_y += 30;

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
            score_y += 25;
        }


        // Objective details for human player
        score_y += 20;
        const char* obj_header = "Your Objectives:";
        ImVec2 oh_size = ImGui::CalcTextSize(obj_header);
        draw_list->AddText(ImVec2(curr_pos.x + window_width/2 - oh_size.x/2, score_y),
            IM_COL32(220, 220, 220, 255), obj_header);
        score_y += 25;

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
                    obj.completed ? IM_COL32(100, 255, 100, 255) : IM_COL32(255, 100, 100, 255),
                    ss.str().c_str());
                score_y += 22;
            }
        }

        // Restart button
        ImGui::SetCursorScreenPos(ImVec2(curr_pos.x + window_width/2 - 60, score_y + 30));
        if(ImGui::Button("Play Again", ImVec2(120, 40))){
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
        draw_list->AddRect(card_tl, card_br, IM_COL32(0, 0, 0, 200), 0.0f, 0, 2.0f);
    }

    ImGui::Dummy(ImVec2(window_width, window_height));
    ImGui::End();
    return true;
}

