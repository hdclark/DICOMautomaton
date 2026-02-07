// Werewolf.cc - A part of DICOMautomaton 2026. Written by hal clark.
// A turn-based social deduction game emulating the Werewolf board game.

#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <numeric>

#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"

#include <SDL.h>
#include "../imgui20210904/imgui.h"

#include "Werewolf.h"

namespace {
    constexpr double pi = 3.14159265358979323846;
    constexpr double firm_suspicion_threshold = 0.75;
    
    // Player positioning constants for circle arrangement
    constexpr float human_player_angle = static_cast<float>(pi / 2.0);  // Bottom center of circle
    constexpr float ai_arc_start = static_cast<float>(-5.0 * pi / 6.0); // -150 degrees
    constexpr float ai_arc_end = static_cast<float>(5.0 * pi / 6.0);    // +150 degrees

    std::string ExtractFirstName(const std::string& full_name){
        size_t space_pos = full_name.find(' ');
        if(space_pos != std::string::npos){
            return full_name.substr(0, space_pos);
        }
        return full_name;
    }

    ImU32 ApplyAlpha(ImU32 base_color, float alpha){
        ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(base_color);
        rgba.w *= std::clamp(alpha, 0.0f, 1.0f);
        return ImGui::ColorConvertFloat4ToU32(rgba);
    }
}

WerewolfGame::WerewolfGame(){
    rng.seed(std::random_device()());
    InitializePersonas();
    InitializeQuestions();
    InitializeResponses();
    BuildResponseCompatibility();
    Reset();
}

void WerewolfGame::Reset(){
    players.clear();
    round_exchanges.clear();
    votes.clear();
    per_player_question_options.clear();
    event_log.clear();
    suspicion_changes.clear();
    
    phase = game_phase_t::Intro;
    round_number = 0;
    current_player_turn = 0;
    human_player_idx = 0;
    werewolf_idx = -1;
    seer_idx = -1;
    game_over = false;
    townspeople_won = false;
    
    selected_target = -1;
    selected_question = -1;
    selected_response = -1;
    selected_announcement_target = -1;
    selected_attack_target = -1;
    selected_seer_target = -1;
    hovered_player = -1;
    active_asker_idx = -1;
    active_target_idx = -1;
    
    anim_timer = 0.0;
    phase_timer = 0.0;
    pause_duration = 0.0;
    waiting_for_pause = false;
    log_scroll_to_bottom = false;
    current_message.clear();
    current_speaker.clear();
    current_message_kind = speech_kind_t::Question;
    waiting_response_from_human_question = false;
    pending_asker_idx = -1;
    pending_question_idx = -1;
    pending_target_idx = -1;
    pending_response_idx = -1;
    
    last_eliminated = -1;
    last_was_werewolf = false;
    last_attacked = -1;
    last_attack_round = -1;
    
    // Assign personas to players
    AssignRoles();
    
    t_updated = std::chrono::steady_clock::now();
}

void WerewolfGame::InitializePersonas(){
    persona_pool.clear();
    
    // Generate ~200 unique personas with varied backgrounds
    // First names pool
    std::vector<std::string> first_names = {
        "Aldric", "Beatrice", "Cedric", "Dorothea", "Edmund", "Fiona", "Gareth", "Helena",
        "Ignatius", "Josephine", "Kenneth", "Lavinia", "Magnus", "Nadia", "Oswald", "Penelope",
        "Quentin", "Rosalind", "Sebastian", "Theodora", "Ulric", "Vivienne", "Wolfgang", "Xenia",
        "Yorick", "Zelda", "Alastair", "Brigitte", "Cornelius", "Dahlia", "Eustace", "Felicity",
        "Gideon", "Harriet", "Ichabod", "Jemima", "Kendrick", "Lucinda", "Mortimer", "Nerissa",
        "Octavius", "Prudence", "Randolph", "Sybil", "Thaddeus", "Ursula", "Valentine", "Winifred",
        "Xavier", "Yvonne", "Zachariah", "Agatha", "Barnaby", "Clarissa", "Desmond", "Eugenia",
        "Ferdinand", "Gertrude", "Horace", "Imogen", "Jasper", "Katharina", "Leopold", "Millicent",
        "Nathaniel", "Ophelia", "Percival", "Quintessa", "Reginald", "Seraphina", "Tobias", "Urania",
        "Victor", "Wilhemina", "Xerxes", "Yolanda", "Zephyr", "Ambrose", "Belinda", "Crispin",
        "Drusilla", "Erasmus", "Florentina", "Gregory", "Hortense", "Isidore", "Jacinta", "Killian",
        "Loretta", "Matthias", "Nicolette", "Orlando", "Philippa", "Radcliffe", "Sophronia", "Tristram",
        "Ulysses", "Venetia", "Wallace", "Xiomara", "Yuri", "Zenobia", "Augustus", "Camilla",
        "Dominic", "Elspeth", "Franklin", "Gwendolyn", "Hamilton", "Isadora", "Jerome", "Katrina"
    };
    
    // Last names pool
    std::vector<std::string> last_names = {
        "Ashworth", "Blackwood", "Cromwell", "Dunmore", "Everhart", "Fairfax", "Grantham", "Holloway",
        "Irvington", "Jameson", "Kingsley", "Lancaster", "Montague", "Northwood", "Pemberton", "Quincy",
        "Ravenswood", "Stirling", "Thornton", "Underwood", "Vandermeer", "Whitmore", "Yardley", "Zimmerman",
        "Aldridge", "Beaumont", "Carrington", "Devereaux", "Ellsworth", "Fitzgerald", "Grimshaw", "Hartwell",
        "Islington", "Jennings", "Kensington", "Lockwood", "Merriweather", "Nightingale", "Osgood", "Prescott",
        "Queensbury", "Rothschild", "Sinclair", "Thistlewood", "Uppington", "Vance", "Wentworth", "Yates"
    };
    
    // Professions pool
    std::vector<std::string> professions = {
        "Blacksmith", "Baker", "Herbalist", "Innkeeper", "Carpenter", "Weaver", "Miller", "Chandler",
        "Cobbler", "Tanner", "Potter", "Brewer", "Butcher", "Tailor", "Mason", "Farmer",
        "Shepherd", "Fisherman", "Woodcutter", "Hunter", "Beekeeper", "Dyer", "Glassblower", "Clockmaker",
        "Apothecary", "Scribe", "Librarian", "Teacher", "Physician", "Midwife", "Gravedigger", "Bellringer",
        "Lamplighter", "Stablehand", "Ferryman", "Cooper", "Wheelwright", "Farrier", "Saddler", "Hatmaker",
        "Jeweler", "Goldsmith", "Silversmith", "Locksmith", "Armorer", "Fletcher", "Bowyer", "Barber",
        "Merchant", "Trader"
    };
    
    // Years in town options
    std::vector<std::string> years_options = {
        "Born here", "1 year", "2 years", "3 years", "5 years", "7 years", "10 years", 
        "15 years", "20 years", "25 years", "30 years", "Since childhood"
    };
    
    // Backstory templates
    std::vector<std::string> backstory_templates = {
        "Moved here after %s in the old country.",
        "Came to town following %s.",
        "Settled here to escape %s.",
        "Arrived seeking %s.",
        "Was brought here by %s.",
        "Returned after years of %s.",
        "Wandered in after %s.",
        "Followed family tradition of %s.",
        "Started fresh after %s.",
        "Found refuge here from %s."
    };
    
    std::vector<std::string> backstory_events = {
        "a great fire", "a family tragedy", "the plague", "a failed harvest", "a broken engagement",
        "military service", "a merchant voyage", "apprenticeship", "university studies", "a pilgrimage",
        "working the mines", "sailing the seas", "tending orchards", "building roads", "serving nobility",
        "a wandering life", "the death of a spouse", "seeking fortune", "political troubles", "religious persecution"
    };
    
    // Quirks
    std::vector<std::string> quirks = {
        "Always hums while working", "Never makes eye contact", "Speaks in rhymes when nervous",
        "Collects unusual stones", "Talks to animals", "Always carries a lucky charm",
        "Never enters buildings first", "Counts everything obsessively", "Whistles at dawn",
        "Never sits with back to door", "Always wears gloves", "Speaks very slowly",
        "Laughs at inappropriate times", "Never uses contractions", "Always looks over shoulder",
        "Refuses to cross running water", "Only eats vegetables", "Never speaks before noon",
        "Always carries dried flowers", "Winks at strangers", "Mutters prayers constantly",
        "Taps fingers rhythmically", "Never finishes sentences", "Speaks in third person",
        "Always agrees with everyone", "Never shares food", "Bows to everyone",
        "Keeps detailed journals", "Never removes hat", "Speaks only in questions"
    };
    
    // Secrets (known only to the player)
    std::vector<std::string> secrets = {
        "Secretly wealthy", "Has a hidden past", "Is in love with another villager",
        "Knows dark magic", "Has committed a crime", "Is actually nobility in disguise",
        "Has a mysterious illness", "Communicates with spirits", "Knows village secrets",
        "Has buried treasure", "Is planning to leave soon", "Has a secret family elsewhere",
        "Witnessed something terrible", "Has a forbidden hobby", "Owes a large debt",
        "Is being blackmailed", "Has a double life", "Knows the truth about someone else",
        "Is protecting someone", "Has prophetic dreams"
    };
    
    // Generate personas
    std::shuffle(first_names.begin(), first_names.end(), rng);
    std::shuffle(last_names.begin(), last_names.end(), rng);
    
    std::uniform_int_distribution<size_t> prof_dist(0, professions.size() - 1);
    std::uniform_int_distribution<size_t> years_dist(0, years_options.size() - 1);
    std::uniform_int_distribution<size_t> backstory_dist(0, backstory_templates.size() - 1);
    std::uniform_int_distribution<size_t> event_dist(0, backstory_events.size() - 1);
    std::uniform_int_distribution<size_t> quirk_dist(0, quirks.size() - 1);
    std::uniform_int_distribution<size_t> secret_dist(0, secrets.size() - 1);
    
    // Create ~200 unique personas by combining first and last names
    std::set<std::string> used_names;
    size_t persona_count = 0;
    const size_t target_personas = 200;
    
    // Sampling interval: With 112 first names and 48 last names (5376 total combinations),
    // taking every 3rd combination gives us approximately 1792 candidates, which is 
    // more than enough to reach our target of 200 diverse personas.
    const int persona_sampling_interval = 3;
    
    // Iterate through combinations of first and last names
    for(size_t fi = 0; fi < first_names.size() && persona_count < target_personas; ++fi){
        for(size_t li = 0; li < last_names.size() && persona_count < target_personas; ++li){
            // Sample combinations to avoid using all 5376 possibilities
            if((fi + li) % persona_sampling_interval != 0) continue;
            
            persona_t p;
            p.name = first_names[fi] + " " + last_names[li];
            
            if(used_names.count(p.name) > 0) continue;
            used_names.insert(p.name);
            
            p.profession = professions[prof_dist(rng)];
            p.years_in_town = years_options[years_dist(rng)];
            
            // Generate backstory
            std::string tmpl = backstory_templates[backstory_dist(rng)];
            std::string event = backstory_events[event_dist(rng)];
            size_t pos = tmpl.find("%s");
            if(pos != std::string::npos){
                p.backstory = tmpl.substr(0, pos) + event + tmpl.substr(pos + 2);
            }else{
                p.backstory = tmpl;
            }
            
            p.quirk = quirks[quirk_dist(rng)];
            p.secret = secrets[secret_dist(rng)];
            
            persona_pool.push_back(p);
            ++persona_count;
        }
    }
}

