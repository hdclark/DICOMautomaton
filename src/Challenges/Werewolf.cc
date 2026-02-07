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
    
    // Player positioning constants for circle arrangement
    constexpr float human_player_angle = static_cast<float>(pi / 2.0);  // Bottom center of circle
    constexpr float ai_arc_start = static_cast<float>(-5.0 * pi / 6.0); // -150 degrees
    constexpr float ai_arc_end = static_cast<float>(5.0 * pi / 6.0);    // +150 degrees
}

WerewolfGame::WerewolfGame(){
    rng.seed(std::random_device()());
    InitializePersonas();
    InitializeQuestions();
    InitializeResponses();
    Reset();
}

void WerewolfGame::Reset(){
    players.clear();
    round_exchanges.clear();
    votes.clear();
    available_question_indices.clear();
    
    phase = game_phase_t::Intro;
    round_number = 0;
    current_player_turn = 0;
    human_player_idx = 0;
    werewolf_idx = -1;
    game_over = false;
    townspeople_won = false;
    
    selected_target = -1;
    selected_question = -1;
    hovered_player = -1;
    
    anim_timer = 0.0;
    phase_timer = 0.0;
    pause_duration = 0.0;
    waiting_for_pause = false;
    current_message.clear();
    current_speaker.clear();
    
    last_eliminated = -1;
    last_was_werewolf = false;
    
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
    
    // Iterate through combinations of first and last names
    for(size_t fi = 0; fi < first_names.size() && persona_count < target_personas; ++fi){
        for(size_t li = 0; li < last_names.size() && persona_count < target_personas; ++li){
            // Only use some combinations to reach ~200 (not all 112*48)
            // Use every 3rd last name to spread combinations
            if((fi + li) % 3 != 0) continue;
            
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

void WerewolfGame::AssignRoles(){
    players.clear();
    players.resize(num_players);
    
    // Shuffle persona pool and assign
    std::vector<size_t> persona_indices(persona_pool.size());
    std::iota(persona_indices.begin(), persona_indices.end(), 0);
    std::shuffle(persona_indices.begin(), persona_indices.end(), rng);
    
    // Assign personas to players
    for(int i = 0; i < num_players && i < static_cast<int>(persona_indices.size()); ++i){
        players[i].persona = persona_pool[persona_indices[i]];
        players[i].is_alive = true;
        players[i].is_werewolf = false;
        players[i].is_human = (i == 0);  // First player is human
        
        // Initialize suspicion levels
        for(int j = 0; j < num_players; ++j){
            if(i != j){
                players[i].suspicion_levels[j] = 0.15;  // Start with slight suspicion of everyone
            }
        }
        
        // Random animation phase
        std::uniform_real_distribution<double> phase_dist(0.0, 2.0 * pi);
        players[i].bob_phase = phase_dist(rng);
    }
    
    // Randomly assign werewolf
    std::uniform_int_distribution<int> wolf_dist(0, num_players - 1);
    werewolf_idx = wolf_dist(rng);
    players[werewolf_idx].is_werewolf = true;
    
    human_player_idx = 0;
    
    // Initialize votes
    votes.resize(num_players, -1);
}

void WerewolfGame::StartRound(){
    round_number++;
    round_exchanges.clear();
    current_player_turn = 0;
    
    // Reset questions asked this round
    for(auto& p : players){
        p.questions_asked_this_round = 0;
        p.selected_vote_target = -1;
    }
    
    // Select available questions for this round (random subset)
    available_question_indices.clear();
    std::vector<int> all_indices(all_questions.size());
    std::iota(all_indices.begin(), all_indices.end(), 0);
    std::shuffle(all_indices.begin(), all_indices.end(), rng);
    
    // Take about 60% of questions for this round
    size_t num_available = std::max(size_t(10), all_questions.size() * 6 / 10);
    for(size_t i = 0; i < num_available && i < all_indices.size(); ++i){
        available_question_indices.push_back(all_indices[i]);
    }
    
    // Reset votes
    std::fill(votes.begin(), votes.end(), -1);
    
    phase = game_phase_t::Discussion;
    phase_timer = 0.0;
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
    int target = AISelectQuestionTarget(asker);
    int question = AISelectQuestion(asker, target);
    int response = AISelectResponse(target, question, players[target].is_werewolf);
    
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
            UpdateSuspicions(i, target, question, response);
        }
    }
    
    players[asker].questions_asked_this_round = 1;
    
    // Store pending response to show after question
    pending_target_idx = target;
    pending_response_idx = response;
    
    // Show the question first
    current_speaker = players[asker].persona.name;
    current_message = all_questions[question].text;
    current_message_is_question = true;
    
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
    if(available_question_indices.empty()) return 0;
    
    // Prefer harder questions for more suspicious targets
    double susp = players[asker_idx].suspicion_levels[target_idx];
    
    std::vector<double> weights;
    for(int idx : available_question_indices){
        double w = 1.0;
        // Higher suspicion -> prefer harder questions
        if(susp > 0.5 && all_questions[idx].difficulty >= 2) w *= 2.0;
        if(susp > 0.7 && all_questions[idx].category == "accusation") w *= 2.0;
        weights.push_back(w);
    }
    
    std::discrete_distribution<int> dist(weights.begin(), weights.end());
    return available_question_indices[dist(rng)];
}

int WerewolfGame::AISelectResponse(int responder_idx, int question_idx, bool as_werewolf){
    // Werewolves try to appear innocent but sometimes slip
    // Townspeople respond more honestly
    
    std::vector<double> weights;
    for(size_t i = 0; i < all_responses.size(); ++i){
        double w = 1.0;
        
        if(as_werewolf){
            // Werewolf prefers confident responses but sometimes deflects
            if(!all_responses[i].is_deflection){
                w *= 2.0;  // Prefer non-deflecting
            }else{
                w *= 0.5;  // But sometimes deflect (suspicious behavior)
            }
            // Slight randomness to make werewolf harder to detect
            std::uniform_real_distribution<double> noise(0.8, 1.2);
            w *= noise(rng);
        }else{
            // Townsperson responds naturally
            if(!all_responses[i].is_deflection){
                w *= 3.0;
            }
            // Question difficulty affects nervousness
            if(all_questions[question_idx].difficulty >= 3){
                if(all_responses[i].is_deflection){
                    w *= 1.5;  // Hard questions make even innocents nervous
                }
            }
        }
        
        weights.push_back(w);
    }
    
    std::discrete_distribution<int> dist(weights.begin(), weights.end());
    return dist(rng);
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

void WerewolfGame::UpdateSuspicions(int observer_idx, int responder_idx, int question_idx, int response_idx){
    if(observer_idx == responder_idx) return;
    if(observer_idx < 0 || observer_idx >= num_players) return;
    if(response_idx < 0 || response_idx >= static_cast<int>(all_responses.size())) return;
    
    double delta = all_responses[response_idx].suspicion_delta;
    
    // Question difficulty affects impact
    double diff_mult = 1.0 + (all_questions[question_idx].difficulty - 1) * 0.3;
    delta *= diff_mult;
    
    // Apply change
    players[observer_idx].suspicion_levels[responder_idx] += delta;
    
    // Clamp to valid range
    players[observer_idx].suspicion_levels[responder_idx] = 
        std::clamp(players[observer_idx].suspicion_levels[responder_idx], 0.0, 1.0);
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
        EliminatePlayer(eliminated);
    }
    
    phase = game_phase_t::VoteResults;
    phase_timer = 0.0;
}

void WerewolfGame::EliminatePlayer(int idx){
    if(idx >= 0 && idx < num_players){
        players[idx].is_alive = false;
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
                                 ImU32 color, const std::string& name, bool is_selected, bool is_dead){
    // Draw a monolith (tall rectangle with slight taper)
    float top_width = width * 0.85f;
    
    ImVec2 bl(center.x - width/2, center.y);
    ImVec2 br(center.x + width/2, center.y);
    ImVec2 tr(center.x + top_width/2, center.y - height);
    ImVec2 tl(center.x - top_width/2, center.y - height);
    
    if(is_dead){
        // Draw fallen/crossed out
        color = IM_COL32(80, 80, 80, 200);
    }
    
    // Main body
    draw_list->AddQuadFilled(bl, br, tr, tl, color);
    
    // Outline
    ImU32 outline_color = is_selected ? IM_COL32(255, 255, 0, 255) : IM_COL32(40, 40, 40, 255);
    float outline_thickness = is_selected ? 3.0f : 1.5f;
    draw_list->AddQuad(bl, br, tr, tl, outline_color, outline_thickness);
    
    // Name label below
    ImVec2 text_size = ImGui::CalcTextSize(name.c_str());
    ImVec2 text_pos(center.x - text_size.x/2, center.y + 5);
    draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), name.c_str());
    
    if(is_dead){
        // Draw X over it
        draw_list->AddLine(ImVec2(center.x - width, center.y - height - 10),
                          ImVec2(center.x + width, center.y + 10),
                          IM_COL32(180, 0, 0, 255), 3.0f);
        draw_list->AddLine(ImVec2(center.x + width, center.y - height - 10),
                          ImVec2(center.x - width, center.y + 10),
                          IM_COL32(180, 0, 0, 255), 3.0f);
    }
}

