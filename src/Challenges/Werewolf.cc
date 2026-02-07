// Werewolf.cc - A turn-based Werewolf board game minigame.

#include <vector>
#include <random>
#include <chrono>
#include <string>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <cmath>
#include <functional>

#include "YgorMath.h"
#include "YgorMisc.h"

#include <SDL.h>
#include "../imgui20210904/imgui.h"

#include "Werewolf.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Persona, question, and response initialization
// ============================================================================

void WerewolfGame::InitPersonas(){
    all_personas.clear();
    // ~15 personas for now; more to be added later.
    all_personas.push_back({"Aldric",    "Blacksmith",   "Born and raised in the village, forged the town bell.",             "Some say Aldric never sleeps—his forge glows at all hours.",         32});
    all_personas.push_back({"Berta",     "Herbalist",    "Moved from the eastern marshes five years ago.",                     "Berta keeps strange dried plants hanging in her window.",            5});
    all_personas.push_back({"Cedric",    "Innkeeper",    "Third-generation innkeeper; knows everyone's drink order.",          "Cedric waters down the ale when he thinks no one notices.",          45});
    all_personas.push_back({"Dalia",     "Weaver",       "Arrived last autumn claiming to flee a flood.",                      "Dalia never talks about her life before the village.",               1});
    all_personas.push_back({"Elmond",    "Farmer",       "Owns the largest wheat field north of the river.",                   "Elmond lost three sheep last month under mysterious circumstances.", 20});
    all_personas.push_back({"Faye",      "Midwife",      "Delivered half the children in town over two decades.",              "Faye hums old lullabies that no one else seems to know.",           22});
    all_personas.push_back({"Gareth",    "Woodcutter",   "Lives alone at the edge of the forest.",                             "Gareth's dog howls every full moon without fail.",                   10});
    all_personas.push_back({"Hilda",     "Baker",        "Famous for her rye bread; opens the shop before dawn.",              "Hilda once found claw marks on her back door but told no one.",      15});
    all_personas.push_back({"Ivan",      "Fisherman",    "Fishes the northern lake and sells catch at the market.",            "Ivan claims he saw red eyes near the lake last winter.",             8});
    all_personas.push_back({"Juna",      "Schoolteacher","Educated in the capital, returned to teach village children.",       "Juna keeps a locked chest she never lets anyone open.",              3});
    all_personas.push_back({"Kael",      "Shepherd",     "Tends goats on the hillside; quiet and observant.",                  "Kael disappears for hours and no one knows where he goes.",         12});
    all_personas.push_back({"Lorna",     "Tanner",       "Works hides near the creek; strong and no-nonsense.",                "Lorna was seen arguing with a stranger on the road last week.",      18});
    all_personas.push_back({"Morten",    "Gravedigger",  "Maintains the old cemetery on the hill.",                             "Morten talks to the headstones when he thinks he is alone.",         25});
    all_personas.push_back({"Nell",      "Seamstress",   "Mends clothes for the whole village from her small cottage.",        "Nell keeps her shutters closed even on warm days.",                  7});
    all_personas.push_back({"Oswin",     "Miller",       "Runs the watermill; flour is his trade.",                             "Oswin's mill wheel sometimes turns at night with no water.",         30});
}