void WerewolfGame::InitializeQuestions(){
    all_questions.clear();
    
    // Profession-related questions
    all_questions.push_back({"What do you do for a living?", "profession", 1});
    all_questions.push_back({"How long have you been practicing your trade?", "profession", 1});
    all_questions.push_back({"Do you enjoy your work?", "profession", 1});
    all_questions.push_back({"Where did you learn your craft?", "profession", 2});
    all_questions.push_back({"Has business been good lately?", "profession", 1});
    all_questions.push_back({"Do you work alone or with others?", "profession", 2});
    all_questions.push_back({"What's the hardest part of your job?", "profession", 2});
    all_questions.push_back({"Have you trained any apprentices?", "profession", 2});
    
    // History-related questions
    all_questions.push_back({"When did you arrive in our town?", "history", 1});
    all_questions.push_back({"Where did you live before coming here?", "history", 2});
    all_questions.push_back({"Why did you choose this town?", "history", 2});
    all_questions.push_back({"Do you have family here?", "history", 1});
    all_questions.push_back({"What was your life like before?", "history", 3});
    all_questions.push_back({"Have you ever lived elsewhere?", "history", 1});
    all_questions.push_back({"Do you plan to stay here long?", "history", 2});
    all_questions.push_back({"What brought you to settle down?", "history", 2});
    
    // Personal questions
    all_questions.push_back({"How are you feeling today?", "personal", 1});
    all_questions.push_back({"Did you sleep well last night?", "personal", 2});
    all_questions.push_back({"Where were you last evening?", "personal", 3});
    all_questions.push_back({"Do you have any hobbies?", "personal", 1});
    all_questions.push_back({"What's your favorite thing about our town?", "personal", 1});
    all_questions.push_back({"Do you have any enemies?", "personal", 3});
    all_questions.push_back({"What do you think about the recent events?", "personal", 2});
    all_questions.push_back({"Have you noticed anything strange lately?", "personal", 3});
    all_questions.push_back({"Do you trust everyone here?", "personal", 3});
    all_questions.push_back({"What keeps you awake at night?", "personal", 3});
    
    // Accusation-related questions
    all_questions.push_back({"Why are you so nervous?", "accusation", 3});
    all_questions.push_back({"What are you hiding from us?", "accusation", 3});
    all_questions.push_back({"Can you account for your whereabouts?", "accusation", 3});
    all_questions.push_back({"Why should we trust you?", "accusation", 2});
    all_questions.push_back({"Have you seen the werewolf?", "accusation", 3});
    all_questions.push_back({"Who do you think the werewolf is?", "accusation", 2});
    all_questions.push_back({"Are you telling us the truth?", "accusation", 3});
    all_questions.push_back({"Why do you look so pale?", "accusation", 2});
    
    // Behavioral questions
    all_questions.push_back({"Why do you keep looking around?", "accusation", 2});
    all_questions.push_back({"What's that look on your face?", "accusation", 2});
    all_questions.push_back({"You seem distracted. Why?", "accusation", 2});
    all_questions.push_back({"Why won't you look me in the eyes?", "accusation", 3});
}

void WerewolfGame::InitializeResponses(){
    all_responses.clear();
    
    // Honest/straightforward responses
    all_responses.push_back({"I have nothing to hide.", false, -0.1});
    all_responses.push_back({"I'm just a simple townsperson.", false, -0.05});
    all_responses.push_back({"You can trust me completely.", false, -0.05});
    all_responses.push_back({"I've lived here peacefully for years.", false, -0.1});
    all_responses.push_back({"I was at home, as always.", false, -0.05});
    all_responses.push_back({"I'm as worried as everyone else.", false, -0.05});
    all_responses.push_back({"I hope we find the werewolf soon.", false, -0.1});
    all_responses.push_back({"My conscience is clear.", false, -0.1});
    all_responses.push_back({"I've done nothing wrong.", false, -0.05});
    all_responses.push_back({"I'm just tired, that's all.", false, 0.0});
    
    // Deflecting responses
    all_responses.push_back({"Why are you asking me?", true, 0.1});
    all_responses.push_back({"That's none of your business.", true, 0.15});
    all_responses.push_back({"Maybe YOU should answer that.", true, 0.2});
    all_responses.push_back({"I don't have to explain myself.", true, 0.15});
    all_responses.push_back({"What about everyone else?", true, 0.1});
    all_responses.push_back({"Let's focus on someone else.", true, 0.2});
    all_responses.push_back({"I find your questions suspicious.", true, 0.15});
    all_responses.push_back({"Perhaps you're deflecting.", true, 0.2});
    all_responses.push_back({"Look at their behavior instead!", true, 0.15});
    all_responses.push_back({"I don't trust your motives.", true, 0.1});
    
    // Nervous responses
    all_responses.push_back({"I... I'm not sure what you mean.", true, 0.1});
    all_responses.push_back({"W-why would you ask that?", true, 0.15});
    all_responses.push_back({"I don't remember exactly...", true, 0.1});
    all_responses.push_back({"It's complicated to explain.", true, 0.1});
    all_responses.push_back({"Can we talk about this later?", true, 0.2});
    all_responses.push_back({"I'd rather not say.", true, 0.25});
    all_responses.push_back({"That's... a difficult question.", true, 0.1});
    all_responses.push_back({"I need time to think about that.", true, 0.15});
    
    // Confident responses
    all_responses.push_back({"Absolutely! Ask anyone here.", false, -0.15});
    all_responses.push_back({"I swear on my family's honor.", false, -0.1});
    all_responses.push_back({"Look into my eyes - I'm honest.", false, -0.1});
    all_responses.push_back({"I've nothing to fear from truth.", false, -0.15});
    all_responses.push_back({"My record speaks for itself.", false, -0.1});
    all_responses.push_back({"Everyone knows I'm trustworthy.", false, -0.1});
    all_responses.push_back({"I've helped this town for years.", false, -0.1});
    all_responses.push_back({"My hands are clean.", false, -0.1});
    
    // Helpful/cooperative responses
    all_responses.push_back({"I'll help find the real threat.", false, -0.1});
    all_responses.push_back({"Let's work together on this.", false, -0.1});
    all_responses.push_back({"I want the werewolf caught too.", false, -0.1});
    all_responses.push_back({"We must protect each other.", false, -0.05});
    all_responses.push_back({"Ask me anything you need.", false, -0.15});
    all_responses.push_back({"I'll answer honestly.", false, -0.1});
    all_responses.push_back({"For the town's safety, I'll comply.", false, -0.1});
    all_responses.push_back({"United we stand against evil.", false, -0.05});
}

void WerewolfGame::BuildResponseCompatibility(){
    compatible_responses.clear();
    compatible_responses.resize(all_questions.size());

    const int total_responses = static_cast<int>(all_responses.size());
    if(total_responses <= 0) return;

    const int honest_start = 0;
    const int honest_end = std::min(total_responses, 10) - 1;
    const int deflect_start = std::min(total_responses, 10);
    const int deflect_end = std::min(total_responses, 20) - 1;
    const int nervous_start = std::min(total_responses, 20);
    const int nervous_end = std::min(total_responses, 28) - 1;
    const int confident_start = std::min(total_responses, 28);
    const int confident_end = std::min(total_responses, 36) - 1;
    const int helpful_start = std::min(total_responses, 36);
    const int helpful_end = total_responses - 1;

    auto append_range = [&](std::vector<int>& dest, int start, int end){
        if(start > end || start >= total_responses) return;
        for(int i = start; i <= end; ++i){
            dest.push_back(i);
        }
    };

    auto finalize = [&](std::vector<int>& dest){
        std::sort(dest.begin(), dest.end());
        dest.erase(std::unique(dest.begin(), dest.end()), dest.end());
        if(dest.size() < 2){
            dest.clear();
            for(int i = 0; i < total_responses; ++i){
                dest.push_back(i);
            }
        }
    };

    for(size_t q = 0; q < all_questions.size(); ++q){
        auto& dest = compatible_responses[q];
        const std::string& category = all_questions[q].category;

        if(category == "profession" || category == "history"){
            append_range(dest, honest_start, honest_end);
            append_range(dest, confident_start, confident_end);
            append_range(dest, helpful_start, helpful_end);
        }else if(category == "personal"){
            append_range(dest, honest_start, honest_end);
            append_range(dest, nervous_start, nervous_end);
            append_range(dest, helpful_start, helpful_end);
        }else if(category == "accusation"){
            append_range(dest, deflect_start, deflect_end);
            append_range(dest, nervous_start, nervous_end);
            append_range(dest, confident_start, confident_end);
        }else{
            append_range(dest, honest_start, honest_end);
            append_range(dest, deflect_start, deflect_end);
            append_range(dest, nervous_start, nervous_end);
            append_range(dest, confident_start, confident_end);
            append_range(dest, helpful_start, helpful_end);
        }

        finalize(dest);
    }
}

void WerewolfGame::AssignRoles(){
    players.clear();
    players.resize(num_players);
    
    // Shuffle persona pool and assign
    std::vector<size_t> persona_indices(persona_pool.size());
    std::iota(persona_indices.begin(), persona_indices.end(), 0);
    std::shuffle(persona_indices.begin(), persona_indices.end(), rng);
    
    int assigned_count = 0;
    std::set<std::string> used_first_names;

    // Assign personas to players (ensure unique first names)
    for(size_t idx = 0; idx < persona_indices.size() && assigned_count < num_players; ++idx){
        const auto& persona = persona_pool[persona_indices[idx]];
        std::string first_name = ExtractFirstName(persona.name);
        if(used_first_names.count(first_name) > 0){
            continue;
        }

        used_first_names.insert(first_name);

        player_t& player = players[assigned_count];
        player.persona = persona;
        player.is_alive = true;
        player.is_werewolf = false;
        player.is_seer = false;
        player.is_human = (assigned_count == 0);  // First player is human
        player.was_lynched = false;
        player.was_attacked = false;
        player.firm_suspicion_target = -1;
        player.made_announcement_this_round = false;
        player.gossip.clear();
        player.suspicion_levels.clear();

        // Initialize suspicion levels
        for(int j = 0; j < num_players; ++j){
            if(assigned_count != j){
                player.suspicion_levels[j] = 0.15;  // Start with slight suspicion of everyone
            }
        }

        // Random animation phase
        std::uniform_real_distribution<double> phase_dist(0.0, 2.0 * pi);
        player.bob_phase = phase_dist(rng);

        ++assigned_count;
    }

    if(assigned_count < num_players){
        for(size_t idx = 0; idx < persona_indices.size() && assigned_count < num_players; ++idx){
            persona_t persona = persona_pool[persona_indices[idx]];
            std::string first_name = ExtractFirstName(persona.name);
            std::string unique_first = first_name;
            int suffix = 2;
            while(used_first_names.count(unique_first) > 0){
                unique_first = first_name + "-" + std::to_string(suffix++);
            }

            if(unique_first != first_name){
                size_t space_pos = persona.name.find(' ');
                std::string last_name = (space_pos != std::string::npos) ? persona.name.substr(space_pos) : "";
                persona.name = unique_first + last_name;
            }

            used_first_names.insert(unique_first);

            player_t& player = players[assigned_count];
            player.persona = persona;
            player.is_alive = true;
            player.is_werewolf = false;
            player.is_seer = false;
            player.is_human = (assigned_count == 0);
            player.was_lynched = false;
            player.was_attacked = false;
            player.firm_suspicion_target = -1;
            player.made_announcement_this_round = false;
            player.gossip.clear();
            player.suspicion_levels.clear();

            for(int j = 0; j < num_players; ++j){
                if(assigned_count != j){
                    player.suspicion_levels[j] = 0.15;
                }
            }

            std::uniform_real_distribution<double> phase_dist(0.0, 2.0 * pi);
            player.bob_phase = phase_dist(rng);
            ++assigned_count;
        }
    }
    
    // Randomly assign werewolf
    std::uniform_int_distribution<int> wolf_dist(0, num_players - 1);
    werewolf_idx = wolf_dist(rng);
    players[werewolf_idx].is_werewolf = true;

    // Randomly assign seer to a townsperson
    seer_idx = -1;
    std::vector<int> seer_candidates;
    seer_candidates.reserve(num_players);
    for(int i = 0; i < num_players; ++i){
        if(!players[i].is_werewolf){
            seer_candidates.push_back(i);
        }
    }
    if(!seer_candidates.empty()){
        std::uniform_int_distribution<int> seer_dist(0, static_cast<int>(seer_candidates.size()) - 1);
        seer_idx = seer_candidates[seer_dist(rng)];
        players[seer_idx].is_seer = true;
    }
    
    human_player_idx = 0;
    
    // Initialize votes
    votes.resize(num_players, -1);
}