void WerewolfGame::DrawSpeechBubble(ImDrawList* draw_list, ImVec2 anchor, const std::string& text, bool is_question){
    ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
    float padding = 10.0f;
    float bubble_w = text_size.x + padding * 2;
    float bubble_h = text_size.y + padding * 2;
    
    ImVec2 bubble_pos(anchor.x - bubble_w/2, anchor.y - bubble_h - 20);
    
    ImU32 bg_color = is_question ? IM_COL32(60, 60, 120, 230) : IM_COL32(60, 120, 60, 230);
    ImU32 border_color = IM_COL32(200, 200, 200, 255);
    
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
                       IM_COL32(255, 255, 255, 255), text.c_str());
}

bool WerewolfGame::Display(bool &enabled){
    if(!enabled) return true;
    
    auto flags = ImGuiWindowFlags_AlwaysAutoResize
               | ImGuiWindowFlags_NoScrollWithMouse
               | ImGuiWindowFlags_NoNavInputs
               | ImGuiWindowFlags_NoScrollbar;
    
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
                             IM_COL32(20, 25, 35, 255));
    
    // Calculate center for circle arrangement
    ImVec2 center(canvas_pos.x + canvas_size.x * 0.5f, canvas_pos.y + canvas_size.y * 0.5f);
    
    // Draw players in circle, with human player at bottom center
    hovered_player = -1;
    ImVec2 mouse_pos = ImGui::GetMousePos();
    
    for(int i = 0; i < num_players; ++i){
        float angle;
        float radius;
        CalculatePlayerPosition(i, angle, radius);
        
        // Add bobbing animation
        float bob = static_cast<float>(std::sin(players[i].bob_phase) * 3.0);
        
        ImVec2 pos(center.x + radius * std::cos(angle),
                   center.y + radius * std::sin(angle) + bob);
        
        // Check for hover
        float dx = mouse_pos.x - pos.x;
        float dy = mouse_pos.y - (pos.y - monolith_height/2);
        if(std::abs(dx) < monolith_width && std::abs(dy) < monolith_height){
            hovered_player = i;
        }
        
        // Determine color
        ImU32 color;
        if(players[i].is_human){
            color = IM_COL32(100, 120, 140, 255);  // Slightly blue for human
        }else{
            color = IM_COL32(110, 110, 110, 255);  // Gray for AI
        }
        
        // Highlight if selected or hovered
        bool is_selected = (i == selected_target) || (i == hovered_player);
        
        // Get first name only for display
        std::string display_name = players[i].persona.name;
        size_t space_pos = display_name.find(' ');
        if(space_pos != std::string::npos){
            display_name = display_name.substr(0, space_pos);
        }
        
        DrawMonolith(draw_list, pos, monolith_height, monolith_width, color, 
                     display_name, is_selected, !players[i].is_alive);
        
        // If werewolf and game over, show indicator
        if(game_over && players[i].is_werewolf){
            ImVec2 wolf_pos(pos.x, pos.y - monolith_height - 30);
            draw_list->AddText(ImVec2(wolf_pos.x - 25, wolf_pos.y), 
                              IM_COL32(255, 100, 100, 255), "WEREWOLF");
        }
    }
    
    // Draw current message/speech bubble
    if(!current_message.empty() && !current_speaker.empty()){
        // Find speaker position
        for(int i = 0; i < num_players; ++i){
            // Extract first name from persona for matching
            const std::string& full_name = players[i].persona.name;
            size_t space_pos = full_name.find(' ');
            std::string first_name = (space_pos != std::string::npos) ? 
                full_name.substr(0, space_pos) : full_name;
            
            if(full_name.find(current_speaker) != std::string::npos ||
               current_speaker.find(first_name) != std::string::npos){
                float angle;
                float radius;
                CalculatePlayerPosition(i, angle, radius);
                ImVec2 pos(center.x + radius * std::cos(angle),
                           center.y + radius * std::sin(angle) - monolith_height);
                DrawSpeechBubble(draw_list, pos, current_message, current_message_is_question);
                break;
            }
        }
    }
    
    // Dummy for canvas interaction
    ImGui::Dummy(ImVec2(canvas_size.x, canvas_size.y - 150));
    
    // UI Panel at bottom
    ImGui::Separator();
    
    // Handle click on players
    if(ImGui::IsMouseClicked(0) && hovered_player >= 0){
        if(phase == game_phase_t::Discussion && 
           players[human_player_idx].questions_asked_this_round == 0 &&
           hovered_player != human_player_idx &&
           players[hovered_player].is_alive){
            selected_target = hovered_player;
            phase = game_phase_t::SelectQuestion;
        }else if(phase == game_phase_t::Voting && 
                 hovered_player != human_player_idx &&
                 players[hovered_player].is_alive){
            votes[human_player_idx] = hovered_player;
        }
    }
    
    // Phase-specific UI
    switch(phase){
        case game_phase_t::Intro:{
            ImGui::TextWrapped("Welcome to Werewolf!");
            ImGui::TextWrapped("One among us is a werewolf in disguise. Find and eliminate them before it's too late!");
            ImGui::TextWrapped("Press Space to continue, or wait a moment...");
            
            if(phase_timer > intro_time || ImGui::IsKeyPressed(SDL_SCANCODE_SPACE)){
                phase = game_phase_t::AssignRoles;
                phase_timer = 0.0;
            }
            break;
        }
        
        case game_phase_t::AssignRoles:{
            if(players[human_player_idx].is_werewolf){
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "You are the WEREWOLF!");
                ImGui::TextWrapped("Survive until only one townsperson remains. Vote strategically to avoid detection!");
            }else{
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "You are a TOWNSPERSON.");
                ImGui::TextWrapped("Find the werewolf among us! Ask questions and vote wisely.");
            }
            
            ImGui::Text("Your persona: %s, the %s", 
                       players[human_player_idx].persona.name.c_str(),
                       players[human_player_idx].persona.profession.c_str());
            ImGui::TextWrapped("Backstory: %s", players[human_player_idx].persona.backstory.c_str());
            
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
            
            if(players[human_player_idx].questions_asked_this_round == 0){
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), 
                                  "Click on a player to ask them a question");
            }else{
                ImGui::Text("Waiting for other players...");
                
                // Process AI turns (will transition to AIQuestion/AIResponse phases)
                ProcessAITurn();
            }
            
            // Show hovered player info
            if(hovered_player >= 0 && hovered_player < num_players){
                ImGui::Separator();
                ImGui::Text("%s", players[hovered_player].persona.name.c_str());
                ImGui::Text("Profession: %s", players[hovered_player].persona.profession.c_str());
                ImGui::Text("In town: %s", players[hovered_player].persona.years_in_town.c_str());
                ImGui::TextWrapped("Known for: %s", players[hovered_player].persona.quirk.c_str());
            }
            break;
        }
        
        case game_phase_t::SelectQuestion:{
            ImGui::Text("Select a question to ask %s:", players[selected_target].persona.name.c_str());
            
            ImGui::BeginChild("QuestionList", ImVec2(0, 120), true);
            for(size_t i = 0; i < available_question_indices.size(); ++i){
                int q_idx = available_question_indices[i];
                if(ImGui::Selectable(all_questions[q_idx].text.c_str(), selected_question == q_idx)){
                    selected_question = q_idx;
                }
            }
            ImGui::EndChild();
            
            if(selected_question >= 0){
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
                            UpdateSuspicions(i, selected_target, selected_question, response);
                        }
                    }
                    
                    players[human_player_idx].questions_asked_this_round = 1;
                    
                    // Show response
                    current_speaker = players[selected_target].persona.name;
                    current_message = all_responses[response].text;
                    current_message_is_question = false;  // This is a response
                    
                    phase = game_phase_t::WaitingResponse;
                    phase_timer = 0.0;
                    selected_target = -1;
                    selected_question = -1;
                }
            }
            
            if(ImGui::Button("Cancel")){
                selected_target = -1;
                selected_question = -1;
                phase = game_phase_t::Discussion;
            }
            break;
        }
        
        case game_phase_t::WaitingResponse:{
            ImGui::Text("Response: %s", current_message.c_str());
            
            if(phase_timer > 2.0){
                phase = game_phase_t::Discussion;
                current_player_turn = 0;
                current_message.clear();
                current_speaker.clear();
                phase_timer = 0.0;
            }
            break;
        }
        
        case game_phase_t::AIQuestion:{
            // Showing AI's question - display for 1.5 seconds then show response
            ImGui::Text("Round %d - Discussion Phase", round_number);
            ImGui::Text("Waiting for other players...");
            
            if(phase_timer > 1.5){
                // Switch to showing the response
                if(pending_target_idx >= 0 && pending_response_idx >= 0){
                    current_speaker = players[pending_target_idx].persona.name;
                    current_message = all_responses[pending_response_idx].text;
                    current_message_is_question = false;  // Now showing response
                }
                phase = game_phase_t::AIResponse;
                phase_timer = 0.0;
            }
            break;
        }
        
        case game_phase_t::AIResponse:{
            // Showing AI's response - display for 1.5 seconds then continue
            ImGui::Text("Round %d - Discussion Phase", round_number);
            ImGui::Text("Waiting for other players...");
            
            if(phase_timer > 1.5){
                // Move to next AI player
                current_player_turn++;
                current_message.clear();
                current_speaker.clear();
                pending_target_idx = -1;
                pending_response_idx = -1;
                phase = game_phase_t::Discussion;
                phase_timer = 0.0;
            }
            break;
        }
        
        case game_phase_t::Voting:{
            ImGui::Text("Round %d - Voting Phase", round_number);
            ImGui::TextWrapped("Click on a player to vote for their elimination");
            
            if(votes[human_player_idx] >= 0){
                ImGui::Text("You voted for: %s", players[votes[human_player_idx]].persona.name.c_str());
                
                if(ImGui::Button("Confirm Vote")){
                    ProcessVoting();
                }
                if(ImGui::Button("Change Vote")){
                    votes[human_player_idx] = -1;
                }
            }else{
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Select someone to vote for");
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
            
            for(int i = 0; i < num_players; ++i){
                if(players[i].is_alive && vote_counts[i] > 0){
                    ImGui::Text("%s: %d votes", players[i].persona.name.c_str(), vote_counts[i]);
                }
            }
            
            if(last_eliminated >= 0){
                ImGui::Separator();
                if(last_was_werewolf){
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 
                                      "%s was the WEREWOLF!", players[last_eliminated].persona.name.c_str());
                }else{
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
                                      "%s was innocent...", players[last_eliminated].persona.name.c_str());
                }
            }else{
                ImGui::Text("Vote was tied - no one eliminated.");
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
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "TOWNSPEOPLE WIN!");
                ImGui::TextWrapped("The werewolf has been eliminated. The town is safe once more.");
            }else{
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "WEREWOLF WINS!");
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