void WerewolfGame::InitQuestions(){
    all_questions.clear();
    all_responses.clear();

    // Q0: background
    all_questions.push_back({"How long have you lived in our village?", 1, 0});
    all_responses.push_back({
        {"I have lived here my whole life.",                         0.1, 0.05},
        {"I moved here a few years ago for a fresh start.",          0.2, 0.1},
        {"I would rather not say.",                                  0.5, 0.3},
        {"Long enough to know every face in town.",                  0.15, 0.05},
    });

    // Q1: alibi
    all_questions.push_back({"Where were you last night after sundown?", 2, 1});
    all_responses.push_back({
        {"I was home, asleep in my bed.",                            0.2, 0.05},
        {"I was at the inn having a drink.",                         0.15, 0.05},
        {"I went for a walk to clear my head.",                      0.4, 0.15},
        {"That is none of your business.",                           0.6, 0.35},
    });

    // Q2: behavior
    all_questions.push_back({"Have you noticed anything strange lately?",1, 2});
    all_responses.push_back({
        {"Nothing out of the ordinary.",                             0.3, 0.1},
        {"I heard howling near the forest edge.",                    0.15, 0.1},
        {"I saw someone lurking near the well at midnight.",         0.2, 0.1},
        {"Why do you ask? Are you trying to deflect suspicion?",    0.35, 0.25},
    });

    // Q3: knowledge
    all_questions.push_back({"What do you know about the recent attacks?",2, 3});
    all_responses.push_back({
        {"Only what everyone else knows—livestock have been killed.",0.1, 0.05},
        {"I think the attacks come from inside the village.",        0.25, 0.15},
        {"I would rather not speculate without evidence.",           0.3, 0.1},
        {"Maybe the person asking knows more than they let on.",     0.4, 0.3},
    });

    // Q4: accusation
    all_questions.push_back({"Do you think someone here is the werewolf?",3, 4});
    all_responses.push_back({
        {"I trust everyone here, but we must be cautious.",          0.15, 0.05},
        {"I have my suspicions, but I will keep them to myself.",    0.35, 0.2},
        {"Yes—I think someone is hiding something.",                 0.25, 0.15},
        {"Accusations without proof are dangerous.",                 0.2, 0.1},
    });

    // Q5: background
    all_questions.push_back({"What brought you to this village originally?",1, 0});
    all_responses.push_back({
        {"I was born here—this is all I have ever known.",           0.1, 0.05},
        {"I came for work and decided to stay.",                     0.15, 0.05},
        {"I came to escape troubles elsewhere.",                     0.3, 0.15},
        {"Does it matter? I am here now.",                           0.45, 0.25},
    });

    // Q6: alibi
    all_questions.push_back({"Can anyone vouch for your whereabouts last night?",2, 1});
    all_responses.push_back({
        {"Yes, I was with my family all evening.",                   0.1, 0.05},
        {"The innkeeper saw me. Ask him.",                           0.15, 0.05},
        {"I live alone. No one can vouch for me.",                   0.35, 0.2},
        {"Why should I need someone to vouch for me?",              0.5, 0.3},
    });

    // Q7: behavior
    all_questions.push_back({"You seem nervous. Is something bothering you?",2, 2});
    all_responses.push_back({
        {"Of course I am nervous—there is a werewolf among us!",    0.15, 0.1},
        {"I am perfectly fine, thank you.",                          0.25, 0.05},
        {"You would be nervous too if people kept staring at you.",  0.3, 0.15},
        {"Mind your own feelings.",                                  0.5, 0.3},
    });

    // Q8: knowledge
    all_questions.push_back({"Did you hear anything unusual during the full moon?",2, 3});
    all_responses.push_back({
        {"Just the wind and the usual night sounds.",                0.2, 0.05},
        {"I thought I heard growling, but it could have been a dog.",0.15, 0.1},
        {"I sleep like a log—I heard nothing.",                      0.3, 0.1},
        {"Full moons make everyone imagine things.",                 0.25, 0.15},
    });

    // Q9: accusation
    all_questions.push_back({"If you had to vote right now, who would you choose?",3, 4});
    all_responses.push_back({
        {"I need more information before I decide.",                 0.15, 0.05},
        {"I would vote for whoever is acting the most defensive.",   0.2, 0.1},
        {"I would rather not say—it could cause panic.",             0.35, 0.2},
        {"Perhaps the one asking the most questions.",               0.4, 0.25},
    });

    // Q10: background
    all_questions.push_back({"Do you have family in the village?",1, 0});
    all_responses.push_back({
        {"Yes, my family has been here for generations.",            0.1, 0.05},
        {"A few relatives, but we are not close.",                   0.15, 0.05},
        {"No, I am on my own.",                                      0.3, 0.15},
        {"Family is a private matter.",                               0.4, 0.25},
    });

    // Q11: behavior
    all_questions.push_back({"Why do you keep looking toward the forest?",2, 2});
    all_responses.push_back({
        {"I was just lost in thought.",                              0.2, 0.1},
        {"The forest unsettles me lately, that is all.",             0.15, 0.05},
        {"I was not looking at the forest.",                         0.35, 0.15},
        {"Maybe I should ask why you are watching me so closely.",   0.4, 0.25},
    });
}

// ============================================================================
// Constructor / Reset
// ============================================================================

WerewolfGame::WerewolfGame(){
    std::random_device rd;
    ww_game.re.seed(rd());
    InitPersonas();
    InitQuestions();
    Reset();
}

void WerewolfGame::Reset(){
    players.clear();
    ww_game.round_number = 0;
    ww_game.phase = ww_phase_t::Setup;
    ww_game.phase_timer = 0.0;
    ww_game.action_delay = 0.0;
    ww_game.ai_action_idx = 0;
    ww_game.vote_counts.clear();
    ww_game.lynched_player = -1;
    ww_game.lynched_was_wolf = false;
    ww_game.selected_target = -1;
    ww_game.selected_question = -1;
    ww_game.selected_response = -1;
    ww_game.human_asked_by = -1;
    ww_game.human_asked_question = -1;
    ww_game.show_role_reveal = true;
    ww_game.role_reveal_timer = 0.0;
    ww_game.game_log.clear();
    ww_game.last_lynch_name.clear();
    ww_game.any_lynch_happened = false;

    AssignRoles();
    t_ww_updated = std::chrono::steady_clock::now();
}

void WerewolfGame::AssignRoles(){
    const auto N = ww_game.num_players;

    // Shuffle personas and pick N unique ones.
    auto personas = all_personas;
    std::shuffle(personas.begin(), personas.end(), ww_game.re);

    players.resize(static_cast<size_t>(N));
    for(int64_t i = 0; i < N; ++i){
        auto &p = players[static_cast<size_t>(i)];
        p.persona = personas[static_cast<size_t>(i % static_cast<int64_t>(personas.size()))];
        p.is_werewolf = false;
        p.is_alive = true;
        p.is_human = (i == 0);
        p.has_asked_question = false;
        p.question_target = -1;
        p.question_idx = -1;
        p.response_idx = -1;
        p.vote_target = -1;
        p.monolith_bob = static_cast<double>(i) * 0.7;
        p.glow_alpha = 0.0f;
        p.suspicion.assign(static_cast<size_t>(N), (N > 1) ? 1.0 / static_cast<double>(N - 1) : 0.0);
    }

    // Pick a random werewolf (could be the human).
    std::uniform_int_distribution<int64_t> dist(0, N - 1);
    const auto wolf_idx = dist(ww_game.re);
    players[static_cast<size_t>(wolf_idx)].is_werewolf = true;

    if(players[0].is_werewolf){
        AddLog("You are the WEREWOLF! Survive until only one townsperson remains.", ImVec4(1.0f,0.3f,0.3f,1.0f));
    } else {
        AddLog("You are a townsperson. Find and lynch the werewolf!", ImVec4(0.3f,1.0f,0.3f,1.0f));
    }

    for(int64_t i = 1; i < N; ++i){
        auto &p = players[static_cast<size_t>(i)];
        AddLog(p.persona.name + " - " + p.persona.profession + ": " + p.persona.backstory, ImVec4(0.8f,0.8f,0.8f,1.0f));
    }
}