void WerewolfGame::StartRound(){
    round_number++;
    round_exchanges.clear();
    current_player_turn = 0;
    selected_response = -1;
    selected_announcement_target = -1;
    selected_attack_target = -1;
    selected_seer_target = -1;
    active_asker_idx = -1;
    active_target_idx = -1;
    last_attacked = -1;
    last_attack_round = -1;
    
    // Reset questions asked this round
    for(auto& p : players){
        p.questions_asked_this_round = 0;
        p.selected_vote_target = -1;
        p.made_announcement_this_round = false;
    }

    // Select available questions for each player this round (random subset)
    per_player_question_options.clear();
    per_player_question_options.resize(num_players);
    std::vector<int> all_indices(all_questions.size());
    std::iota(all_indices.begin(), all_indices.end(), 0);
    for(int i = 0; i < num_players; ++i){
        per_player_question_options[i] = all_indices;
        std::shuffle(per_player_question_options[i].begin(), per_player_question_options[i].end(), rng);
        if(per_player_question_options[i].size() > max_questions_per_player){
            per_player_question_options[i].resize(max_questions_per_player);
        }
    }

    suspicion_changes.clear();
    
    // Reset votes
    std::fill(votes.begin(), votes.end(), -1);
    
    phase = game_phase_t::Discussion;
    phase_timer = 0.0;
    AssignRoundGossip();
    LogEvent("Round " + std::to_string(round_number) + " begins.");
}

void WerewolfGame::AssignRoundGossip(){
    std::vector<std::string> town_gossip = {
        "The harvest festival should be lively this year.",
        "I saw fresh tracks near the old mill.",
        "The tavern's been unusually quiet lately.",
        "The bellringer has been acting odd.",
        "Someone was prowling near the woods last night.",
        "The baker says supplies are running low.",
        "I've heard strange howls beyond the river.",
        "The mayor seems worried about the livestock."
    };

    std::vector<std::string> ambiguous_gossip = {
        "We should all stay alert tonight.",
        "It's hard to know who to trust.",
        "Everyone seems on edge these days.",
        "Best to keep watch over one another."
    };

    std::vector<std::string> suspicion_gossip = {
        "I'm convinced %s will get my vote tonight.",
        "I can't shake the feeling that %s is hiding something.",
        "If we had to choose now, I'd point at %s.",
        "Something about %s doesn't add up."
    };

    std::uniform_int_distribution<size_t> town_dist(0, town_gossip.size() - 1);
    std::uniform_int_distribution<size_t> ambig_dist(0, ambiguous_gossip.size() - 1);
    std::uniform_int_distribution<size_t> susp_dist(0, suspicion_gossip.size() - 1);

    for(int i = 0; i < num_players; ++i){
        if(!players[i].is_alive){
            players[i].gossip.clear();
            continue;
        }

        UpdateFirmSuspicionTarget(i);

        if(players[i].is_werewolf){
            players[i].gossip = ambiguous_gossip[ambig_dist(rng)];
            continue;
        }

        if(players[i].firm_suspicion_target >= 0){
            const std::string& target_name = players[players[i].firm_suspicion_target].persona.name;
            const std::string& tmpl = suspicion_gossip[susp_dist(rng)];
            size_t pos = tmpl.find("%s");
            if(pos != std::string::npos){
                players[i].gossip = tmpl.substr(0, pos) + target_name + tmpl.substr(pos + 2);
            }else{
                players[i].gossip = tmpl;
            }
        }else{
            players[i].gossip = town_gossip[town_dist(rng)];
        }
    }
}

void WerewolfGame::LogEvent(const std::string& entry){
    event_log.push_back(entry);
    if(event_log.size() > max_log_entries){
        event_log.erase(event_log.begin());
    }
    log_scroll_to_bottom = true;
}

void WerewolfGame::RecordExchange(int asker_idx, int target_idx, int question_idx, int response_idx){
    if(asker_idx < 0 || asker_idx >= num_players) return;
    if(target_idx < 0 || target_idx >= num_players) return;
    if(question_idx < 0 || question_idx >= static_cast<int>(all_questions.size())) return;
    if(response_idx < 0 || response_idx >= static_cast<int>(all_responses.size())) return;

    std::ostringstream oss;
    oss << players[asker_idx].persona.name << " asked " << players[target_idx].persona.name
        << ": \"" << all_questions[question_idx].text << "\" -> \""
        << all_responses[response_idx].text << "\"";
    LogEvent(oss.str());
}

int WerewolfGame::SelectAnnouncementTarget(int announcer_idx){
    if(announcer_idx < 0 || announcer_idx >= num_players) return -1;
    int best_target = -1;
    double best_susp = 0.0;

    for(int j = 0; j < num_players; ++j){
        if(j != announcer_idx && players[j].is_alive){
            auto it = players[announcer_idx].suspicion_levels.find(j);
            if(it == players[announcer_idx].suspicion_levels.end()) continue;
            if(it->second > best_susp){
                best_susp = it->second;
                best_target = j;
            }
        }
    }

    if(best_target >= 0) return best_target;

    std::vector<int> candidates;
    for(int j = 0; j < num_players; ++j){
        if(j != announcer_idx && players[j].is_alive){
            candidates.push_back(j);
        }
    }
    if(candidates.empty()) return -1;
    std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);
    return candidates[dist(rng)];
}

std::string WerewolfGame::BuildAnnouncementText(int announcer_idx, int target_idx){
    if(announcer_idx < 0 || announcer_idx >= num_players) return "";
    if(target_idx < 0 || target_idx >= num_players) return "";

    std::vector<std::string> town_templates = {
        "I'm sure %s is the werewolf.",
        "We should watch %s closely.",
        "I believe %s is our culprit.",
        "My vote is leaning toward %s."
    };

    std::vector<std::string> wolf_templates = {
        "Something about %s feels off.",
        "Keep an eye on %s.",
        "I can't ignore the way %s acts.",
        "Maybe we should question %s next.",
        "I am the seer, and I have seen that %s is the werewolf!"
    };

    if(players[announcer_idx].is_seer){
        town_templates.push_back("I am the seer, and I have seen that %s is the werewolf!");
    }

    const auto& templates = players[announcer_idx].is_werewolf ? wolf_templates : town_templates;
    std::uniform_int_distribution<size_t> dist(0, templates.size() - 1);
    const std::string& tmpl = templates[dist(rng)];
    size_t pos = tmpl.find("%s");
    if(pos != std::string::npos){
        return tmpl.substr(0, pos) + players[target_idx].persona.name + tmpl.substr(pos + 2);
    }
    return tmpl;
}

void WerewolfGame::ApplyAnnouncementEffects(int announcer_idx, int target_idx, bool is_werewolf){
    if(target_idx < 0 || target_idx >= num_players) return;
    std::uniform_real_distribution<double> use_dist(0.0, 1.0);
    double delta = is_werewolf ? 0.12 : 0.15;

    for(int i = 0; i < num_players; ++i){
        if(i == announcer_idx || i == target_idx || !players[i].is_alive) continue;
        if(use_dist(rng) < 0.55){
            double before = players[i].suspicion_levels[target_idx];
            players[i].suspicion_levels[target_idx] += delta;
            players[i].suspicion_levels[target_idx] =
                std::clamp(players[i].suspicion_levels[target_idx], 0.0, 1.0);
            double actual_delta = players[i].suspicion_levels[target_idx] - before;
            QueueSuspicionChange(i, target_idx, actual_delta, ai_message_display_time);
        }
        UpdateFirmSuspicionTarget(i);
    }

    if(target_idx != announcer_idx && players[target_idx].is_alive){
        auto it = players[target_idx].suspicion_levels.find(announcer_idx);
        if(it != players[target_idx].suspicion_levels.end()){
            double before = it->second;
            double accused_delta = players[target_idx].is_werewolf ?
                accused_werewolf_suspicion_delta : accused_innocent_suspicion_delta;
            it->second = std::clamp(it->second + accused_delta, 0.0, 1.0);
            double actual_delta = it->second - before;
            QueueSuspicionChange(target_idx, announcer_idx, actual_delta, ai_message_display_time);

            UpdateFirmSuspicionTarget(target_idx);
        }
    }
}

int WerewolfGame::AISelectSeerTarget(int seer_idx){
    if(seer_idx < 0 || seer_idx >= num_players) return -1;

    std::vector<int> candidates;
    std::vector<double> weights;
    for(int i = 0; i < num_players; ++i){
        if(i == seer_idx || !players[i].is_alive) continue;
        candidates.push_back(i);
        auto it = players[seer_idx].suspicion_levels.find(i);
        double susp = (it != players[seer_idx].suspicion_levels.end()) ? it->second : 0.0;
        weights.push_back(0.1 + susp);
    }

    if(candidates.empty()) return -1;
    std::discrete_distribution<int> dist(weights.begin(), weights.end());
    return candidates[dist(rng)];
}