// ============================================================================
// Utility helpers
// ============================================================================

int64_t WerewolfGame::CountAlive() const {
    int64_t c = 0;
    for(const auto &p : players) if(p.is_alive) ++c;
    return c;
}
int64_t WerewolfGame::CountAliveWolves() const {
    int64_t c = 0;
    for(const auto &p : players) if(p.is_alive && p.is_werewolf) ++c;
    return c;
}
int64_t WerewolfGame::CountAliveTown() const {
    int64_t c = 0;
    for(const auto &p : players) if(p.is_alive && !p.is_werewolf) ++c;
    return c;
}

void WerewolfGame::AddLog(const std::string &text, const ImVec4 &color){
    ww_game.game_log.push_back({text, color});
    // Keep log manageable.
    while(ww_game.game_log.size() > 80) ww_game.game_log.erase(ww_game.game_log.begin());
}

// ============================================================================
// Round management
// ============================================================================

void WerewolfGame::StartRound(){
    ww_game.round_number++;
    ww_game.phase = ww_phase_t::QuestionSelect;
    ww_game.phase_timer = 0.0;
    ww_game.action_delay = 0.0;
    ww_game.ai_action_idx = 0;
    ww_game.selected_target = -1;
    ww_game.selected_question = -1;
    ww_game.selected_response = -1;
    ww_game.human_asked_by = -1;
    ww_game.human_asked_question = -1;
    ww_game.lynched_player = -1;

    for(auto &p : players){
        p.has_asked_question = false;
        p.question_target = -1;
        p.question_idx = -1;
        p.response_idx = -1;
        p.vote_target = -1;
    }

    SelectRoundQuestions();
    AddLog("--- Round " + std::to_string(ww_game.round_number) + " begins ---", ImVec4(1.0f,1.0f,0.4f,1.0f));
}

void WerewolfGame::SelectRoundQuestions(){
    // Pick a subset of questions for this round (6 out of total).
    std::vector<int64_t> indices(all_questions.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), ww_game.re);
    const size_t count = std::min(indices.size(), static_cast<size_t>(6));
    ww_game.round_question_indices.assign(indices.begin(), indices.begin() + static_cast<std::ptrdiff_t>(count));
}

void WerewolfGame::AIAskQuestions(){
    const auto N = static_cast<int64_t>(players.size());
    for(int64_t i = 1; i < N; ++i){
        auto &p = players[static_cast<size_t>(i)];
        if(!p.is_alive || p.has_asked_question) continue;

        // Pick a random alive target (not self).
        std::vector<int64_t> targets;
        for(int64_t j = 0; j < N; ++j){
            if(j != i && players[static_cast<size_t>(j)].is_alive) targets.push_back(j);
        }
        if(targets.empty()) continue;

        // Prefer asking the most suspicious player.
        std::sort(targets.begin(), targets.end(), [&](int64_t a, int64_t b){
            return p.suspicion[static_cast<size_t>(a)] > p.suspicion[static_cast<size_t>(b)];
        });

        // 60% chance ask most suspicious, 40% random.
        std::uniform_real_distribution<double> prob(0.0, 1.0);
        int64_t target = targets[0];
        if(prob(ww_game.re) > 0.6 && targets.size() > 1){
            std::uniform_int_distribution<size_t> rdist(0, targets.size()-1);
            target = targets[rdist(ww_game.re)];
        }

        // Pick a question from round pool.
        if(ww_game.round_question_indices.empty()) continue;
        std::uniform_int_distribution<size_t> qdist(0, ww_game.round_question_indices.size()-1);
        const auto qi = ww_game.round_question_indices[qdist(ww_game.re)];

        p.has_asked_question = true;
        p.question_target = target;
        p.question_idx = qi;

        if(target == 0){
            ww_game.human_asked_by = i;
            ww_game.human_asked_question = qi;
        }
    }
}

void WerewolfGame::AIRespond(){
    const auto N = static_cast<int64_t>(players.size());
    for(int64_t i = 0; i < N; ++i){
        auto &p = players[static_cast<size_t>(i)];
        if(!p.is_alive) continue;

        // Check if anyone asked this player a question.
        for(int64_t j = 0; j < N; ++j){
            if(j == i) continue;
            auto &asker = players[static_cast<size_t>(j)];
            if(asker.question_target == i && asker.question_idx >= 0){
                if(i == 0) continue; // Human responds via UI.

                const auto qi = static_cast<size_t>(asker.question_idx);
                if(qi >= all_responses.size()) continue;
                const auto &resps = all_responses[qi];
                if(resps.empty()) continue;

                // Werewolf picks least suspicious response; townsperson picks randomly with slight preference for honest ones.
                if(p.is_werewolf){
                    size_t best = 0;
                    double best_susp = 999.0;
                    for(size_t r = 0; r < resps.size(); ++r){
                        if(resps[r].suspicion_if_wolf < best_susp){
                            best_susp = resps[r].suspicion_if_wolf;
                            best = r;
                        }
                    }
                    // Sometimes pick suboptimal to seem natural.
                    std::uniform_real_distribution<double> prob(0.0, 1.0);
                    if(prob(ww_game.re) < 0.25 && resps.size() > 1){
                        std::uniform_int_distribution<size_t> rdist(0, resps.size()-1);
                        best = rdist(ww_game.re);
                    }
                    p.response_idx = static_cast<int64_t>(best);
                } else {
                    // Townsperson: random with preference for low-suspicion.
                    std::vector<double> weights;
                    for(const auto &r : resps) weights.push_back(1.0 / (0.1 + r.suspicion_if_town));
                    std::discrete_distribution<size_t> ddist(weights.begin(), weights.end());
                    p.response_idx = static_cast<int64_t>(ddist(ww_game.re));
                }

                // Update asker's suspicion of this player based on response.
                const auto ri = static_cast<size_t>(p.response_idx);
                if(ri < resps.size()){
                    double susp_delta = resps[ri].suspicion_if_wolf * 0.5; // rough heuristic
                    asker.suspicion[static_cast<size_t>(i)] += susp_delta;
                }
                break; // Only one question per player.
            }
        }
    }
}

void WerewolfGame::AIVote(){
    const auto N = static_cast<int64_t>(players.size());
    for(int64_t i = 1; i < N; ++i){
        auto &p = players[static_cast<size_t>(i)];
        if(!p.is_alive) continue;

        // Vote for the most suspicious alive player (not self).
        int64_t best = -1;
        double best_susp = -1.0;
        for(int64_t j = 0; j < N; ++j){
            if(j == i || !players[static_cast<size_t>(j)].is_alive) continue;
            if(p.suspicion[static_cast<size_t>(j)] > best_susp){
                best_susp = p.suspicion[static_cast<size_t>(j)];
                best = j;
            }
        }

        // Werewolf strategic voting: vote for whoever has the second-most suspicion on them from others.
        if(p.is_werewolf){
            // Try to get a townsperson lynched.
            std::vector<std::pair<double,int64_t>> susp_sums;
            for(int64_t j = 0; j < N; ++j){
                if(j == i || !players[static_cast<size_t>(j)].is_alive) continue;
                double sum = 0.0;
                for(int64_t k = 0; k < N; ++k){
                    if(k == j || !players[static_cast<size_t>(k)].is_alive) continue;
                    sum += players[static_cast<size_t>(k)].suspicion[static_cast<size_t>(j)];
                }
                susp_sums.push_back({sum, j});
            }
            std::sort(susp_sums.begin(), susp_sums.end(), [](const auto &a, const auto &b){ return a.first > b.first; });
            if(!susp_sums.empty()){
                // Vote for most-suspected townsperson.
                for(const auto &ss : susp_sums){
                    if(!players[static_cast<size_t>(ss.second)].is_werewolf){
                        best = ss.second;
                        break;
                    }
                }
                if(best < 0 && !susp_sums.empty()) best = susp_sums[0].second;
            }
        }

        p.vote_target = best;
    }
}

void WerewolfGame::TallyVotes(){
    const auto N = static_cast<int64_t>(players.size());
    ww_game.vote_counts.assign(static_cast<size_t>(N), 0);
    for(int64_t i = 0; i < N; ++i){
        if(!players[static_cast<size_t>(i)].is_alive) continue;
        const auto vt = players[static_cast<size_t>(i)].vote_target;
        if(vt >= 0 && vt < N){
            ww_game.vote_counts[static_cast<size_t>(vt)]++;
        }
    }
}

void WerewolfGame::PerformLynch(){
    const auto N = static_cast<int64_t>(players.size());
    int64_t max_votes = 0;
    int64_t lynched = -1;
    for(int64_t i = 0; i < N; ++i){
        if(!players[static_cast<size_t>(i)].is_alive) continue;
        if(ww_game.vote_counts[static_cast<size_t>(i)] > max_votes){
            max_votes = ww_game.vote_counts[static_cast<size_t>(i)];
            lynched = i;
        }
    }
    // Tie-break: random among tied.
    if(lynched >= 0){
        std::vector<int64_t> tied;
        for(int64_t i = 0; i < N; ++i){
            if(players[static_cast<size_t>(i)].is_alive && ww_game.vote_counts[static_cast<size_t>(i)] == max_votes){
                tied.push_back(i);
            }
        }
        if(tied.size() > 1){
            std::uniform_int_distribution<size_t> tdist(0, tied.size()-1);
            lynched = tied[tdist(ww_game.re)];
        }
    }

    ww_game.lynched_player = lynched;
    if(lynched >= 0){
        auto &lp = players[static_cast<size_t>(lynched)];
        lp.is_alive = false;
        ww_game.lynched_was_wolf = lp.is_werewolf;
        ww_game.last_lynch_name = lp.persona.name;
        ww_game.last_lynch_was_wolf = lp.is_werewolf;
        ww_game.any_lynch_happened = true;

        if(lp.is_werewolf){
            AddLog(lp.persona.name + " was lynched. They WERE the werewolf!", ImVec4(0.3f,1.0f,0.3f,1.0f));
        } else {
            AddLog(lp.persona.name + " was lynched. They were NOT the werewolf.", ImVec4(1.0f,0.5f,0.2f,1.0f));
        }
    }
}

bool WerewolfGame::CheckGameOver(){
    if(CountAliveWolves() == 0){
        ww_game.phase = ww_phase_t::GameOver;
        AddLog("The werewolf has been found! Townspeople win!", ImVec4(0.3f,1.0f,0.3f,1.0f));
        return true;
    }
    if(CountAliveTown() <= 1){
        ww_game.phase = ww_phase_t::GameOver;
        AddLog("The werewolf prevails! The village is lost.", ImVec4(1.0f,0.3f,0.3f,1.0f));
        return true;
    }
    // Human dead?
    if(!players[0].is_alive){
        ww_game.phase = ww_phase_t::GameOver;
        AddLog("You have been lynched. Game over.", ImVec4(1.0f,0.3f,0.3f,1.0f));
        return true;
    }
    return false;
}

// ============================================================================
// Drawing helpers
// ============================================================================