void WerewolfGame::ResolveSeerVision(int seer_idx, int target_idx, bool reveal_to_player){
    if(seer_idx < 0 || seer_idx >= num_players) return;
    if(target_idx < 0 || target_idx >= num_players) return;
    if(!players[target_idx].is_alive) return;

    bool is_werewolf = players[target_idx].is_werewolf;
    players[seer_idx].suspicion_levels[target_idx] = is_werewolf ? 1.0 : 0.0;
    UpdateFirmSuspicionTarget(seer_idx);

    if(reveal_to_player){
        std::ostringstream oss;
        if(is_werewolf){
            oss << "Your vision reveals that " << players[target_idx].persona.name
                << " IS the werewolf!";
        }else{
            oss << "Your vision reveals that " << players[target_idx].persona.name
                << " is not the werewolf.";
        }
        current_speaker = players[seer_idx].persona.name;
        current_message = oss.str();
        current_message_kind = speech_kind_t::Announcement;
        LogEvent(oss.str());
    }
}

void WerewolfGame::ProceedAfterWerewolfAttack(){
    if(seer_idx < 0 || seer_idx >= num_players || !players[seer_idx].is_alive){
        phase = game_phase_t::VoteResults;
        phase_timer = 0.0;
        return;
    }

    if(players[seer_idx].is_human){
        selected_seer_target = -1;
        phase = game_phase_t::SelectSeerTarget;
        phase_timer = 0.0;
        return;
    }

    int target = AISelectSeerTarget(seer_idx);
    if(target >= 0){
        ResolveSeerVision(seer_idx, target, false);
    }

    phase = game_phase_t::VoteResults;
    phase_timer = 0.0;
}

void WerewolfGame::AdjustSuspicionsAfterAttack(int attacked_idx){
    if(attacked_idx < 0 || attacked_idx >= num_players) return;
    for(int i = 0; i < num_players; ++i){
        if(i == attacked_idx || !players[i].is_alive) continue;
        auto it = players[i].suspicion_levels.find(attacked_idx);
        if(it == players[i].suspicion_levels.end()) continue;
        it->second = 0.0;
        UpdateFirmSuspicionTarget(i);
    }
}

void WerewolfGame::ResolveWerewolfAttack(int forced_target, bool force_skip){
    last_attacked = -1;
    if(werewolf_idx < 0 || werewolf_idx >= num_players) return;
    if(!players[werewolf_idx].is_alive) return;

    if(force_skip){
        LogEvent("The werewolf did not attack this round.");
        return;
    }

    if(forced_target >= 0){
        if(forced_target >= num_players) return;
        if(!players[forced_target].is_alive) return;
        if(players[forced_target].is_werewolf) return;
        EliminatePlayer(forced_target, true);
        last_attacked = forced_target;
        last_attack_round = round_number;
        AdjustSuspicionsAfterAttack(forced_target);
        LogEvent(players[forced_target].persona.name + " was attacked during the night.");
        return;
    }

    std::uniform_real_distribution<double> skip_dist(0.0, 1.0);
    if(skip_dist(rng) < werewolf_skip_attack_probability){
        LogEvent("The werewolf did not attack this round.");
        return;
    }

    std::vector<int> candidates;
    for(int i = 0; i < num_players; ++i){
        if(players[i].is_alive && !players[i].is_werewolf){
            candidates.push_back(i);
        }
    }

    if(candidates.empty()) return;

    std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);
    int target = candidates[dist(rng)];
    EliminatePlayer(target, true);
    last_attacked = target;
    last_attack_round = round_number;
    AdjustSuspicionsAfterAttack(target);
    LogEvent(players[target].persona.name + " was attacked during the night.");
}

void WerewolfGame::ProcessAITurn(){
    // Find next alive AI player
    while(current_player_turn < num_players){
        if(players[current_player_turn].is_alive && 
           !players[current_player_turn].is_human &&
           players[current_player_turn].questions_asked_this_round == 0){
            break;
        }
        current_player_turn++;
    }
    
    if(current_player_turn >= num_players){
        // All players have asked, move to voting
        phase = game_phase_t::Voting;
        phase_timer = 0.0;
        current_message.clear();
        current_speaker.clear();
        return;
    }
    
    int asker = current_player_turn;
    int announcement_target = -1;
    bool wants_announcement = false;
    if(!players[asker].made_announcement_this_round){
        if(players[asker].firm_suspicion_target >= 0){
            announcement_target = players[asker].firm_suspicion_target;
            wants_announcement = true;
        }else if(players[asker].is_werewolf){
            std::uniform_real_distribution<double> announce_dist(0.0, 1.0);
            if(announce_dist(rng) < 0.35){
                announcement_target = SelectAnnouncementTarget(asker);
                wants_announcement = true;
            }
        }
        if(wants_announcement && (announcement_target < 0 || !players[announcement_target].is_alive)){
            announcement_target = SelectAnnouncementTarget(asker);
        }
        if(wants_announcement && announcement_target >= 0 && !players[announcement_target].is_alive){
            announcement_target = -1;
        }
    }

    if(announcement_target >= 0){
        players[asker].made_announcement_this_round = true;

        current_speaker = players[asker].persona.name;
        current_message = BuildAnnouncementText(asker, announcement_target);
        current_message_kind = speech_kind_t::Announcement;
        active_asker_idx = asker;
        active_target_idx = announcement_target;
        pending_asker_idx = asker;
        pending_target_idx = announcement_target;
        pending_question_idx = -1;
        pending_response_idx = -1;

        ApplyAnnouncementEffects(asker, announcement_target, players[asker].is_werewolf);
        LogEvent(current_speaker + " announced: \"" + current_message + "\"");

        phase = game_phase_t::Announcement;
        phase_timer = 0.0;
        return;
    }

    std::uniform_real_distribution<double> skip_dist(0.0, 1.0);
    if(skip_dist(rng) < 0.12){
        players[asker].questions_asked_this_round = 1;
        LogEvent(players[asker].persona.name + " skipped asking a question.");
        current_player_turn++;
        return;
    }

    int target = AISelectQuestionTarget(asker);
    int question = AISelectQuestion(asker, target);
    int response = -1;
    if(target != human_player_idx){
        response = AISelectResponse(target, question, players[target].is_werewolf);
    }

    if(response >= 0){
        // Record exchange
        exchange_t ex;
        ex.asker_idx = asker;
        ex.target_idx = target;
        ex.question_idx = question;
        ex.response_idx = response;
        ex.timestamp = std::chrono::steady_clock::now();
        round_exchanges.push_back(ex);

        // Update suspicions for all observers
        for(int i = 0; i < num_players; ++i){
            if(players[i].is_alive && i != target){
                UpdateSuspicions(i, target, question, response, 2.0 * ai_message_display_time);
            }
        }
        RecordExchange(asker, target, question, response);
    }

    players[asker].questions_asked_this_round = 1;

    // Store pending response to show after question
    pending_asker_idx = asker;
    pending_target_idx = target;
    pending_question_idx = question;
    pending_response_idx = response;

    // Show the question first
    current_speaker = players[asker].persona.name;
    current_message = all_questions[question].text;
    current_message_kind = speech_kind_t::Question;
    active_asker_idx = asker;
    active_target_idx = target;

    // Move to AIQuestion phase to show question, then AIResponse for response
    phase = game_phase_t::AIQuestion;
    phase_timer = 0.0;
}

int WerewolfGame::AISelectQuestionTarget(int asker_idx){
    // Select target based on suspicion levels
    std::vector<int> candidates;
    std::vector<double> weights;
    
    for(int i = 0; i < num_players; ++i){
        if(i != asker_idx && players[i].is_alive){
            candidates.push_back(i);
            // Higher suspicion = more likely to be questioned
            double weight = 0.1 + players[asker_idx].suspicion_levels[i];
            weights.push_back(weight);
        }
    }
    
    if(candidates.empty()) return asker_idx;  // Shouldn't happen
    
    // Weighted random selection
    std::discrete_distribution<int> dist(weights.begin(), weights.end());
    return candidates[dist(rng)];
}

int WerewolfGame::AISelectQuestion(int asker_idx, int target_idx){
    const std::vector<int>* options = nullptr;
    std::vector<int> fallback_questions;
    if(asker_idx >= 0 && asker_idx < num_players &&
       asker_idx < static_cast<int>(per_player_question_options.size())){
        options = &per_player_question_options[asker_idx];
    }
    if(options == nullptr || options->empty()){
        fallback_questions.resize(all_questions.size());
        std::iota(fallback_questions.begin(), fallback_questions.end(), 0);
        options = &fallback_questions;
    }
    
    // Prefer harder questions for more suspicious targets
    double susp = players[asker_idx].suspicion_levels[target_idx];
    
    std::vector<double> weights;
    for(int idx : *options){
        double w = 1.0;
        // Higher suspicion -> prefer harder questions
        if(susp > 0.5 && all_questions[idx].difficulty >= 2) w *= 2.0;
        if(susp > 0.7 && all_questions[idx].category == "accusation") w *= 2.0;
        weights.push_back(w);
    }
    
    std::discrete_distribution<int> dist(weights.begin(), weights.end());
    return (*options)[dist(rng)];
}

int WerewolfGame::AISelectResponse(int responder_idx, int question_idx, bool as_werewolf){
    // Werewolves try to appear innocent but sometimes slip
    // Townspeople respond more honestly
    std::vector<int> candidates;
    if(question_idx >= 0 && question_idx < static_cast<int>(compatible_responses.size())){
        candidates = compatible_responses[question_idx];
    }
    if(candidates.empty()){
        candidates.resize(all_responses.size());
        std::iota(candidates.begin(), candidates.end(), 0);
    }

    int difficulty = 1;
    if(question_idx >= 0 && question_idx < static_cast<int>(all_questions.size())){
        difficulty = all_questions[question_idx].difficulty;
    }

    std::vector<int> filtered_candidates;
    std::vector<double> weights;
    for(int idx : candidates){
        if(idx < 0 || idx >= static_cast<int>(all_responses.size())) continue;
        double w = 1.0;

        if(as_werewolf){
            // Werewolf prefers confident responses but sometimes deflects
            if(!all_responses[idx].is_deflection){
                w *= 2.0;  // Prefer non-deflecting
            }else{
                w *= 0.5;  // But sometimes deflect (suspicious behavior)
            }
            // Slight randomness to make werewolf harder to detect
            std::uniform_real_distribution<double> noise(0.8, 1.2);
            w *= noise(rng);
        }else{
            // Townsperson responds naturally
            if(!all_responses[idx].is_deflection){
                w *= 3.0;
            }
            // Question difficulty affects nervousness
            if(difficulty >= 3){
                if(all_responses[idx].is_deflection){
                    w *= 1.5;  // Hard questions make even innocents nervous
                }
            }
        }

        filtered_candidates.push_back(idx);
        weights.push_back(w);
    }

    if(weights.empty()) return 0;
    std::discrete_distribution<int> dist(weights.begin(), weights.end());
    return filtered_candidates[dist(rng)];
}