void WerewolfGame::DrawMonolith(ImDrawList *dl, ImVec2 center, float w, float h,
                                 const std::string &name, bool alive, bool highlighted,
                                 bool is_human_player, float glow, ImU32 extra_color){
    if(!alive){
        // Fallen monolith: draw at an angle, darker.
        const ImU32 col = IM_COL32(60, 60, 60, 180);
        ImVec2 tl(center.x - w*0.5f, center.y - h*0.15f);
        ImVec2 br(center.x + w*0.5f, center.y + h*0.15f);
        dl->AddRectFilled(tl, br, col, 2.0f);

        // Name below.
        const auto ts = ImGui::CalcTextSize(name.c_str());
        dl->AddText(ImVec2(center.x - ts.x*0.5f, center.y + h*0.2f), IM_COL32(120,120,120,200), name.c_str());
        return;
    }

    // Glow effect.
    if(glow > 0.01f){
        const auto ga = static_cast<uint8_t>(glow * 80.0f);
        ImU32 gcol = extra_color ? extra_color : IM_COL32(100, 150, 255, ga);
        ImVec2 gtl(center.x - w*0.7f, center.y - h*0.6f);
        ImVec2 gbr(center.x + w*0.7f, center.y + h*0.6f);
        dl->AddRectFilled(gtl, gbr, gcol, 6.0f);
    }

    // Monolith body.
    ImU32 body_col = IM_COL32(130, 130, 140, 255);
    if(is_human_player) body_col = IM_COL32(140, 140, 160, 255);
    if(highlighted) body_col = IM_COL32(180, 180, 200, 255);

    ImVec2 tl(center.x - w*0.5f, center.y - h*0.5f);
    ImVec2 br(center.x + w*0.5f, center.y + h*0.5f);
    dl->AddRectFilled(tl, br, body_col, 3.0f);

    // Subtle top highlight.
    dl->AddRectFilled(tl, ImVec2(br.x, tl.y + 4.0f), IM_COL32(180,180,190,200), 3.0f);

    // Border.
    dl->AddRect(tl, br, IM_COL32(80,80,90,255), 3.0f, 0, 1.5f);

    // Name below monolith.
    const auto ts = ImGui::CalcTextSize(name.c_str());
    dl->AddText(ImVec2(center.x - ts.x*0.5f, br.y + 3.0f), IM_COL32(220,220,220,255), name.c_str());
}

void WerewolfGame::DrawVotingArrow(ImDrawList *dl, ImVec2 from, ImVec2 to, ImU32 color){
    dl->AddLine(from, to, color, 2.0f);
    // Arrowhead.
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float len = std::sqrt(dx*dx + dy*dy);
    if(len < 1.0f) return;
    const float nx = dx / len;
    const float ny = dy / len;
    const float ax = -ny * 5.0f;
    const float ay = nx * 5.0f;
    ImVec2 p1(to.x - nx*10.0f + ax, to.y - ny*10.0f + ay);
    ImVec2 p2(to.x - nx*10.0f - ax, to.y - ny*10.0f - ay);
    dl->AddTriangleFilled(to, p1, p2, color);
}

// ============================================================================
// Main Display function
// ============================================================================

bool WerewolfGame::Display(bool &enabled){
    if(!enabled) return true;

    const auto t_now = std::chrono::steady_clock::now();
    const double dt = std::chrono::duration<double>(t_now - t_ww_updated).count();
    t_ww_updated = t_now;

    // Clamp dt to avoid jumps.
    const double cdt = std::min(dt, 0.1);
    ww_game.phase_timer += cdt;

    // Reset on R key.
    if(ImGui::IsKeyPressed(SDL_SCANCODE_R)){
        Reset();
        return true;
    }

    ImGuiWindowFlags wf = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav;

    ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Werewolf", &enabled, wf)){
        ImGui::End();
        return true;
    }

    const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    const ImVec2 canvas_sz(860.0f, 420.0f);
    ImDrawList *dl = ImGui::GetWindowDrawList();

    // Dark background for the play area.
    dl->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_sz.x, canvas_pos.y + canvas_sz.y),
                      IM_COL32(20, 25, 35, 255), 4.0f);

    // --- Draw monoliths in a semicircle, human at center bottom ---
    const auto N = static_cast<int64_t>(players.size());
    const float cx = canvas_pos.x + canvas_sz.x * 0.5f;
    const float cy = canvas_pos.y + canvas_sz.y * 0.55f;
    const float radius_x = canvas_sz.x * 0.38f;
    const float radius_y = canvas_sz.y * 0.38f;
    const float mono_w = 30.0f;
    const float mono_h = 55.0f;

    std::vector<ImVec2> monolith_centers(static_cast<size_t>(N));

    // Human player at bottom center.
    monolith_centers[0] = ImVec2(cx, canvas_pos.y + canvas_sz.y - mono_h * 0.7f);

    // Others in a semicircle from left to right across the top.
    {
        int64_t alive_others = 0;
        for(int64_t i = 1; i < N; ++i) alive_others++; // count all for stable positions
        for(int64_t i = 1; i < N; ++i){
            const double angle = M_PI + M_PI * static_cast<double>(i) / static_cast<double>(N); // spread across top semicircle
            // Bob animation.
            auto &p = players[static_cast<size_t>(i)];
            p.monolith_bob += cdt * 1.2;
            const float bob = p.is_alive ? static_cast<float>(std::sin(p.monolith_bob) * 2.0) : 0.0f;
            monolith_centers[static_cast<size_t>(i)] = ImVec2(
                cx + radius_x * static_cast<float>(std::cos(angle)),
                cy + radius_y * static_cast<float>(std::sin(angle)) + bob
            );
        }
    }

    // Human player bob.
    players[0].monolith_bob += cdt * 1.0;
    monolith_centers[0].y += static_cast<float>(std::sin(players[0].monolith_bob) * 1.5);

    // Update glow animations.
    for(int64_t i = 0; i < N; ++i){
        auto &p = players[static_cast<size_t>(i)];
        const float target_glow = (ww_game.phase == ww_phase_t::VoteResult &&
                                   ww_game.lynched_player == i) ? 1.0f : 0.0f;
        p.glow_alpha += (target_glow - p.glow_alpha) * static_cast<float>(cdt) * 3.0f;
    }

    // Draw vote arrows during VoteResult phase.
    if(ww_game.phase == ww_phase_t::VoteResult || ww_game.phase == ww_phase_t::RoundEnd){
        for(int64_t i = 0; i < N; ++i){
            const auto &p = players[static_cast<size_t>(i)];
            if(!p.is_alive || p.vote_target < 0) continue;
            const auto from = monolith_centers[static_cast<size_t>(i)];
            const auto to = monolith_centers[static_cast<size_t>(p.vote_target)];
            DrawVotingArrow(dl, from, to, IM_COL32(200, 80, 80, 140));
        }
    }

    // Draw monoliths.
    for(int64_t i = 0; i < N; ++i){
        const auto &p = players[static_cast<size_t>(i)];
        bool highlighted = false;
        ImU32 extra = 0;
        if(ww_game.phase == ww_phase_t::QuestionSelect && ww_game.selected_target == i && i != 0){
            highlighted = true;
            extra = IM_COL32(100, 200, 100, 60);
        }
        if(ww_game.phase == ww_phase_t::VoteSelect && ww_game.selected_target == i && i != 0){
            highlighted = true;
            extra = IM_COL32(200, 100, 100, 60);
        }
        DrawMonolith(dl, monolith_centers[static_cast<size_t>(i)], mono_w, mono_h,
                     p.persona.name, p.is_alive, highlighted, p.is_human, p.glow_alpha, extra);
    }

    // Title and round info.
    {
        std::string title = "WEREWOLF - Round " + std::to_string(ww_game.round_number);
        const auto ts = ImGui::CalcTextSize(title.c_str());
        dl->AddText(ImVec2(canvas_pos.x + canvas_sz.x*0.5f - ts.x*0.5f, canvas_pos.y + 5.0f),
                    IM_COL32(240,220,150,255), title.c_str());
    }

    // Show role reminder.
    if(players[0].is_alive){
        const char *role_str = players[0].is_werewolf ? "[You are the WEREWOLF]" : "[You are a Townsperson]";
        ImU32 role_col = players[0].is_werewolf ? IM_COL32(255,80,80,255) : IM_COL32(80,200,80,255);
        const auto rs = ImGui::CalcTextSize(role_str);
        dl->AddText(ImVec2(canvas_pos.x + canvas_sz.x*0.5f - rs.x*0.5f, canvas_pos.y + 22.0f),
                    role_col, role_str);
    }

    // Show last lynch info.
    if(ww_game.any_lynch_happened){
        std::string info = "Last lynched: " + ww_game.last_lynch_name + 
            (ww_game.last_lynch_was_wolf ? " (was the werewolf)" : " (was NOT the werewolf)");
        ImU32 icol = ww_game.last_lynch_was_wolf ? IM_COL32(80,220,80,200) : IM_COL32(220,160,80,200);
        const auto is2 = ImGui::CalcTextSize(info.c_str());
        dl->AddText(ImVec2(canvas_pos.x + canvas_sz.x*0.5f - is2.x*0.5f, canvas_pos.y + 38.0f), icol, info.c_str());
    }

    // Advance cursor past the canvas.
    ImGui::Dummy(canvas_sz);
    ImGui::Separator();

    // === Phase-specific UI below the canvas ===
    const float panel_w = ImGui::GetContentRegionAvail().x;

    switch(ww_game.phase){
    case ww_phase_t::Setup:
    {
        ImGui::TextWrapped("Welcome to Werewolf! One among you is a werewolf in disguise.");
        ImGui::Spacing();
        if(ww_game.show_role_reveal && ww_game.phase_timer < 3.0){
            if(players[0].is_werewolf){
                ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "You are the WEREWOLF. Survive!");
            } else {
                ImGui::TextColored(ImVec4(0.3f,1,0.3f,1), "You are a Townsperson. Find the werewolf!");
            }
            ImGui::TextColored(ImVec4(0.7f,0.7f,0.7f,1), "Round starts in %.0f...", 3.0 - ww_game.phase_timer);
        }
        if(ww_game.phase_timer >= 3.0){
            StartRound();
        }
        break;
    }
    case ww_phase_t::QuestionSelect:
    {
        ImGui::TextWrapped("Select a player to ask a question (click a monolith above), then choose a question.");

        // Player can click monoliths to select target.
        for(int64_t i = 1; i < N; ++i){
            if(!players[static_cast<size_t>(i)].is_alive) continue;
            const auto &mc = monolith_centers[static_cast<size_t>(i)];
            ImVec2 tl(mc.x - mono_w*0.5f, mc.y - mono_h*0.5f);
            ImVec2 br(mc.x + mono_w*0.5f, mc.y + mono_h*0.5f);
            if(ImGui::IsMouseHoveringRect(tl, br) && ImGui::IsMouseClicked(0)){
                ww_game.selected_target = i;
                ww_game.selected_question = -1;
            }
        }

        if(ww_game.selected_target >= 0){
            ImGui::Text("Asking: %s (%s)", players[static_cast<size_t>(ww_game.selected_target)].persona.name.c_str(),
                        players[static_cast<size_t>(ww_game.selected_target)].persona.profession.c_str());
            ImGui::Spacing();

            for(size_t qi = 0; qi < ww_game.round_question_indices.size(); ++qi){
                const auto qidx = static_cast<size_t>(ww_game.round_question_indices[qi]);
                if(qidx >= all_questions.size()) continue;
                const auto &q = all_questions[qidx];
                std::string label = q.text + "##q" + std::to_string(qi);
                if(ImGui::Selectable(label.c_str(), ww_game.selected_question == static_cast<int64_t>(qi))){
                    ww_game.selected_question = static_cast<int64_t>(qi);
                }
            }

            if(ww_game.selected_question >= 0 && ImGui::Button("Ask Question")){
                const auto real_qi = ww_game.round_question_indices[static_cast<size_t>(ww_game.selected_question)];
                players[0].has_asked_question = true;
                players[0].question_target = ww_game.selected_target;
                players[0].question_idx = real_qi;

                AddLog("You asked " + players[static_cast<size_t>(ww_game.selected_target)].persona.name + ": \"" +
                       all_questions[static_cast<size_t>(real_qi)].text + "\"", ImVec4(0.7f,0.9f,1.0f,1.0f));

                ww_game.phase = ww_phase_t::QuestionsAI;
                ww_game.phase_timer = 0.0;
                ww_game.ai_action_idx = 1;
                ww_game.action_delay = 0.5;
            }
        }

        // Allow skipping.
        if(ImGui::Button("Skip (don't ask anyone)")){
            players[0].has_asked_question = true;
            players[0].question_target = -1;
            ww_game.phase = ww_phase_t::QuestionsAI;
            ww_game.phase_timer = 0.0;
            ww_game.ai_action_idx = 1;
            ww_game.action_delay = 0.5;
        }
        break;
    }
    case ww_phase_t::QuestionsAI:
    {
        ImGui::TextWrapped("Other players are asking questions...");
        ww_game.action_delay -= cdt;
        if(ww_game.action_delay <= 0.0){
            // Have all AI ask.
            AIAskQuestions();
            // Log AI questions.
            for(int64_t i = 1; i < N; ++i){
                const auto &p = players[static_cast<size_t>(i)];
                if(!p.is_alive || p.question_target < 0) continue;
                AddLog(p.persona.name + " asks " + players[static_cast<size_t>(p.question_target)].persona.name + ": \"" +
                       all_questions[static_cast<size_t>(p.question_idx)].text + "\"", ImVec4(0.8f,0.8f,0.6f,1.0f));
            }
            // Transition: check if human was asked.
            if(ww_game.human_asked_by >= 0){
                ww_game.phase = ww_phase_t::ResponseSelect;
            } else {
                ww_game.phase = ww_phase_t::ResponsesAI;
                ww_game.action_delay = 1.0;
            }
            ww_game.phase_timer = 0.0;
        }
        break;
    }
    case ww_phase_t::ResponseSelect:
    {
        const auto asker = ww_game.human_asked_by;
        const auto qi = ww_game.human_asked_question;
        ImGui::TextWrapped("%s asks you: \"%s\"",
            players[static_cast<size_t>(asker)].persona.name.c_str(),
            all_questions[static_cast<size_t>(qi)].text.c_str());
        ImGui::Spacing();
        ImGui::Text("Choose your response:");

        const auto &resps = all_responses[static_cast<size_t>(qi)];
        for(size_t ri = 0; ri < resps.size(); ++ri){
            std::string label = resps[ri].text + "##r" + std::to_string(ri);
            if(ImGui::Selectable(label.c_str(), ww_game.selected_response == static_cast<int64_t>(ri))){
                ww_game.selected_response = static_cast<int64_t>(ri);
            }
        }

        if(ww_game.selected_response >= 0 && ImGui::Button("Respond")){
            players[0].response_idx = ww_game.selected_response;
            AddLog("You responded: \"" + resps[static_cast<size_t>(ww_game.selected_response)].text + "\"",
                   ImVec4(0.7f,0.9f,1.0f,1.0f));

            // Update asker's suspicion.
            const auto ri = static_cast<size_t>(ww_game.selected_response);
            if(ri < resps.size()){
                players[static_cast<size_t>(asker)].suspicion[0] += resps[ri].suspicion_if_wolf * 0.5;
            }

            ww_game.phase = ww_phase_t::ResponsesAI;
            ww_game.phase_timer = 0.0;
            ww_game.action_delay = 1.0;
        }
        break;
    }
    case ww_phase_t::ResponsesAI:
    {
        ImGui::TextWrapped("Players are responding to questions...");
        ww_game.action_delay -= cdt;
        if(ww_game.action_delay <= 0.0){
            AIRespond();

            // Log responses and show the answer to the human's question.
            if(players[0].question_target >= 0){
                const auto tgt = static_cast<size_t>(players[0].question_target);
                const auto &tp = players[tgt];
                if(tp.response_idx >= 0){
                    const auto qi = static_cast<size_t>(players[0].question_idx);
                    if(qi < all_responses.size()){
                        const auto &resps = all_responses[qi];
                        const auto ri = static_cast<size_t>(tp.response_idx);
                        if(ri < resps.size()){
                            AddLog(tp.persona.name + " responds: \"" + resps[ri].text + "\"",
                                   ImVec4(0.9f,0.9f,0.7f,1.0f));
                        }
                    }
                }
            }

            // Log other responses visible to the group.
            for(int64_t i = 1; i < N; ++i){
                const auto &asker = players[static_cast<size_t>(i)];
                if(!asker.is_alive || asker.question_target < 0) continue;
                const auto tgt = static_cast<size_t>(asker.question_target);
                if(static_cast<int64_t>(tgt) == 0) continue; // human already handled
                const auto &tp = players[tgt];
                if(tp.response_idx >= 0){
                    const auto qi = static_cast<size_t>(asker.question_idx);
                    if(qi < all_responses.size()){
                        const auto &resps = all_responses[qi];
                        const auto ri = static_cast<size_t>(tp.response_idx);
                        if(ri < resps.size()){
                            AddLog(tp.persona.name + " responds to " + asker.persona.name + ": \"" + resps[ri].text + "\"",
                                   ImVec4(0.7f,0.7f,0.6f,1.0f));
                        }
                    }
                }
            }

            ww_game.phase = ww_phase_t::Discussion;
            ww_game.phase_timer = 0.0;
        }
        break;
    }
    case ww_phase_t::Discussion:
    {
        ImGui::TextWrapped("The townspeople discuss what they have heard...");

        // Show gossip about a random alive player.
        if(ww_game.phase_timer < 0.1){
            std::vector<int64_t> alive_idx;
            for(int64_t i = 0; i < N; ++i){
                if(players[static_cast<size_t>(i)].is_alive) alive_idx.push_back(i);
            }
            if(!alive_idx.empty()){
                std::uniform_int_distribution<size_t> gd(0, alive_idx.size()-1);
                const auto gi = alive_idx[gd(ww_game.re)];
                const auto &gp = players[static_cast<size_t>(gi)];
                AddLog("Gossip: " + gp.persona.gossip, ImVec4(0.6f,0.6f,0.8f,1.0f));
            }
        }

        if(ww_game.phase_timer >= 2.5){
            ww_game.phase = ww_phase_t::VoteSelect;
            ww_game.phase_timer = 0.0;
            ww_game.selected_target = -1;
        }
        break;
    }
    case ww_phase_t::VoteSelect:
    {
        ImGui::TextWrapped("Vote! Click on a monolith to vote for who you think is the werewolf.");

        for(int64_t i = 1; i < N; ++i){
            if(!players[static_cast<size_t>(i)].is_alive) continue;
            const auto &mc = monolith_centers[static_cast<size_t>(i)];
            ImVec2 tl(mc.x - mono_w*0.5f, mc.y - mono_h*0.5f);
            ImVec2 br(mc.x + mono_w*0.5f, mc.y + mono_h*0.5f);
            if(ImGui::IsMouseHoveringRect(tl, br) && ImGui::IsMouseClicked(0)){
                ww_game.selected_target = i;
            }
        }

        if(ww_game.selected_target >= 0){
            ImGui::Text("Your vote: %s", players[static_cast<size_t>(ww_game.selected_target)].persona.name.c_str());
            if(ImGui::Button("Confirm Vote")){
                players[0].vote_target = ww_game.selected_target;
                AddLog("You voted for " + players[static_cast<size_t>(ww_game.selected_target)].persona.name + ".",
                       ImVec4(0.9f,0.7f,0.7f,1.0f));
                ww_game.phase = ww_phase_t::VotesAI;
                ww_game.phase_timer = 0.0;
                ww_game.action_delay = 1.5;
            }
        }
        break;
    }
    case ww_phase_t::VotesAI:
    {
        ImGui::TextWrapped("The other players are casting their votes...");
        ww_game.action_delay -= cdt;
        if(ww_game.action_delay <= 0.0){
            AIVote();
            TallyVotes();
            ww_game.phase = ww_phase_t::VoteResult;
            ww_game.phase_timer = 0.0;
        }
        break;
    }
    case ww_phase_t::VoteResult:
    {
        ImGui::TextWrapped("The votes are in...");

        // Show vote counts.
        for(int64_t i = 0; i < N; ++i){
            if(!players[static_cast<size_t>(i)].is_alive && ww_game.lynched_player != i) continue;
            const auto vc = ww_game.vote_counts[static_cast<size_t>(i)];
            if(vc > 0){
                ImGui::Text("  %s: %lld vote(s)", players[static_cast<size_t>(i)].persona.name.c_str(),
                            static_cast<long long>(vc));
            }
        }

        if(ww_game.phase_timer >= 3.0 && ww_game.lynched_player < 0){
            PerformLynch();
            ww_game.phase = ww_phase_t::RoundEnd;
            ww_game.phase_timer = 0.0;
        } else if(ww_game.phase_timer >= 3.0 && ww_game.lynched_player >= 0){
            ww_game.phase = ww_phase_t::RoundEnd;
            ww_game.phase_timer = 0.0;
        }
        if(ww_game.phase_timer >= 2.0 && ww_game.lynched_player < 0){
            PerformLynch();
        }
        break;
    }
    case ww_phase_t::RoundEnd:
    {
        if(ww_game.lynched_player >= 0){
            const auto &lp = players[static_cast<size_t>(ww_game.lynched_player)];
            if(ww_game.lynched_was_wolf){
                ImGui::TextColored(ImVec4(0.3f,1,0.3f,1), "%s was the werewolf! The townspeople win!",
                                   lp.persona.name.c_str());
            } else {
                ImGui::TextColored(ImVec4(1,0.5f,0.2f,1), "%s was NOT the werewolf. The hunt continues...",
                                   lp.persona.name.c_str());
            }
        }

        if(ww_game.phase_timer >= 3.0){
            if(!CheckGameOver()){
                StartRound();
            }
        }
        break;
    }
    case ww_phase_t::GameOver:
    {
        if(CountAliveWolves() == 0){
            ImGui::TextColored(ImVec4(0.3f,1,0.3f,1), "VICTORY! The werewolf has been eliminated!");
        } else {
            ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "DEFEAT! The werewolf wins.");
        }
        ImGui::Spacing();
        if(ImGui::Button("Play Again")){
            Reset();
        }
        break;
    }
    } // end switch

    ImGui::Separator();

    // === Game Log ===
    ImGui::Text("Game Log:");
    ImGui::BeginChild("WerewolfLog", ImVec2(panel_w, 120.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
    for(const auto &entry : ww_game.game_log){
        ImGui::TextColored(entry.color, "%s", entry.text.c_str());
    }
    if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY()){
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    ImGui::End();
    return true;
}