int WerewolfGame::AISelectVoteTarget(int voter_idx){
    if(players[voter_idx].is_werewolf){
        // Werewolf votes strategically - vote for whoever has most suspicion
        // to blend in, OR vote for someone else if they're very suspected
        
        // Find who others suspect most (excluding self)
        int most_suspected = -1;
        double max_total_susp = -1.0;
        
        for(int i = 0; i < num_players; ++i){
            if(i != voter_idx && players[i].is_alive && !players[i].is_werewolf){
                double total_susp = 0.0;
                for(int j = 0; j < num_players; ++j){
                    if(j != i && j != voter_idx && players[j].is_alive){
                        total_susp += players[j].suspicion_levels[i];
                    }
                }
                if(total_susp > max_total_susp){
                    max_total_susp = total_susp;
                    most_suspected = i;
                }
            }
        }
        
        return (most_suspected >= 0) ? most_suspected : voter_idx;
    }else{
        // Townsperson votes based on their suspicion levels
        std::vector<int> candidates;
        std::vector<double> weights;
        
        for(int i = 0; i < num_players; ++i){
            if(i != voter_idx && players[i].is_alive){
                candidates.push_back(i);
                weights.push_back(0.1 + players[voter_idx].suspicion_levels[i]);
            }
        }
        
        if(candidates.empty()) return voter_idx;
        
        std::discrete_distribution<int> dist(weights.begin(), weights.end());
        return candidates[dist(rng)];
    }
}

void WerewolfGame::QueueSuspicionChange(int observer_idx, int responder_idx, double delta, double display_delay){
    if(observer_idx < 0 || observer_idx >= num_players) return;
    if(responder_idx < 0 || responder_idx >= num_players) return;
    if(std::abs(delta) <= 0.0001) return;

    suspicion_change_t change;
    change.observer_idx = observer_idx;
    change.responder_idx = responder_idx;
    change.delta = delta;
//    change.timestamp = std::chrono::steady_clock::now()
//        + std::chrono::duration<double>(display_delay);
    std::chrono::milliseconds display_delay_ms( static_cast<long int>(display_delay * 1000.0) );
    change.timestamp = std::chrono::steady_clock::now() + display_delay_ms;
    suspicion_changes.push_back(change);
}

void WerewolfGame::UpdateSuspicions(int observer_idx, int responder_idx, int question_idx, int response_idx,
                                    double display_delay){
    if(observer_idx == responder_idx) return;
    if(observer_idx < 0 || observer_idx >= num_players) return;
    if(response_idx < 0 || response_idx >= static_cast<int>(all_responses.size())) return;
    if(!players[observer_idx].is_alive || !players[responder_idx].is_alive) return;
    
    double delta = all_responses[response_idx].suspicion_delta;
    
    // Question difficulty affects impact
    double diff_mult = 1.0 + (all_questions[question_idx].difficulty - 1) * 0.3;
    delta *= diff_mult;
    
    double before = players[observer_idx].suspicion_levels[responder_idx];
    
    // Apply change
    players[observer_idx].suspicion_levels[responder_idx] += delta;
    
    // Clamp to valid range
    players[observer_idx].suspicion_levels[responder_idx] =
        std::clamp(players[observer_idx].suspicion_levels[responder_idx], 0.0, 1.0);

    double actual_delta = players[observer_idx].suspicion_levels[responder_idx] - before;
    if(players[observer_idx].is_alive && players[responder_idx].is_alive){
        QueueSuspicionChange(observer_idx, responder_idx, actual_delta, display_delay);
    }

    UpdateFirmSuspicionTarget(observer_idx);
}

void WerewolfGame::UpdateFirmSuspicionTarget(int observer_idx){
    if(observer_idx < 0 || observer_idx >= num_players) return;
    if(!players[observer_idx].is_alive) return;

    int best_target = -1;
    double best_susp = 0.0;
    for(int i = 0; i < num_players; ++i){
        if(i != observer_idx && players[i].is_alive){
            double susp = players[observer_idx].suspicion_levels[i];
            if(susp > best_susp){
                best_susp = susp;
                best_target = i;
            }
        }
    }

    players[observer_idx].firm_suspicion_target =
        (best_susp >= firm_suspicion_threshold) ? best_target : -1;
}

void WerewolfGame::ProcessVoting(){
    // AI players vote
    for(int i = 0; i < num_players; ++i){
        if(players[i].is_alive && !players[i].is_human){
            votes[i] = AISelectVoteTarget(i);
        }
    }
    
    // Count votes
    std::vector<int> vote_counts(num_players, 0);
    for(int i = 0; i < num_players; ++i){
        if(players[i].is_alive && votes[i] >= 0 && votes[i] < num_players){
            vote_counts[votes[i]]++;
        }
    }
    
    // Find player with most votes
    int max_votes = 0;
    int eliminated = -1;
    for(int i = 0; i < num_players; ++i){
        if(vote_counts[i] > max_votes){
            max_votes = vote_counts[i];
            eliminated = i;
        }
    }
    
    // Check for tie (no elimination in case of tie)
    int tie_count = 0;
    for(int i = 0; i < num_players; ++i){
        if(vote_counts[i] == max_votes) tie_count++;
    }
    
    if(tie_count > 1){
        eliminated = -1;  // Tie, no elimination
    }
    
    last_eliminated = eliminated;
    if(eliminated >= 0){
        last_was_werewolf = players[eliminated].is_werewolf;
        EliminatePlayer(eliminated, false);
        if(last_was_werewolf){
            LogEvent(players[eliminated].persona.name + " was lynched (werewolf).");
        }else{
            LogEvent(players[eliminated].persona.name + " was lynched (innocent).");

            for(int i = 0; i < num_players; ++i){
                if(!players[i].is_alive || i == eliminated) continue;
                auto it = players[i].suspicion_levels.find(eliminated);
                double redistributed = 0.0;
                if(it != players[i].suspicion_levels.end()){
                    redistributed = it->second;
                    it->second = 0.0;
                }

                int remaining_targets = 0;
                for(int j = 0; j < num_players; ++j){
                    if(j != i && j != eliminated && players[j].is_alive){
                        remaining_targets++;
                    }
                }

                if(remaining_targets > 0 && redistributed > 0.0){
                    double bump = redistributed / static_cast<double>(remaining_targets);
                    for(int j = 0; j < num_players; ++j){
                        if(j != i && j != eliminated && players[j].is_alive){
                            players[i].suspicion_levels[j] =
                                std::clamp(players[i].suspicion_levels[j] + bump, 0.0, 1.0);
                        }
                    }
                }

                UpdateFirmSuspicionTarget(i);
            }
        }
    }else{
        LogEvent("The vote was tied. No one was lynched.");
    }

    if(last_eliminated >= 0 && last_was_werewolf){
        // Clear any prior night attack markers when the werewolf is eliminated.
        last_attacked = -1;
        last_attack_round = -1;
        phase = game_phase_t::VoteResults;
        phase_timer = 0.0;
        return;
    }

    if(werewolf_idx >= 0 && werewolf_idx < num_players &&
       players[werewolf_idx].is_human && players[werewolf_idx].is_alive){
        selected_attack_target = -1;
        phase = game_phase_t::SelectAttack;
        phase_timer = 0.0;
        return;
    }

    ResolveWerewolfAttack();
    ProceedAfterWerewolfAttack();
}

void WerewolfGame::EliminatePlayer(int idx, bool attacked){
    if(idx >= 0 && idx < num_players){
        players[idx].is_alive = false;
        players[idx].was_attacked = attacked;
        players[idx].was_lynched = !attacked;
    }
}

bool WerewolfGame::CheckGameOver(){
    int alive_count = 0;
    int alive_townspeople = 0;
    bool werewolf_alive = false;
    
    for(int i = 0; i < num_players; ++i){
        if(players[i].is_alive){
            alive_count++;
            if(players[i].is_werewolf){
                werewolf_alive = true;
            }else{
                alive_townspeople++;
            }
        }
    }
    
    if(!werewolf_alive){
        game_over = true;
        townspeople_won = true;
        return true;
    }
    
    if(alive_townspeople <= 1){
        game_over = true;
        townspeople_won = false;
        return true;
    }
    
    return false;
}

void WerewolfGame::CalculatePlayerPosition(int player_idx, float& angle, float& radius) const{
    // Calculate the angle and radius for a player's position in the circle
    if(player_idx == human_player_idx){
        // Human player at bottom center
        angle = human_player_angle;
        radius = circle_radius + 50.0f;
    }else{
        // Other players in upper arc
        int other_idx = (player_idx < human_player_idx) ? player_idx : player_idx - 1;
        int num_others = num_players - 1;
        float arc_span = ai_arc_end - ai_arc_start;
        // Guard against division by zero when only 1 AI player
        float position_ratio = (num_others > 1) ? 
            static_cast<float>(other_idx) / static_cast<float>(num_others - 1) : 0.5f;
        // Subtract pi/2 to rotate coordinate system so 0 degrees points upward
        // (ImGui uses +Y downward, so top of screen is negative Y)
        angle = ai_arc_start + position_ratio * arc_span - static_cast<float>(pi/2);
        radius = circle_radius;
    }
}

void WerewolfGame::DrawMonolith(ImDrawList* draw_list, ImVec2 center, float height, float width,
                                 ImU32 color, const std::string& name, bool is_selected,
                                 float lynch_progress, float attack_progress, bool is_dead){
    // Draw a monolith (tall rectangle with slight taper)
    float top_width = width * 0.85f;
    
    ImVec2 bl(center.x - width/2, center.y);
    ImVec2 br(center.x + width/2, center.y);
    ImVec2 tr(center.x + top_width/2, center.y - height);
    ImVec2 tl(center.x - top_width/2, center.y - height);
    
    if(is_dead){
        // Draw fallen/crossed out
        color = monolith_dead_color;
    }
    
    // Main body
    draw_list->AddQuadFilled(bl, br, tr, tl, color);
    
    // Outline
    ImU32 outline_color = is_selected ? monolith_outline_selected_color : monolith_outline_color;
    float outline_thickness = is_selected ? 3.0f : 1.5f;
    draw_list->AddQuad(bl, br, tr, tl, outline_color, outline_thickness);
    
    // Name label below
    ImVec2 text_size = ImGui::CalcTextSize(name.c_str());
    ImVec2 text_pos(center.x - text_size.x/2, center.y + 5);
    draw_list->AddText(text_pos, monolith_name_color, name.c_str());

    if(lynch_progress > 0.0f){
        // Draw X over it for lynching
        ImU32 line_color = ApplyAlpha(lynch_line_color, lynch_progress);
        draw_list->AddLine(ImVec2(center.x - width, center.y - height - 10),
                          ImVec2(center.x + width, center.y + 10),
                          line_color, 3.0f);
        draw_list->AddLine(ImVec2(center.x + width, center.y - height - 10),
                          ImVec2(center.x - width, center.y + 10),
                          line_color, 3.0f);
    }
    if(attack_progress > 0.0f){
        // Draw slashes for werewolf attack
        ImU32 line_color = ApplyAlpha(attack_line_color, attack_progress);
        draw_list->AddLine(ImVec2(center.x - width, center.y - height + 10),
                          ImVec2(center.x + width, center.y - 10),
                          line_color, 3.0f);
        draw_list->AddLine(ImVec2(center.x - width, center.y - height - 10),
                          ImVec2(center.x + width, center.y - height + 30),
                          line_color, 3.0f);
    }
}

void WerewolfGame::DrawSpeechBubble(ImDrawList* draw_list, ImVec2 anchor, const std::string& text, speech_kind_t kind){
    ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
    float padding = 10.0f;
    float bubble_w = text_size.x + padding * 2;
    float bubble_h = text_size.y + padding * 2;
    
    ImVec2 bubble_pos(anchor.x - bubble_w/2, anchor.y - bubble_h - 20);
    
    ImU32 bg_color = speech_response_color;
    if(kind == speech_kind_t::Question){
        bg_color = speech_question_color;
    }else if(kind == speech_kind_t::Announcement){
        bg_color = speech_announcement_color;
    }
    ImU32 border_color = speech_border_color;
    
    draw_list->AddRectFilled(bubble_pos, ImVec2(bubble_pos.x + bubble_w, bubble_pos.y + bubble_h), 
                             bg_color, 8.0f);
    draw_list->AddRect(bubble_pos, ImVec2(bubble_pos.x + bubble_w, bubble_pos.y + bubble_h),
                       border_color, 8.0f, 0, 2.0f);
    
    // Triangle pointer
    ImVec2 tri[3] = {
        ImVec2(anchor.x - 8, bubble_pos.y + bubble_h),
        ImVec2(anchor.x + 8, bubble_pos.y + bubble_h),
        ImVec2(anchor.x, anchor.y - 5)
    };
    draw_list->AddTriangleFilled(tri[0], tri[1], tri[2], bg_color);
    
    // Text
    draw_list->AddText(ImVec2(bubble_pos.x + padding, bubble_pos.y + padding),
                       monolith_name_color, text.c_str());
}

std::string WerewolfGame::GetPhaseName(game_phase_t phase) const {
    std::string phase_str;
    switch(phase){
        case game_phase_t::Intro:{
            phase_str = "Intro";
            break;
        }
        
        case game_phase_t::AssignRoles:{
            phase_str = "AssignRoles";
            break;
        }
        
        case game_phase_t::Discussion:{
            phase_str = "Discussion";
            break;
        }
        
        case game_phase_t::SelectQuestion:{
            phase_str = "SelectQuestion";
            break;
        }
        
        case game_phase_t::WaitingResponse:{
            phase_str = "WaitingResponse";
            break;
        }

        case game_phase_t::SelectResponse:{
            phase_str = "SelectResponse";
            break;
        }

        case game_phase_t::SelectAnnouncement:{
            phase_str = "SelectAnnouncement";
            break;
        }

        case game_phase_t::Announcement:{
            phase_str = "Announcement";
            break;
        }
        
        case game_phase_t::AIQuestion:{
            phase_str = "AIQuestion";
            break;
        }
        
        case game_phase_t::AIResponse:{
            phase_str = "AIResponse";
            break;
        }
        
        case game_phase_t::Voting:{
            phase_str = "Voting";
            break;
        }

        case game_phase_t::SelectAttack:{
            phase_str = "SelectAttack";
            break;
        }

        case game_phase_t::SelectSeerTarget:{
            phase_str = "SelectSeerTarget";
            break;
        }

        case game_phase_t::SeerVision:{
            phase_str = "SeerVision";
            break;
        }
        
        case game_phase_t::VoteResults:{
            phase_str = "VoteResults";
            break;
        }
        
        case game_phase_t::GameOver:{
            phase_str = "GameOver";
            break;
        }
        
        default:
            break;
    }
    return phase_str;
}

bool WerewolfGame::Display(bool &enabled){
    if(!enabled) return true;
    
    auto flags = ImGuiWindowFlags_NoNavInputs;
    ImGui::SetNextWindowSize(ImVec2(window_width, window_height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    ImGui::Begin("Werewolf", &enabled, flags);
    
    const auto f = ImGui::IsWindowFocused();
    if(f && ImGui::IsKeyPressed(SDL_SCANCODE_R)){
        Reset();
    }
    
    // Time update
    const auto t_now = std::chrono::steady_clock::now();
    auto t_diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_updated).count();
    if(t_diff_ms > 50) t_diff_ms = 50;
    const double dt = static_cast<double>(t_diff_ms) / 1000.0;
    t_updated = t_now;
    
    anim_timer += dt;
    phase_timer += dt;

    if(!suspicion_changes.empty()){
        suspicion_changes.erase(
            std::remove_if(suspicion_changes.begin(), suspicion_changes.end(),
                           [&](const suspicion_change_t& change){
                               double age = std::chrono::duration<double>(t_now - change.timestamp).count();
                               return age > suspicion_change_duration;
                           }),
            suspicion_changes.end());
    }
    
    // Update animation for all players
    for(auto& p : players){
        p.bob_phase += dt * 1.5;
        if(p.bob_phase > 2.0 * pi) p.bob_phase -= 2.0 * pi;
    }
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    
    // Background
    draw_list->AddRectFilled(canvas_pos, 
                             ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
                             background_color);
    
    // Calculate center for circle arrangement
    ImVec2 center(canvas_pos.x + canvas_size.x * 0.5f,
                  canvas_pos.y + canvas_size.y * 0.5f - (0.5f * interaction_panel_height) + 50);
    
    // Draw players in circle, with human player at bottom center
    hovered_player = -1;
    ImVec2 mouse_pos = ImGui::GetMousePos();
    std::vector<ImVec2> player_positions(num_players);
    bool focus_pair_active = (active_asker_idx >= 0 && active_target_idx >= 0) &&
                             (phase == game_phase_t::AIQuestion ||
                              phase == game_phase_t::AIResponse ||
                              phase == game_phase_t::WaitingResponse ||
                              phase == game_phase_t::SelectResponse);
    
    for(int i = 0; i < num_players; ++i){
        float angle;
        float radius;
        CalculatePlayerPosition(i, angle, radius);
        
        // Add bobbing animation
        float bob = static_cast<float>(std::sin(players[i].bob_phase) * 3.0);
        
        ImVec2 pos(center.x + radius * std::cos(angle),
                   center.y + radius * std::sin(angle) + bob);
        player_positions[i] = pos;
        
        // Check for hover
        float dx = mouse_pos.x - pos.x;
        float dy = mouse_pos.y - (pos.y - monolith_height/2);
        if(std::abs(dx) < monolith_width && std::abs(dy) < monolith_height){
            hovered_player = i;
        }
        
        // Determine color
        ImU32 color;
        if(players[i].is_human){
            color = monolith_human_color;  // Slightly blue for human
        }else{
            color = monolith_ai_color;  // Gray for AI
        }
        
        // Highlight if selected or hovered
        bool is_focus = focus_pair_active && (i == active_asker_idx || i == active_target_idx);
        bool is_selected = (i == selected_target) || (i == hovered_player) || is_focus;
        if(focus_pair_active && !is_focus){
            ImVec4 dimmed = ImGui::ColorConvertU32ToFloat4(color);
            dimmed.x *= 0.35f;
            dimmed.y *= 0.35f;
            dimmed.z *= 0.35f;
            dimmed.w *= 0.8f;
            color = ImGui::ColorConvertFloat4ToU32(dimmed);
        }
        
        // Get first name only for display
        std::string display_name = ExtractFirstName(players[i].persona.name);
        
        float lynch_progress = 0.0f;
        float attack_progress = 0.0f;
        if(players[i].was_lynched){
            if(phase == game_phase_t::VoteResults && last_eliminated == i){
                lynch_progress = static_cast<float>(
                    std::clamp(phase_timer / elimination_indicator_fade_time, 0.0, 1.0));
            }else{
                lynch_progress = 1.0f;
            }
        }
        if(players[i].was_attacked){
            if(phase == game_phase_t::VoteResults && last_attacked == i && last_attack_round == round_number){
                double delay = (last_eliminated >= 0) ? attack_indicator_delay : 0.0;
                double attack_raw = (phase_timer - delay) / elimination_indicator_fade_time;
                attack_progress = static_cast<float>(std::clamp(attack_raw, 0.0, 1.0));
            }else{
                attack_progress = 1.0f;
            }
        }

        DrawMonolith(draw_list, pos, monolith_height, monolith_width, color,
                     display_name, is_selected, lynch_progress, attack_progress, !players[i].is_alive);
        
        // If werewolf and game over, show indicator
        if(game_over && players[i].is_werewolf){
            ImVec2 wolf_pos(pos.x, pos.y - monolith_height - 30);
            draw_list->AddText(ImVec2(wolf_pos.x - 25, wolf_pos.y), 
                              werewolf_label_color, "WEREWOLF");
        }
    }

    if(!suspicion_changes.empty()){
        std::vector<int> responder_offsets(num_players, 0);
        for(const auto& change : suspicion_changes){
            if(change.responder_idx < 0 || change.responder_idx >= num_players) continue;
            double age = std::chrono::duration<double>(t_now - change.timestamp).count();
            if(age < 0.0) continue;
            if(age > suspicion_change_duration) continue;
            float progress = static_cast<float>(age / suspicion_change_duration);
            float alpha = 1.0f - progress;
            float rise = progress * 20.0f;
            int offset_slot = responder_offsets[change.responder_idx] % suspicion_change_offset_slots;
            responder_offsets[change.responder_idx] += 1;
            float offset_x = static_cast<float>(offset_slot
                             - (suspicion_change_offset_slots / 2)) * suspicion_change_horizontal_offset
                             + 20.0f;
            float offset_y = 10.0f;

            //ImVec2 base = player_positions[change.responder_idx];
            ImVec2 base = player_positions[change.observer_idx];
            ImVec2 text_pos(base.x + offset_x, base.y - monolith_height - offset_y - rise);

            std::ostringstream oss;
            //oss << "Δ" << std::fixed << std::setprecision(2);
            oss << std::fixed << std::setprecision(2);
            if(change.delta >= 0.0){
                oss << "+";
            }
            oss << change.delta;

            ImVec4 color = (change.delta >= 0.0) ? suspicion_increase_color : suspicion_decrease_color;
            color.w *= alpha;
            draw_list->AddText(text_pos, ImGui::ColorConvertFloat4ToU32(color), oss.str().c_str());
        }
    }

    if(phase == game_phase_t::VoteResults){
        std::string center_text;
        if(phase_timer <= ai_message_display_time){
            if(last_eliminated >= 0){
                center_text = players[last_eliminated].persona.name + " was lynched.";
            }else{
                center_text = "Tie. No one was lynched.";
            }
        }else if(phase_timer <= 2.0 * ai_message_display_time){
            if(last_attacked >= 0){
                center_text = players[last_attacked].persona.name + " was attacked during the night.";
            }else{
                center_text = "No one was attacked during the night.";
            }
        }

        if(!center_text.empty()){
            ImVec2 text_size = ImGui::CalcTextSize(center_text.c_str());
            ImVec2 text_pos(center.x - text_size.x / 2.0f, center.y - text_size.y / 2.0f);
            draw_list->AddText(text_pos, monolith_name_color, center_text.c_str());
        }
    }

    if( (hovered_player >= 0)
    &&  (hovered_player < num_players) ){
    //&&  (hovered_player != human_player_idx) ){
        ImGui::BeginTooltip();
        ImGui::Text("%s", players[hovered_player].persona.name.c_str());
        ImGui::Separator();
        ImGui::Text("Profession: %s", players[hovered_player].persona.profession.c_str());
        ImGui::Text("In town: %s", players[hovered_player].persona.years_in_town.c_str());
        ImGui::Text("Known for: %s", players[hovered_player].persona.quirk.c_str());
        ImGui::Separator();
        if(!players[hovered_player].gossip.empty()){
            ImGui::Text("Gossip: %s", players[hovered_player].gossip.c_str());
        }else{
            ImGui::Text("Gossip: (quiet)");
        }

        // Show internal suspicion numbers for debugging purposes, for now.
        ImGui::Separator();
        ImGui::Text("Suspicions:");
        for(int i = 0; i < num_players; ++i){
            if(i == hovered_player) continue;
            double susp = players[hovered_player].suspicion_levels[i];
            ImGui::Text("  %s: %.2f", players[i].persona.name.c_str(), susp);
        }
        ImGui::EndTooltip();
    }
    
    // Draw current message/speech bubble
    if(!current_message.empty() && !current_speaker.empty()){
        // Find speaker position
        for(int i = 0; i < num_players; ++i){
            // Extract first name from persona for matching
            const std::string& full_name = players[i].persona.name;
            std::string first_name = ExtractFirstName(full_name);
            
            if(full_name.find(current_speaker) != std::string::npos ||
               current_speaker.find(first_name) != std::string::npos){
                float angle;
                float radius;
                CalculatePlayerPosition(i, angle, radius);
                ImVec2 pos(center.x + radius * std::cos(angle),
                           center.y + radius * std::sin(angle) - monolith_height);
                DrawSpeechBubble(draw_list, pos, current_message, current_message_kind);
                break;
            }
        }
    }
    
    // Dummy for canvas interaction which holds the UI Panel
    ImGui::Dummy(ImVec2(canvas_size.x,
                 canvas_size.y - interaction_panel_height + 80));

    {
        std::string phase_str = GetPhaseName(phase);
        ImGui::Text("Current phase: %s", phase_str.c_str());
    }
    ImGui::Text("Game Log");

    ImGui::BeginChild("GameLog", ImVec2(0, interaction_panel_height/8.0f), true);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.60f, 0.60f, 1.0f));
    for(const auto& entry : event_log){
        ImGui::TextWrapped("%s", entry.c_str());
    }
    ImGui::PopStyleColor();
    if(log_scroll_to_bottom){
        ImGui::SetScrollHereY(1.0f);
        log_scroll_to_bottom = false;
    }
    ImGui::EndChild();
    
    
    // Handle click on players
    if(ImGui::IsMouseClicked(0) && hovered_player >= 0){
        if(phase == game_phase_t::Discussion && 
           players[human_player_idx].is_alive &&
           players[human_player_idx].questions_asked_this_round == 0 &&
           hovered_player != human_player_idx &&
           players[hovered_player].is_alive){
            selected_target = hovered_player;
            phase = game_phase_t::SelectQuestion;
        }else if(phase == game_phase_t::Voting &&
                 players[human_player_idx].is_alive &&
                 hovered_player != human_player_idx &&
                 players[hovered_player].is_alive){
            votes[human_player_idx] = hovered_player;
        }else if(phase == game_phase_t::SelectAttack &&
                 players[human_player_idx].is_alive &&
                 players[human_player_idx].is_werewolf &&
                 hovered_player != human_player_idx &&
                 players[hovered_player].is_alive){
            selected_attack_target = hovered_player;
        }else if(phase == game_phase_t::SelectSeerTarget &&
                 players[human_player_idx].is_alive &&
                 players[human_player_idx].is_seer &&
                 hovered_player != human_player_idx &&
                 players[hovered_player].is_alive){
            selected_seer_target = hovered_player;
        }
    }
    
    // Phase-specific UI
    switch(phase){
        case game_phase_t::Intro:{
            ImGui::TextWrapped("Welcome to Werewolf!");
            ImGui::TextWrapped("One among us is a werewolf in disguise. Find and eliminate them before it's too late!");
            ImGui::TextColored(instruction_text_color, "Press Space to continue, or wait a moment...");
            
            if(phase_timer > intro_time || ImGui::IsKeyPressed(SDL_SCANCODE_SPACE)){
                phase = game_phase_t::AssignRoles;
                phase_timer = 0.0;
            }
            break;
        }
        
        case game_phase_t::AssignRoles:{
            if(players[human_player_idx].is_werewolf){
                ImGui::TextColored(role_werewolf_text_color, "You are the WEREWOLF!");
                ImGui::TextWrapped("Survive until only one townsperson remains. Vote strategically to avoid detection!");
            }else{
                ImGui::TextColored(role_town_text_color, "You are a TOWNSPERSON.");
                ImGui::TextWrapped("Find the werewolf among us! Ask questions and vote wisely.");
                if(players[human_player_idx].is_seer){
                    ImGui::TextColored(role_town_text_color, "You are the SEER.");
                    ImGui::TextWrapped("Each night, you will glimpse the truth about one player.");
                }
            }
            
            ImGui::Text("  Your persona: %s, the %s", 
                       players[human_player_idx].persona.name.c_str(),
                       players[human_player_idx].persona.profession.c_str());
            ImGui::Text("  Backstory: %s", players[human_player_idx].persona.backstory.c_str());
            
            if(ImGui::Button("Start Game") || (phase_timer > 5.0 && ImGui::IsKeyPressed(SDL_SCANCODE_SPACE))){
                StartRound();
            }
            break;
        }
        
        case game_phase_t::Discussion:{
            ImGui::Text("Round %d - Discussion Phase", round_number);
            
            int alive_count = 0;
            for(const auto& p : players) if(p.is_alive) alive_count++;
            ImGui::Text("Players remaining: %d", alive_count);

            if(!players[human_player_idx].is_alive){
                ImGui::Text("You have been eliminated and can only observe.");
                ProcessAITurn();
                break;
            }
            
            if(players[human_player_idx].questions_asked_this_round == 0){
                ImGui::TextColored(instruction_text_color,
                                  "Click on a player to ask them a question");
                if(ImGui::Button("Skip Question")){
                    players[human_player_idx].questions_asked_this_round = 1;
                    LogEvent(players[human_player_idx].persona.name + " skipped asking a question.");
                    current_player_turn = 0;
                }
            }else{
                ImGui::Text("Waiting for other players...");
                
                // Process AI turns (will transition to AIQuestion/AIResponse phases)
                ProcessAITurn();
            }
            
            break;
        }
        
        case game_phase_t::SelectQuestion:{
            if(!players[human_player_idx].is_alive ||
               selected_target < 0 || selected_target >= num_players ||
               !players[selected_target].is_alive){
                selected_target = -1;
                selected_question = -1;
                phase = game_phase_t::Discussion;
                phase_timer = 0.0;
                break;
            }

            ImGui::TextColored(instruction_text_color, "Select a question to ask %s:",
                               players[selected_target].persona.name.c_str());

            const std::vector<int>* options = nullptr;
            std::vector<int> fallback_questions;
            if(human_player_idx >= 0 && human_player_idx < num_players &&
               human_player_idx < static_cast<int>(per_player_question_options.size())){
                options = &per_player_question_options[human_player_idx];
            }
            if(options == nullptr || options->empty()){
                fallback_questions.resize(all_questions.size());
                std::iota(fallback_questions.begin(), fallback_questions.end(), 0);
                options = &fallback_questions;
            }
            
            ImGui::BeginChild("QuestionList", ImVec2(0, interaction_panel_height/6.0f), true);
            for(int q_idx : *options){
                if(ImGui::Selectable(all_questions[q_idx].text.c_str(), selected_question == q_idx)){
                    selected_question = q_idx;
                }
            }
            ImGui::EndChild();
            
            if(ImGui::Button("Cancel")){
                selected_target = -1;
                selected_question = -1;
                phase = game_phase_t::Discussion;
            }
            
            if(selected_question >= 0){
                ImGui::SameLine();
                if(ImGui::Button("Ask Question")){
                    // Get response
                    int response = AISelectResponse(selected_target, selected_question, 
                                                    players[selected_target].is_werewolf);
                    
                    // Record exchange
                    exchange_t ex;
                    ex.asker_idx = human_player_idx;
                    ex.target_idx = selected_target;
                    ex.question_idx = selected_question;
                    ex.response_idx = response;
                    ex.timestamp = std::chrono::steady_clock::now();
                    round_exchanges.push_back(ex);
                    
                    // Update all players' suspicions
                    for(int i = 0; i < num_players; ++i){
                        if(players[i].is_alive && i != selected_target){
                            UpdateSuspicions(i, selected_target, selected_question, response,
//                                             2.0 * ai_message_display_time);
                                             0.5 * ai_message_display_time);
                        }
                    }
                    RecordExchange(human_player_idx, selected_target, selected_question, response);
                    
                    players[human_player_idx].questions_asked_this_round = 1;
                    
                    // Show response
                    current_speaker = players[selected_target].persona.name;
                    current_message = all_responses[response].text;
                    current_message_kind = speech_kind_t::Response;
                    waiting_response_from_human_question = true;
                    active_asker_idx = human_player_idx;
                    active_target_idx = selected_target;
                    
                    phase = game_phase_t::WaitingResponse;
                    phase_timer = 0.0;
                    selected_target = -1;
                    selected_question = -1;
                }
            }
            break;
        }
        
        case game_phase_t::WaitingResponse:{
            ImGui::Text("Response: %s", current_message.c_str());
            
            if(phase_timer > 2.0){
                current_message.clear();
                current_speaker.clear();
                active_asker_idx = -1;
                active_target_idx = -1;
                pending_asker_idx = -1;
                pending_target_idx = -1;
                pending_question_idx = -1;
                pending_response_idx = -1;
                if(waiting_response_from_human_question){
                    phase = game_phase_t::SelectAnnouncement;
                    current_player_turn = 0;
                    selected_announcement_target = -1;
                }else{
                    phase = game_phase_t::Discussion;
                    current_player_turn++;
                }
                waiting_response_from_human_question = false;
                phase_timer = 0.0;
            }
            break;
        }

        case game_phase_t::SelectResponse:{
            if(!players[human_player_idx].is_alive){
                pending_asker_idx = -1;
                pending_target_idx = -1;
                pending_question_idx = -1;
                pending_response_idx = -1;
                phase = game_phase_t::Discussion;
                phase_timer = 0.0;
                break;
            }

            if(pending_question_idx < 0 || pending_question_idx >= static_cast<int>(all_questions.size()) ||
               pending_asker_idx < 0){
                phase = game_phase_t::Discussion;
                phase_timer = 0.0;
                break;
            }

            ImGui::TextColored(instruction_text_color, "Respond to %s:", players[pending_asker_idx].persona.name.c_str());
            ImGui::TextWrapped("Question: %s", all_questions[pending_question_idx].text.c_str());

            const std::vector<int>* responses = nullptr;
            std::vector<int> fallback_responses;
            if(pending_question_idx >= 0 &&
               pending_question_idx < static_cast<int>(compatible_responses.size())){
                responses = &compatible_responses[pending_question_idx];
            }
            if(responses == nullptr || responses->empty()){
                fallback_responses.resize(all_responses.size());
                std::iota(fallback_responses.begin(), fallback_responses.end(), 0);
                responses = &fallback_responses;
            }

            ImGui::BeginChild("ResponseList", ImVec2(0, interaction_panel_height/6.0f), true);
            if(responses != nullptr){
                for(int idx : *responses){
                    if(idx < 0 || idx >= static_cast<int>(all_responses.size())) continue;
                    if(ImGui::Selectable(all_responses[idx].text.c_str(), selected_response == idx)){
                        selected_response = idx;
                    }
                }
            }
            ImGui::EndChild();

            if(selected_response >= 0){
                if(ImGui::Button("Respond")){
                    exchange_t ex;
                    ex.asker_idx = pending_asker_idx;
                    ex.target_idx = human_player_idx;
                    ex.question_idx = pending_question_idx;
                    ex.response_idx = selected_response;
                    ex.timestamp = std::chrono::steady_clock::now();
                    round_exchanges.push_back(ex);

                    for(int i = 0; i < num_players; ++i){
                        if(players[i].is_alive && i != human_player_idx){
                            UpdateSuspicions(i, human_player_idx, pending_question_idx, selected_response,
//                                             2.0 * ai_message_display_time);
                                             0.0);
                        }
                    }
                    RecordExchange(pending_asker_idx, human_player_idx, pending_question_idx, selected_response);

                    pending_response_idx = selected_response;
                    current_speaker = players[human_player_idx].persona.name;
                    current_message = all_responses[selected_response].text;
                    current_message_kind = speech_kind_t::Response;
                    waiting_response_from_human_question = false;
                    phase = game_phase_t::WaitingResponse;
                    phase_timer = 0.0;
                    selected_response = -1;
                }
            }else{
                ImGui::TextColored(instruction_text_color,
                                   "Select a response to continue.");
            }
            break;
        }

        case game_phase_t::SelectAnnouncement:{
            if(!players[human_player_idx].is_alive){
                selected_announcement_target = -1;
                phase = game_phase_t::Discussion;
                current_player_turn = 0;
                phase_timer = 0.0;
                break;
            }

            ImGui::TextColored(instruction_text_color, "Make an announcement about your suspicions?");

            ImGui::BeginChild("AnnouncementTargets", ImVec2(0, interaction_panel_height/6.0f), true);
            for(int i = 0; i < num_players; ++i){
                if(i == human_player_idx || !players[i].is_alive) continue;
                std::string label = players[i].persona.name;
                if(ImGui::Selectable(label.c_str(), selected_announcement_target == i)){
                    selected_announcement_target = i;
                }
            }
            ImGui::EndChild();

            if(ImGui::Button("Skip Announcement")){
                selected_announcement_target = -1;
                phase = game_phase_t::Discussion;
                current_player_turn = 0;
                phase_timer = 0.0;
            }

            if(selected_announcement_target >= 0){
                ImGui::SameLine();
                if(ImGui::Button("Announce")){
                    current_speaker = players[human_player_idx].persona.name;
                    current_message = BuildAnnouncementText(human_player_idx, selected_announcement_target);
                    current_message_kind = speech_kind_t::Announcement;
                    active_asker_idx = human_player_idx;
                    active_target_idx = selected_announcement_target;
                    pending_asker_idx = human_player_idx;
                    pending_target_idx = selected_announcement_target;
                    ApplyAnnouncementEffects(human_player_idx, selected_announcement_target,
                                             players[human_player_idx].is_werewolf);
                    LogEvent(current_speaker + " announced: \"" + current_message + "\"");
                    players[human_player_idx].made_announcement_this_round = true;
                    phase = game_phase_t::Announcement;
                    phase_timer = 0.0;
                    selected_announcement_target = -1;
                }
            }
            break;
        }

        case game_phase_t::Announcement:{
            ImGui::Text("Announcement:");
            ImGui::TextWrapped("%s", current_message.c_str());

            if(phase_timer > ai_message_display_time){
                current_message.clear();
                current_speaker.clear();
                active_asker_idx = -1;
                active_target_idx = -1;
                if(pending_asker_idx == human_player_idx){
                    current_player_turn = 0;
                }else if(pending_asker_idx >= 0){
                    current_player_turn = pending_asker_idx;
                }else{
                    current_player_turn = 0;
                }
                pending_asker_idx = -1;
                pending_target_idx = -1;
                phase = game_phase_t::Discussion;
                phase_timer = 0.0;
            }
            break;
        }
        
        case game_phase_t::AIQuestion:{
            // Showing AI's question - display then show response
            ImGui::Text("Round %d - Discussion Phase", round_number);
            ImGui::Text("Waiting for other players...");
            
            if(phase_timer > ai_message_display_time){
                if(pending_target_idx == human_player_idx){
                    selected_response = -1;
                    phase = game_phase_t::SelectResponse;
                    phase_timer = 0.0;
                }else{
                    // Switch to showing the response
                    if(pending_target_idx >= 0 && pending_response_idx >= 0){
                        current_speaker = players[pending_target_idx].persona.name;
                        current_message = all_responses[pending_response_idx].text;
                        current_message_kind = speech_kind_t::Response;
                    }
                    phase = game_phase_t::AIResponse;
                    phase_timer = 0.0;
                }
            }
            break;
        }
        
        case game_phase_t::AIResponse:{
            // Showing AI's response - display then continue
            ImGui::Text("Round %d - Discussion Phase", round_number);
            ImGui::Text("Waiting for other players...");
            
            if(phase_timer > ai_message_display_time){
                // Move to next AI player
                current_player_turn++;
                current_message.clear();
                current_speaker.clear();
                active_asker_idx = -1;
                active_target_idx = -1;
                pending_asker_idx = -1;
                pending_target_idx = -1;
                pending_question_idx = -1;
                pending_response_idx = -1;
                phase = game_phase_t::Discussion;
                phase_timer = 0.0;
            }
            break;
        }
        
        case game_phase_t::Voting:{
            ImGui::Text("Round %d - Voting Phase", round_number);

            if(!players[human_player_idx].is_alive){
                ImGui::Text("You have been eliminated and can only observe.");
                if(phase_timer > 1.0){
                    ProcessVoting();
                }
                break;
            }

            ImGui::TextColored(instruction_text_color, "Click on a player to vote for their elimination");
            
            if(votes[human_player_idx] >= 0){
                ImGui::Text("You voted for: %s", players[votes[human_player_idx]].persona.name.c_str());
                
                if(ImGui::Button("Confirm Vote")){
                    ProcessVoting();
                }
                ImGui::SameLine();
                if(ImGui::Button("Change Vote")){
                    votes[human_player_idx] = -1;
                }
            }else{
                ImGui::TextColored(instruction_text_color, "Select someone to vote for");
            }
            break;
        }

        case game_phase_t::SelectAttack:{
            ImGui::Text("Night falls...");

            if(!players[human_player_idx].is_alive || !players[human_player_idx].is_werewolf){
                ResolveWerewolfAttack();
                ProceedAfterWerewolfAttack();
                break;
            }

            ImGui::TextColored(instruction_text_color,
                               "Click on a player to attack, or choose to skip.");

            if(selected_attack_target >= 0){
                ImGui::Text("Selected target: %s",
                            players[selected_attack_target].persona.name.c_str());
                if(ImGui::Button("Attack")){
                    ResolveWerewolfAttack(selected_attack_target);
                    selected_attack_target = -1;
                    ProceedAfterWerewolfAttack();
                }
                ImGui::SameLine();
            }
            if(ImGui::Button("Skip Attack")){
                ResolveWerewolfAttack(-1, true);
                selected_attack_target = -1;
                ProceedAfterWerewolfAttack();
            }
            break;
        }

        case game_phase_t::SelectSeerTarget:{
            if(!players[human_player_idx].is_alive || !players[human_player_idx].is_seer){
                phase = game_phase_t::VoteResults;
                phase_timer = 0.0;
                break;
            }

            int available_targets = 0;
            for(int i = 0; i < num_players; ++i){
                if(i != human_player_idx && players[i].is_alive){
                    available_targets++;
                }
            }
            if(available_targets == 0){
                phase = game_phase_t::VoteResults;
                phase_timer = 0.0;
                break;
            }

            ImGui::Text("Seer Vision");
            ImGui::TextColored(instruction_text_color,
                               "Click on a player to reveal their true nature.");

            if(selected_seer_target >= 0){
                ImGui::Text("Vision target: %s",
                            players[selected_seer_target].persona.name.c_str());
                if(ImGui::Button("Use Vision")){
                    ResolveSeerVision(human_player_idx, selected_seer_target, true);
                    selected_seer_target = -1;
                    phase = game_phase_t::SeerVision;
                    phase_timer = 0.0;
                }
            }
            break;
        }

        case game_phase_t::SeerVision:{
            ImGui::Text("Seer Vision:");
            ImGui::TextWrapped("%s", current_message.c_str());

            if(phase_timer > ai_message_display_time){
                current_message.clear();
                current_speaker.clear();
                phase = game_phase_t::VoteResults;
                phase_timer = 0.0;
            }
            break;
        }
        
        case game_phase_t::VoteResults:{
            ImGui::Text("Vote Results:");
            
            // Count and display votes
            std::vector<int> vote_counts(num_players, 0);
            for(int i = 0; i < num_players; ++i){
                if(players[i].is_alive && votes[i] >= 0){
                    vote_counts[votes[i]]++;
                }
            }
            
            ImGui::BeginChild("VoteResults", ImVec2(0, interaction_panel_height/8.0f), true);
            for(int i = 0; i < num_players; ++i){
                if(players[i].is_alive && vote_counts[i] > 0){
                    ImGui::TextWrapped("  %s: %d votes", players[i].persona.name.c_str(), vote_counts[i]);
                }
            }
            ImGui::EndChild();
            
            if(last_eliminated >= 0){
                ImGui::Separator();
                if(last_was_werewolf){
                    ImGui::TextColored(success_text_color,
                                      "%s was the WEREWOLF!", players[last_eliminated].persona.name.c_str());
                }else{
                    ImGui::TextColored(warning_text_color,
                                      "%s was innocent...", players[last_eliminated].persona.name.c_str());
                }
            }else{
                ImGui::Text("Vote was tied - no one eliminated.");
            }

            if(last_attacked >= 0 && (last_eliminated < 0 || phase_timer >= attack_indicator_delay)){
                ImGui::Separator();
                ImGui::TextColored(night_attack_text_color,
                                  "%s was attacked during the night.",
                                  players[last_attacked].persona.name.c_str());
            }
            
            if(phase_timer > vote_reveal_time + elimination_time){
                if(CheckGameOver()){
                    phase = game_phase_t::GameOver;
                }else{
                    StartRound();
                }
                phase_timer = 0.0;
            }
            break;
        }
        
        case game_phase_t::GameOver:{
            if(townspeople_won){
                ImGui::TextColored(success_text_color, "TOWNSPEOPLE WIN!");
                ImGui::TextWrapped("The werewolf has been eliminated. The town is safe once more.");
            }else{
                ImGui::TextColored(role_werewolf_text_color, "WEREWOLF WINS!");
                ImGui::TextWrapped("The werewolf has eliminated enough townspeople. Darkness falls upon the village.");
            }
            
            // Show who the werewolf was
            for(int i = 0; i < num_players; ++i){
                if(players[i].is_werewolf){
                    ImGui::Text("The werewolf was: %s", players[i].persona.name.c_str());
                    break;
                }
            }
            
            if(ImGui::Button("Play Again")){
                Reset();
            }
            break;
        }
        
        default:
            break;
    }

    ImGui::End();
    return true;
}
