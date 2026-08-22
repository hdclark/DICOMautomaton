//ScheduleCoverage.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Explicator.h>

#include "YgorString.h"
#include "YgorLog.h"

#include "../Structs.h"
#include "../Tables.h"
#include "../Regex_Selectors.h"
#include "../Metadata.h"

#include "ScheduleCoverage.h"


namespace ScheduleCoverageCore {

namespace {

// --------------------------------------------------------------------------------------------------------------------------
// Small string helpers.
// --------------------------------------------------------------------------------------------------------------------------

std::string trim(const std::string &s){
    const auto first = std::find_if(s.begin(), s.end(), [](unsigned char c){ return !std::isspace(c); });
    const auto last = std::find_if(s.rbegin(), s.rend(), [](unsigned char c){ return !std::isspace(c); }).base();
    if(first >= last) return std::string();
    return std::string(first, last);
}

std::string to_lower(const std::string &s){
    std::string out;
    out.reserve(s.size());
    for(const char c : s) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

std::string to_upper(const std::string &s){
    std::string out;
    out.reserve(s.size());
    for(const char c : s) out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    return out;
}

std::vector<std::string> split_ws(const std::string &s){
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string tok;
    while(iss >> tok) out.push_back(tok);
    return out;
}

std::string join(const std::vector<int64_t> &v, const std::string &sep){
    std::stringstream ss;
    for(size_t i = 0; i < v.size(); ++i){
        if(i != 0) ss << sep;
        ss << v[i];
    }
    return ss.str();
}

// Format a double as an integer when it is integral, otherwise with up to 6 significant fractional digits.
std::string format_double(double d){
    if(std::isnan(d)) return "nan";
    if(std::isinf(d)) return (d < 0.0) ? "-inf" : "inf";
    const double rounded = std::round(d);
    if(std::abs(d - rounded) < 1e-9){
        std::stringstream ss;
        ss << static_cast<int64_t>(rounded);
        return ss.str();
    }
    std::stringstream ss;
    ss.precision(6);
    ss << d;
    return ss.str();
}

// --------------------------------------------------------------------------------------------------------------------------
// Term matching and classification.
// --------------------------------------------------------------------------------------------------------------------------

// Match a single (trimmed) cell against a list of terms using either exact or regex matching. Case-insensitive.
bool term_list_matches(const std::string &cell,
                       const std::vector<std::string> &terms,
                       bool regex_mode){
    const std::string cell_trimmed = trim(cell);
    for(const auto &term_raw : terms){
        const std::string term = trim(term_raw);
        if(term.empty()) continue;
        if(regex_mode){
            try{
                const auto re = std::regex(term, std::regex::icase | std::regex::optimize | std::regex::ECMAScript);
                if(std::regex_match(cell_trimmed, re)) return true;
            }catch(const std::regex_error &){
                throw std::runtime_error("Unable to compile cell-classification regex '" + term + "'");
            }
        }else{
            if(to_lower(cell_trimmed) == to_lower(term)) return true;
        }
    }
    return false;
}

bool is_vacation_cell(const std::string &raw){
    return (to_lower(trim(raw)) == "vac");
}

// --------------------------------------------------------------------------------------------------------------------------
// Quota parsing.
// --------------------------------------------------------------------------------------------------------------------------

bool is_all_digits(const std::string &s){
    if(s.empty()) return false;
    for(const char c : s){
        if(!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

} // anonymous namespace


// --------------------------------------------------------------------------------------------------------------------------
// Public: classification.
// --------------------------------------------------------------------------------------------------------------------------

CellClass classify_cell(const std::string &raw,
                        const TermLists &terms,
                        bool *matched_known){
    if(matched_known) *matched_known = false;
    const std::string cell = trim(raw);
    if(cell.empty()){
        // An empty cell is not "known"; it is treated as immutable/non-counting.
        return CellClass::Immutable;
    }

    const auto mark = [&](bool b) -> bool {
        if(matched_known && b) *matched_known = true;
        return b;
    };

    // Priority order. The default term lists are disjoint, but the explicit order makes behavior deterministic when a
    // user configures overlapping terms.
    if(mark(term_list_matches(cell, terms.holiday, terms.regex_mode)))    return CellClass::Holiday;
    if(mark(term_list_matches(cell, terms.onsite, terms.regex_mode)))     return CellClass::Onsite;
    if(mark(term_list_matches(cell, terms.remote_pref, terms.regex_mode)))return CellClass::RemotePreference;
    if(mark(term_list_matches(cell, terms.undecided, terms.regex_mode)))  return CellClass::Undecided;
    if(mark(term_list_matches(cell, terms.remote, terms.regex_mode)))     return CellClass::Remote;
    if(mark(term_list_matches(cell, terms.immutable, terms.regex_mode)))  return CellClass::Immutable;

    // Unknown term: fall back to immutable/non-counting. The caller is responsible for emitting a warning.
    return CellClass::Immutable;
}


// --------------------------------------------------------------------------------------------------------------------------
// Public: quota parsing.
// --------------------------------------------------------------------------------------------------------------------------

bool parse_quota(const std::string &quota,
                 int64_t &min_onsite,
                 std::vector<std::string> &subset){
    subset.clear();
    min_onsite = 1;

    const std::string s = trim(quota);
    if(s.empty()) return false;

    const auto tokens = split_ws(s);

    bool has_or = false;
    for(const auto &t : tokens){
        if(to_upper(t) == "OR") has_or = true;
    }

    if(has_or){
        // An OR-list. Optional "any <N> of" prefix is accepted, but a plain list defaults to a minimum of 1.
        std::vector<std::string> parts;
        for(const auto &t : tokens){
            if(to_upper(t) == "OR") continue;
            parts.push_back(t);
        }

        size_t start = 0;
        if(parts.size() >= 3
           && (to_lower(parts[0]) == "any")
           && is_all_digits(parts[1])
           && (to_lower(parts[2]) == "of")){
            min_onsite = std::stoll(parts[1]);
            start = 3;
        }

        if(start >= parts.size()) return false;
        subset.assign(parts.begin() + static_cast<std::ptrdiff_t>(start), parts.end());
        return !subset.empty();
    }

    if(tokens.size() == 1 && to_lower(tokens[0]) == "any"){
        min_onsite = 1;
        subset.clear();
        return true;
    }

    if(tokens.size() >= 1 && to_lower(tokens[0]) == "any"){
        // "any <N>".
        if(tokens.size() == 2 && is_all_digits(tokens[1])){
            min_onsite = std::stoll(tokens[1]);
            subset.clear();
            return true;
        }
        return false;
    }

    if(tokens.size() == 1 && is_all_digits(tokens[0])){
        min_onsite = std::stoll(tokens[0]);
        subset.clear();
        return true;
    }

    return false;
}


// --------------------------------------------------------------------------------------------------------------------------
// Public: schedule parsing.
// --------------------------------------------------------------------------------------------------------------------------

ParsedSchedule parse_schedule(const tables::table2 &table,
                              const std::string &requirement_regex,
                              const std::string &header_regex,
                              const TermLists &terms){
    ParsedSchedule out;

    if(table.data.empty()){
        throw std::runtime_error("The selected table is empty; there is no schedule to process");
    }

    const auto re_req = Compile_Regex(requirement_regex);
    const auto re_hdr = Compile_Regex(header_regex);

    const auto [min_row, max_row] = table.min_max_row();
    const auto [min_col, max_col] = table.min_max_col();

    bool in_schedule = false;
    std::vector<int64_t> staff_columns;

    for(int64_t r = min_row; r <= max_row; ++r){
        const auto first_cell_opt = table.value(r, min_col);

        if(!in_schedule){
            if(first_cell_opt && std::regex_search(first_cell_opt.value(), re_req)){
                Requirement req;
                req.label = first_cell_opt.value();
                req.type = table.value(r, min_col + 1).value_or("");
                req.quota_raw = table.value(r, min_col + 2).value_or("");
                if(!parse_quota(req.quota_raw, req.min_onsite, req.subset)){
                    throw std::runtime_error("Unable to parse quota expression '" + req.quota_raw
                                             + "' for requirement '" + req.label + "'");
                }
                out.requirements.push_back(std::move(req));
                continue;
            }

            if(first_cell_opt && std::regex_search(first_cell_opt.value(), re_hdr)){
                // First header row: read the staff names.
                for(int64_t c = min_col + 1; c <= max_col; ++c){
                    const auto v_opt = table.value(r, c);
                    if(v_opt && !trim(v_opt.value()).empty()){
                        out.staff.push_back(v_opt.value());
                        staff_columns.push_back(c);
                    }
                }
                if(out.staff.empty()){
                    throw std::runtime_error("A schedule header row was found, but no staff columns were identified");
                }
                out.staff_columns = staff_columns;
                in_schedule = true;
                continue;
            }

            // Otherwise: a blank or unrelated row before the schedule region. Skip it.
            continue;
        }

        // In the schedule region.
        if(first_cell_opt && std::regex_search(first_cell_opt.value(), re_hdr)){
            // A repeated header row (a new week block). Verify consistency with the first header.
            std::vector<std::string> header_staff;
            for(int64_t c = min_col + 1; c <= max_col; ++c){
                const auto v_opt = table.value(r, c);
                if(v_opt && !trim(v_opt.value()).empty()) header_staff.push_back(v_opt.value());
            }
            if(header_staff.size() != out.staff.size()){
                YLOGWARN("Repeated header row has a different number of staff columns than the first header; "
                         "continuing with the first header's staff mapping");
            }
            continue;
        }

        if(!first_cell_opt){
            // Blank row: a separator between blocks. Skip it.
            continue;
        }

        // A date row.
        Day day;
        day.row = r;
        day.date = first_cell_opt.value();
        day.cells.resize(out.staff.size());
        day.classes.resize(out.staff.size());

        for(size_t i = 0; i < out.staff.size(); ++i){
            const auto v_opt = table.value(r, staff_columns[i]);
            day.cells[i] = v_opt.value_or("");
            bool known = false;
            day.classes[i] = classify_cell(day.cells[i], terms, &known);
            if(!known && !day.cells[i].empty()){
                YLOGWARN("Unknown cell term '" << day.cells[i] << "' for staff '" << out.staff[i]
                         << "' on '" << day.date << "'; treating as immutable/non-counting");
            }
        }

        // A day is a holiday only when every staff cell is a holiday marker.
        const bool all_holiday = std::all_of(day.classes.begin(), day.classes.end(),
                                             [](CellClass c){ return c == CellClass::Holiday; });
        day.holiday = all_holiday;
        if(!all_holiday){
            const bool any_holiday = std::any_of(day.classes.begin(), day.classes.end(),
                                                 [](CellClass c){ return c == CellClass::Holiday; });
            if(any_holiday){
                YLOGWARN("Day '" << day.date << "' has a mix of holiday and non-holiday cells; "
                         "holiday cells will be treated as non-counting");
            }
        }

        out.days.push_back(std::move(day));
    }

    if(!in_schedule){
        throw std::runtime_error("No schedule header row matching '" + header_regex + "' was found");
    }
    if(out.days.empty()){
        throw std::runtime_error("No schedule date rows were found after the header");
    }

    return out;
}


// --------------------------------------------------------------------------------------------------------------------------
// Public: requirement model.
// --------------------------------------------------------------------------------------------------------------------------

RequirementModel build_requirement_model(const ParsedSchedule &schedule){
    RequirementModel model;

    std::map<std::string, int64_t> name_to_idx;
    for(size_t i = 0; i < schedule.staff.size(); ++i){
        name_to_idx[schedule.staff[i]] = static_cast<int64_t>(i);
    }

    for(const auto &req : schedule.requirements){
        std::vector<int64_t> subset;
        if(req.subset.empty()){
            for(size_t i = 0; i < schedule.staff.size(); ++i){
                subset.push_back(static_cast<int64_t>(i));
            }
        }else{
            for(const auto &nm : req.subset){
                const auto it = name_to_idx.find(nm);
                if(it == name_to_idx.end()){
                    throw std::runtime_error("Requirement '" + req.label + "' references staff '" + nm
                                             + "' which is not present in the schedule header");
                }
                subset.push_back(it->second);
            }
        }
        model.subsets.push_back(std::move(subset));
        model.min_onsite.push_back(req.min_onsite);
    }

    return model;
}


// --------------------------------------------------------------------------------------------------------------------------
// Public: violation evaluation.
// --------------------------------------------------------------------------------------------------------------------------

std::vector<int64_t> evaluate_violation(const std::vector<int64_t> &onsite,
                                        const RequirementModel &model){
    std::vector<int64_t> v(model.subsets.size(), 0);

    std::set<int64_t> on(onsite.begin(), onsite.end());
    for(size_t r = 0; r < model.subsets.size(); ++r){
        int64_t cnt = 0;
        for(const int64_t idx : model.subsets[r]){
            if(on.count(idx) != 0) ++cnt;
        }
        v[r] = std::max<int64_t>(0, model.min_onsite[r] - cnt);
    }
    return v;
}


// --------------------------------------------------------------------------------------------------------------------------
// Public: Phase A -- candidate generation.
// --------------------------------------------------------------------------------------------------------------------------

std::vector<DayCandidate> generate_day_candidates(const Day &day,
                                                  const RequirementModel &model){
    std::vector<DayCandidate> out;

    if(day.holiday) return out;

    // Identify the mutable cells and the fixed on-site cells.
    std::vector<int64_t> var_idx;
    std::vector<bool> var_is_pref;
    std::vector<int64_t> fixed_onsite;

    for(size_t i = 0; i < day.classes.size(); ++i){
        switch(day.classes[i]){
            case CellClass::Onsite:
                fixed_onsite.push_back(static_cast<int64_t>(i));
                break;
            case CellClass::Undecided:
                var_idx.push_back(static_cast<int64_t>(i));
                var_is_pref.push_back(false);
                break;
            case CellClass::RemotePreference:
                var_idx.push_back(static_cast<int64_t>(i));
                var_is_pref.push_back(true);
                break;
            default:
                break;
        }
    }

    const size_t M = var_idx.size();
    if(M >= 63){
        throw std::runtime_error("A day has " + std::to_string(M) + " mutable cells, which is not supported");
    }

    const uint64_t total = (uint64_t{1} << M);
    for(uint64_t mask = 0; mask < total; ++mask){
        DayCandidate cand;
        cand.onsite = fixed_onsite;
        for(size_t b = 0; b < M; ++b){
            if((mask & (uint64_t{1} << b)) != 0){
                cand.onsite.push_back(var_idx[b]);
                if(var_is_pref[b]) cand.overridden.push_back(var_idx[b]);
            }
        }
        std::sort(cand.onsite.begin(), cand.onsite.end());
        std::sort(cand.overridden.begin(), cand.overridden.end());
        cand.violation = evaluate_violation(cand.onsite, model);
        out.push_back(std::move(cand));
    }

    // Deduplicate identical candidates (same on-site set, overrides, and violation vector).
    {
        std::set<std::string> seen;
        std::vector<DayCandidate> dedup;
        dedup.reserve(out.size());
        for(auto &c : out){
            std::stringstream key;
            key << join(c.violation, ",") << "|";
            for(const auto x : c.onsite) key << x << ",";
            key << "|";
            for(const auto x : c.overridden) key << x << ",";
            const std::string k = key.str();
            if(seen.count(k) == 0){
                seen.insert(k);
                dedup.push_back(std::move(c));
            }
        }
        out.swap(dedup);
    }

    // Prune candidates whose violation vector is strictly dominated by another candidate's. Candidates sharing a
    // violation vector are all retained because they may differ in their on-site set (relevant for fairness) or their
    // override count.
    {
        std::vector<size_t> survivors;
        survivors.reserve(out.size());
        for(size_t i = 0; i < out.size(); ++i){
            bool dominated = false;
            for(size_t j = 0; j < out.size() && !dominated; ++j){
                if(i == j) continue;
                const auto &a = out[j].violation; // candidate that might dominate
                const auto &b = out[i].violation; // candidate being tested
                bool leq = true;
                bool lt = false;
                for(size_t k = 0; k < b.size(); ++k){
                    if(a[k] > b[k]){ leq = false; break; }
                    if(a[k] < b[k]) lt = true;
                }
                if(leq && lt) dominated = true;
            }
            if(!dominated) survivors.push_back(i);
        }
        std::vector<DayCandidate> pruned;
        pruned.reserve(survivors.size());
        for(const size_t idx : survivors) pruned.push_back(std::move(out[idx]));
        out.swap(pruned);
    }

    return out;
}


// --------------------------------------------------------------------------------------------------------------------------
// Public: Phase B -- lexicographic baseline selection.
// --------------------------------------------------------------------------------------------------------------------------

std::vector<size_t> select_baseline(const std::vector<std::vector<DayCandidate>> &day_candidates){
    std::vector<size_t> choice(day_candidates.size(), 0);

    const auto lex_less = [](const DayCandidate &a, const DayCandidate &b) -> bool {
        for(size_t k = 0; k < a.violation.size(); ++k){
            if(a.violation[k] != b.violation[k]) return a.violation[k] < b.violation[k];
        }
        return a.overridden.size() < b.overridden.size();
    };

    for(size_t d = 0; d < day_candidates.size(); ++d){
        const auto &cs = day_candidates[d];
        if(cs.empty()) continue;
        size_t best = 0;
        for(size_t i = 1; i < cs.size(); ++i){
            if(lex_less(cs[i], cs[best])) best = i;
        }
        choice[d] = best;
    }
    return choice;
}


// --------------------------------------------------------------------------------------------------------------------------
// Public: fairness.
// --------------------------------------------------------------------------------------------------------------------------

double fairness_penalty(const std::vector<int64_t> &staff_onsite,
                        const std::string &metric){
    if(staff_onsite.empty()) return 0.0;

    if(metric == "range"){
        const auto [mn_it, mx_it] = std::minmax_element(staff_onsite.begin(), staff_onsite.end());
        return static_cast<double>(*mx_it - *mn_it);
    }

    if(metric == "variance"){
        double sum = 0.0;
        for(const int64_t v : staff_onsite) sum += static_cast<double>(v);
        const double mean = sum / static_cast<double>(staff_onsite.size());
        double acc = 0.0;
        for(const int64_t v : staff_onsite){
            const double d = static_cast<double>(v) - mean;
            acc += d * d;
        }
        return acc / static_cast<double>(staff_onsite.size());
    }

    if(metric == "gini"){
        std::vector<int64_t> sorted = staff_onsite;
        std::sort(sorted.begin(), sorted.end());
        int64_t sum = 0;
        for(const int64_t v : sorted) sum += v;
        if(sum == 0) return 0.0;

        // Gini = sum_{i<j} |x_i - x_j| / (n * sum).
        int64_t numerator = 0;
        const size_t n = sorted.size();
        for(size_t i = 0; i < n; ++i){
            for(size_t j = i + 1; j < n; ++j){
                numerator += sorted[j] - sorted[i];
            }
        }
        return static_cast<double>(numerator) / (static_cast<double>(n) * static_cast<double>(sum));
    }

    throw std::runtime_error("Fairness metric '" + metric + "' not understood");
}


// --------------------------------------------------------------------------------------------------------------------------
// Solution construction and local search.
// --------------------------------------------------------------------------------------------------------------------------

namespace {

Solution build_solution(const ParsedSchedule &schedule,
                        const std::vector<std::vector<DayCandidate>> &day_candidates,
                        const std::vector<size_t> &choice,
                        const size_t n_req,
                        const std::string &metric){
    Solution sol;
    const size_t n_days = schedule.days.size();
    sol.day_onsite.resize(n_days);
    sol.day_overridden.resize(n_days);
    sol.day_violation.resize(n_days);
    sol.staff_onsite.assign(schedule.staff.size(), 0);
    sol.violation_sum.assign(n_req, 0);

    for(size_t d = 0; d < n_days; ++d){
        const auto &cs = day_candidates[d];
        if(cs.empty()) continue;
        const auto &c = cs[choice[d]];
        sol.day_onsite[d] = c.onsite;
        sol.day_overridden[d] = c.overridden;
        sol.day_violation[d] = c.violation;
        for(const int64_t idx : c.onsite) sol.staff_onsite[static_cast<size_t>(idx)] += 1;
        for(size_t r = 0; r < c.violation.size(); ++r){
            sol.violation_sum[r] += c.violation[r];
        }
        sol.overrides += static_cast<int64_t>(c.overridden.size());
    }

    sol.fairness = fairness_penalty(sol.staff_onsite, metric);
    return sol;
}

// Deterministic simulated annealing over the per-day refinement pools. Each pool holds candidates that share the
// baseline's (lexicographically-optimal) violation vector, so coverage optimality is never sacrificed.
Solution local_search(const ParsedSchedule &schedule,
                      const std::vector<std::vector<DayCandidate>> &pools,
                      const size_t n_req,
                      const double w_fair,
                      const double w_pref,
                      const uint64_t seed,
                      const std::string &metric){
    const size_t n_days = schedule.days.size();

    // Start from the first candidate in each pool.
    std::vector<size_t> choice(n_days, 0);
    std::vector<int64_t> staff_onsite(schedule.staff.size(), 0);
    int64_t overrides = 0;
    for(size_t d = 0; d < n_days; ++d){
        if(pools[d].empty()) continue;
        for(const int64_t idx : pools[d][0].onsite) staff_onsite[static_cast<size_t>(idx)] += 1;
        overrides += static_cast<int64_t>(pools[d][0].overridden.size());
    }

    const auto objective = [&](const std::vector<int64_t> &so, int64_t ov) -> double {
        return w_fair * fairness_penalty(so, metric) + w_pref * static_cast<double>(ov);
    };

    double cur = objective(staff_onsite, overrides);
    double best = cur;
    auto best_choice = choice;

    // Collect the days that actually have a choice to make.
    std::vector<size_t> mutable_days;
    for(size_t d = 0; d < n_days; ++d){
        if(pools[d].size() > 1) mutable_days.push_back(d);
    }
    if(mutable_days.empty()){
        return build_solution(schedule, pools, best_choice, n_req, metric);
    }

    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> U(0.0, 1.0);
    std::uniform_int_distribution<size_t> day_dist(0, mutable_days.size() - 1);

    const int64_t iters = 100000;
    const double T0 = std::max(1.0, std::abs(cur) * 0.5);

    for(int64_t it = 0; it < iters; ++it){
        const double T = T0 * (1.0 - static_cast<double>(it) / static_cast<double>(iters));

        const size_t d = mutable_days[day_dist(rng)];
        const auto &pool = pools[d];
        if(pool.size() <= 1) continue;

        std::uniform_int_distribution<size_t> cand_dist(0, pool.size() - 1);
        size_t new_idx = cand_dist(rng);
        if(new_idx == choice[d]){
            new_idx = (new_idx + 1) % pool.size();
        }

        // Apply the tentative swap.
        const auto &old_c = pool[choice[d]];
        const auto &new_c = pool[new_idx];
        for(const int64_t idx : old_c.onsite) staff_onsite[static_cast<size_t>(idx)] -= 1;
        for(const int64_t idx : new_c.onsite) staff_onsite[static_cast<size_t>(idx)] += 1;
        overrides += static_cast<int64_t>(new_c.overridden.size()) - static_cast<int64_t>(old_c.overridden.size());

        const double cand = objective(staff_onsite, overrides);

        bool accept = (cand <= cur);
        if(!accept && T > 0.0){
            const double delta = cur - cand;
            const double prob = std::exp(delta / T);
            accept = (U(rng) < prob);
        }

        if(accept){
            cur = cand;
            choice[d] = new_idx;
        }else{
            // Revert.
            for(const int64_t idx : new_c.onsite) staff_onsite[static_cast<size_t>(idx)] -= 1;
            for(const int64_t idx : old_c.onsite) staff_onsite[static_cast<size_t>(idx)] += 1;
            overrides -= static_cast<int64_t>(new_c.overridden.size()) - static_cast<int64_t>(old_c.overridden.size());
        }

        if(cur < best){
            best = cur;
            best_choice = choice;
        }
    }

    return build_solution(schedule, pools, best_choice, n_req, metric);
}

} // anonymous namespace


// --------------------------------------------------------------------------------------------------------------------------
// Public: Phases B-D -- produce Pareto-spread variations.
// --------------------------------------------------------------------------------------------------------------------------

std::vector<Solution> produce_variations(const ParsedSchedule &schedule,
                                         const RequirementModel &model,
                                         const SolverConfig &config){
    const size_t n_days = schedule.days.size();

    // Phase A.
    std::vector<std::vector<DayCandidate>> day_candidates(n_days);
    for(size_t d = 0; d < n_days; ++d){
        day_candidates[d] = generate_day_candidates(schedule.days[d], model);
    }

    // Phase B.
    const auto baseline_choice = select_baseline(day_candidates);
    const Solution baseline = build_solution(schedule, day_candidates, baseline_choice, model.subsets.size(), config.fairness_metric);

    YLOGINFO("ScheduleCoverage: parsed " << schedule.days.size() << " days, " << schedule.staff.size()
             << " staff, " << schedule.requirements.size() << " requirements");

    // Build the per-day refinement pools: candidates that share the baseline's violation vector.
    std::vector<std::vector<DayCandidate>> pools(n_days);
    for(size_t d = 0; d < n_days; ++d){
        const auto &cs = day_candidates[d];
        if(cs.empty()) continue;
        const auto &base_v = cs[baseline_choice[d]].violation;
        for(const auto &c : cs){
            if(c.violation == base_v) pools[d].push_back(c);
        }
    }

    size_t total_candidates = 0;
    for(const auto &cs : day_candidates) total_candidates += cs.size();
    YLOGINFO("ScheduleCoverage: generated " << total_candidates << " per-day candidates; baseline requirement vector = ["
             << join(baseline.violation_sum, ",") << "]");

    // Phase C/D: sweep a small set of (fairness, preference) weight combinations and seeds, then keep the
    // Pareto-spread, de-duplicated results.
    const std::vector<std::pair<double,double>> weight_combos = {
        { config.fairness_weight, config.preference_weight },
        { 1.0, 0.0 },
        { 0.0, 1.0 },
        { config.fairness_weight, 0.0 },
        { 0.0, config.preference_weight },
    };

    const std::vector<uint64_t> seeds = {
        static_cast<uint64_t>(config.seed),
        static_cast<uint64_t>(config.seed) + 1,
        static_cast<uint64_t>(config.seed) + 2,
    };

    std::vector<Solution> candidates;
    candidates.push_back(baseline);

    for(const auto &wc : weight_combos){
        for(const uint64_t s : seeds){
            candidates.push_back(local_search(schedule, pools, model.subsets.size(), wc.first, wc.second, s, config.fairness_metric));
        }
    }

    // De-duplicate by canonical signature.
    std::vector<Solution> dedup;
    {
        std::set<std::string> seen;
        for(auto &sol : candidates){
            std::stringstream key;
            for(size_t d = 0; d < n_days; ++d){
                auto onsite = sol.day_onsite[d];
                std::sort(onsite.begin(), onsite.end());
                for(const int64_t idx : onsite) key << idx << ",";
                key << ";";
            }
            const std::string k = key.str();
            if(seen.count(k) == 0){
                seen.insert(k);
                dedup.push_back(std::move(sol));
            }
        }
    }

    // Project onto the (fairness, overrides) Pareto front. (All solutions share the same requirement objective.)
    std::vector<Solution> front;
    for(size_t i = 0; i < dedup.size(); ++i){
        bool dominated = false;
        for(size_t j = 0; j < dedup.size() && !dominated; ++j){
            if(i == j) continue;
            const auto &a = dedup[j];
            const auto &b = dedup[i];
            const bool leq = (a.fairness <= b.fairness) && (a.overrides <= b.overrides);
            const bool lt = (a.fairness < b.fairness) || (a.overrides < b.overrides);
            if(leq && lt) dominated = true;
        }
        if(!dominated) front.push_back(std::move(dedup[i]));
    }

    // Order by (fairness, overrides) for a stable, explainable output.
    std::sort(front.begin(), front.end(), [](const Solution &a, const Solution &b){
        if(a.fairness != b.fairness) return a.fairness < b.fairness;
        return a.overrides < b.overrides;
    });

    // If more solutions than requested, select a spread across the front.
    const int64_t n_out = std::max<int64_t>(1, config.n_variations);
    if(static_cast<int64_t>(front.size()) > n_out){
        std::vector<Solution> selected;
        selected.reserve(static_cast<size_t>(n_out));
        const size_t N = front.size();
        for(int64_t k = 0; k < n_out; ++k){
            const size_t idx = (N <= 1) ? 0
                             : static_cast<size_t>(std::llround(static_cast<double>(k) * static_cast<double>(N - 1)
                                                                / static_cast<double>(n_out - 1)));
            selected.push_back(front[idx]);
        }
        front.swap(selected);
    }

    return front;
}


// --------------------------------------------------------------------------------------------------------------------------
// Public: rendering.
// --------------------------------------------------------------------------------------------------------------------------

tables::table2 render_variation(const tables::table2 &original,
                                const ParsedSchedule &schedule,
                                const Solution &solution){
    tables::table2 out = original;

    // Overwrite the mutable cells per the solution. Immutable and holiday cells are preserved verbatim.
    for(size_t d = 0; d < schedule.days.size(); ++d){
        const auto &day = schedule.days[d];
        if(day.holiday) continue;
        if(day.row < 0) continue;

        std::set<int64_t> on(solution.day_onsite[d].begin(), solution.day_onsite[d].end());
        std::set<int64_t> ov(solution.day_overridden[d].begin(), solution.day_overridden[d].end());

        for(size_t s = 0; s < day.classes.size(); ++s){
            const auto cls = day.classes[s];
            if(cls == CellClass::Undecided){
                out.inject(day.row, schedule.staff_columns[s], (on.count(static_cast<int64_t>(s)) != 0) ? "onsite" : "remote");
            }else if(cls == CellClass::RemotePreference){
                if(ov.count(static_cast<int64_t>(s)) != 0){
                    out.inject(day.row, schedule.staff_columns[s], "onsite");
                }
                // Otherwise leave the original "Remote" cell untouched.
            }
            // Fixed on-site, fixed remote, and immutable cells are already correct.
        }
    }

    // Append the report block, leaving one blank row below the last schedule row.
    const int64_t rep_row = out.next_empty_row() + 1;
    int64_t r = rep_row;

    out.inject(r, 0, "== Schedule Report ==");
    ++r;

    // FLAG rows: one per day+requirement whose quota cannot be met.
    for(size_t d = 0; d < schedule.days.size(); ++d){
        const auto &day = schedule.days[d];
        if(day.holiday) continue;
        const auto &dv = solution.day_violation[d];
        for(size_t req = 0; req < dv.size(); ++req){
            if(dv[req] > 0){
                const std::string label = (req < schedule.requirements.size()) ? schedule.requirements[req].label
                                                                              : ("Requirement " + std::to_string(req + 1));
                out.inject(r, 0, "FLAG");
                out.inject(r, 1, day.date);
                out.inject(r, 2, label);
                out.inject(r, 3, "deficit=" + std::to_string(dv[req]));
                ++r;
            }
        }
    }

    // OVERRIDE rows: one per Remote -> on-site change.
    for(size_t d = 0; d < schedule.days.size(); ++d){
        const auto &day = schedule.days[d];
        for(const int64_t idx : solution.day_overridden[d]){
            out.inject(r, 0, "OVERRIDE");
            out.inject(r, 1, day.date);
            out.inject(r, 2, schedule.staff[static_cast<size_t>(idx)]);
            out.inject(r, 3, "Remote -> onsite");
            ++r;
        }
    }

    // TALLY rows: per-staff on-site/remote/vacation/other tallies.
    for(size_t s = 0; s < schedule.staff.size(); ++s){
        int64_t onsite = 0;
        int64_t remote = 0;
        int64_t vacation = 0;
        int64_t other = 0;

        for(size_t d = 0; d < schedule.days.size(); ++d){
            const auto &day = schedule.days[d];
            const auto cls = day.classes[s];
            const bool is_on = std::find(solution.day_onsite[d].begin(), solution.day_onsite[d].end(),
                                         static_cast<int64_t>(s)) != solution.day_onsite[d].end();
            const bool is_ov = std::find(solution.day_overridden[d].begin(), solution.day_overridden[d].end(),
                                         static_cast<int64_t>(s)) != solution.day_overridden[d].end();

            if(day.holiday){
                ++other;
            }else if(cls == CellClass::Onsite){
                ++onsite;
            }else if(cls == CellClass::Remote){
                ++remote;
            }else if(cls == CellClass::Undecided){
                if(is_on) ++onsite; else ++remote;
            }else if(cls == CellClass::RemotePreference){
                if(is_ov) ++onsite; else ++remote;
            }else{ // Immutable or Holiday marker on a mixed day.
                if(is_vacation_cell(day.cells[s])) ++vacation;
                else ++other;
            }
        }

        out.inject(r, 0, "TALLY");
        out.inject(r, 1, schedule.staff[s]);
        out.inject(r, 2, "onsite=" + std::to_string(onsite));
        out.inject(r, 3, "remote=" + std::to_string(remote));
        out.inject(r, 4, "vacation=" + std::to_string(vacation));
        out.inject(r, 5, "other=" + std::to_string(other));
        ++r;
    }

    // OBJECTIVES row.
    out.inject(r, 0, "OBJECTIVES");
    out.inject(r, 1, "violations=" + join(solution.violation_sum, ","));
    out.inject(r, 2, "fairness=" + format_double(solution.fairness));
    out.inject(r, 3, "overrides=" + std::to_string(solution.overrides));
    ++r;

    return out;
}

} // namespace ScheduleCoverageCore


// --------------------------------------------------------------------------------------------------------------------------
// Operation.
// --------------------------------------------------------------------------------------------------------------------------

OperationDoc OpArgDocScheduleCoverage(){
    OperationDoc out;
    out.name = "ScheduleCoverage";

    out.tags.emplace_back("category: table processing");
    out.tags.emplace_back("category: medical physics");

    out.desc =
        "This operation ingests a staff-rostering schedule held in a sparse table, classifies every cell, solves an"
        " optimization problem that fills in undecided (and overrideable) entries to satisfy a prioritized list of"
        " on-site coverage requirements, balances long-term fairness across staff, and emits several schedule"
        " variations near the Pareto front, each with an appended report for auditing."
        "\n\n"
        "The input table is expected to have two interleaved regions: (1) requirement rows near the top whose first"
        " column matches 'RequirementRegex' and whose second and third columns hold a requirement type and a quota"
        " expression, and (2) one or more schedule blocks, each beginning with a header row matching 'HeaderRegex'"
        " (whose columns name the staff) followed by date rows whose cells hold per-staff statuses."
        "\n\n"
        "Cell terms are classified into the categories below using user-overridable, comma-separated term lists"
        " (case-insensitive, exact by default; opt into regex matching via 'TermMatchMode'). A term matching no known"
        " category is treated as immutable/non-counting and a warning is logged."
        "\n\n"
        "Quota expressions are either 'any <N>', '<N>', 'any', or a staff OR-list like 'A OR B OR C' (which means at"
        " least one of the listed staff). Each requirement evaluates to: on a given day, the number of on-site staff"
        " within the requirement's subset must meet or exceed its minimum."
        "\n\n"
        "The solver first minimizes requirement deficits in lexicographic priority order, then balances fairness and"
        " honors remote preferences without worsening coverage. Output tables carry a 'ScheduleVariation' metadata"
        " key plus objective values, and each has an appended report block with 'FLAG', 'OVERRIDE', 'TALLY', and"
        " 'OBJECTIVES' rows (see the operation notes for the report format).";

    out.args.emplace_back();
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "RequirementRegex";
    out.args.back().desc = "A regular expression that identifies requirement-label cells in column 0.";
    out.args.back().default_val = "^Requirement";
    out.args.back().expected = true;
    out.args.back().examples = { "^Requirement", "^Req", "Requirement" };

    out.args.emplace_back();
    out.args.back().name = "HeaderRegex";
    out.args.back().desc = "A regular expression that identifies schedule header rows (whose columns name the staff).";
    out.args.back().default_val = "^Date$";
    out.args.back().expected = true;
    out.args.back().examples = { "^Date$", "^Week", "^Header" };

    out.args.emplace_back();
    out.args.back().name = "HolidayTerms";
    out.args.back().desc = "Comma-separated terms that mark a whole day as a holiday (skipped by the optimizer).";
    out.args.back().default_val = "Holiday";
    out.args.back().expected = true;
    out.args.back().examples = { "Holiday", "Holiday,Stat" };

    out.args.emplace_back();
    out.args.back().name = "ImmutableTerms";
    out.args.back().desc = "Comma-separated terms that are fixed and do not count toward on-site quotas (e.g.,"
                           " vacation, clinic time, primary/secondary roles).";
    out.args.back().default_val = "Vac,CTO,Prim,Sec";
    out.args.back().expected = true;
    out.args.back().examples = { "Vac,CTO,Prim,Sec", "Vac,Leave,Prim,Sec" };

    out.args.emplace_back();
    out.args.back().name = "OnsiteTerms";
    out.args.back().desc = "Comma-separated terms that are fixed on-site and are never changed.";
    out.args.back().default_val = "onsite";
    out.args.back().expected = true;
    out.args.back().examples = { "onsite", "Onsite,OnSite" };

    out.args.emplace_back();
    out.args.back().name = "RemotePreferenceTerms";
    out.args.back().desc = "Comma-separated terms that prefer remote work but may be overridden to on-site when"
                           " needed to satisfy coverage.";
    out.args.back().default_val = "Remote";
    out.args.back().expected = true;
    out.args.back().examples = { "Remote", "Remote,Wfh" };

    out.args.emplace_back();
    out.args.back().name = "UndecidedTerms";
    out.args.back().desc = "Comma-separated terms that are undecided and must be assigned either on-site or remote.";
    out.args.back().default_val = "x";
    out.args.back().expected = true;
    out.args.back().examples = { "x", "x,TBD,?" };

    out.args.emplace_back();
    out.args.back().name = "RemoteTerms";
    out.args.back().desc = "Comma-separated terms that are fixed remote and are never changed.";
    out.args.back().default_val = "remote";
    out.args.back().expected = true;
    out.args.back().examples = { "remote", "remote,home" };

    out.args.emplace_back();
    out.args.back().name = "TermMatchMode";
    out.args.back().desc = "Controls whether the term lists are matched exactly or as (case-insensitive) regular"
                           " expressions.";
    out.args.back().default_val = "exact";
    out.args.back().expected = true;
    out.args.back().examples = { "exact", "regex" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "NVariations";
    out.args.back().desc = "The number of schedule variations to emit (spread across the Pareto front).";
    out.args.back().default_val = "3";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "3", "5" };

    out.args.emplace_back();
    out.args.back().name = "FairnessMetric";
    out.args.back().desc = "The fairness metric used to measure imbalance of the total assigned on-site days across"
                           " staff: 'range' (max minus min), 'variance' (population variance), or 'gini' (Gini"
                           " coefficient).";
    out.args.back().default_val = "range";
    out.args.back().expected = true;
    out.args.back().examples = { "range", "variance", "gini" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "PreferenceWeight";
    out.args.back().desc = "Weight on minimizing remote-preference overrides (a secondary objective).";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0", "1.0", "2.5" };

    out.args.emplace_back();
    out.args.back().name = "FairnessWeight";
    out.args.back().desc = "Weight on minimizing the fairness metric (a secondary objective).";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0", "1.0", "2.5" };

    out.args.emplace_back();
    out.args.back().name = "Seed";
    out.args.back().desc = "Seed for the deterministic local-search RNG. With a fixed seed, repeated runs produce"
                           " identical output.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "12345" };

    out.notes.emplace_back(
        "Report format (appended below the last schedule row, using reserved leading tokens in column 0):"
        " a 'FLAG' row is emitted for every day+requirement whose quota cannot be met; an 'OVERRIDE' row is emitted"
        " for every Remote -> on-site change; a 'TALLY' row reports each staff member's on-site, remote, vacation,"
        " and other day tallies; and an 'OBJECTIVES' row records the total requirement violations, the fairness"
        " metric value, and the override count.");
    out.notes.emplace_back(
        "The requirement objective of every emitted variation is guaranteed to equal the lexicographic optimum of the"
        " per-day requirement deficits; variations differ only in the fairness/override secondary objectives.");

    return out;
}

bool ScheduleCoverage(Drover &DICOM_data,
                      const OperationArgPkg& OptArgs,
                      std::map<std::string, std::string>& /*InvocationMetadata*/,
                      const std::string& FilenameLex){
    Explicator X(FilenameLex);
    using namespace ScheduleCoverageCore;

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TableSelectionStr = OptArgs.getValueStr("TableSelection").value();
    const auto RequirementRegexStr = OptArgs.getValueStr("RequirementRegex").value();
    const auto HeaderRegexStr = OptArgs.getValueStr("HeaderRegex").value();
    const auto TermMatchModeStr = OptArgs.getValueStr("TermMatchMode").value();
    const auto FairnessMetricStr = OptArgs.getValueStr("FairnessMetric").value();

    const auto NVariationsStr = OptArgs.getValueStr("NVariations").value();
    const auto PreferenceWeightStr = OptArgs.getValueStr("PreferenceWeight").value();
    const auto FairnessWeightStr = OptArgs.getValueStr("FairnessWeight").value();
    const auto SeedStr = OptArgs.getValueStr("Seed").value();

    TermLists terms;
    terms.holiday     = SplitStringToVector(OptArgs.getValueStr("HolidayTerms").value(), ',', 'd');
    terms.immutable   = SplitStringToVector(OptArgs.getValueStr("ImmutableTerms").value(), ',', 'd');
    terms.onsite      = SplitStringToVector(OptArgs.getValueStr("OnsiteTerms").value(), ',', 'd');
    terms.remote_pref = SplitStringToVector(OptArgs.getValueStr("RemotePreferenceTerms").value(), ',', 'd');
    terms.undecided   = SplitStringToVector(OptArgs.getValueStr("UndecidedTerms").value(), ',', 'd');
    terms.remote      = SplitStringToVector(OptArgs.getValueStr("RemoteTerms").value(), ',', 'd');

    const auto re_exact = Compile_Regex("^ex?a?c?t?$");
    const auto re_regex = Compile_Regex("^re?g?e?x?$");
    if(std::regex_match(TermMatchModeStr, re_exact)){
        terms.regex_mode = false;
    }else if(std::regex_match(TermMatchModeStr, re_regex)){
        terms.regex_mode = true;
    }else{
        throw std::runtime_error("TermMatchMode argument not understood");
    }

    SolverConfig config;
    config.fairness_metric = FairnessMetricStr;
    config.n_variations = std::stoll(NVariationsStr);
    config.preference_weight = std::stod(PreferenceWeightStr);
    config.fairness_weight = std::stod(FairnessWeightStr);
    config.seed = std::stoll(SeedStr);

    if(config.n_variations < 1){
        throw std::runtime_error("NVariations must be at least 1");
    }
    if(config.preference_weight < 0.0 || config.fairness_weight < 0.0){
        throw std::runtime_error("PreferenceWeight and FairnessWeight must be non-negative");
    }
    if(!(config.fairness_metric == "range" || config.fairness_metric == "variance" || config.fairness_metric == "gini")){
        throw std::runtime_error("FairnessMetric argument not understood");
    }

    //-----------------------------------------------------------------------------------------------------------------
    auto STs_all = All_STs(DICOM_data);
    auto STs = Whitelist(STs_all, TableSelectionStr);
    if(STs.empty()){
        throw std::runtime_error("No table matched the TableSelection");
    }

    for(auto &stp_it : STs){
        tables::table2 &t = (*stp_it)->table;
        if(t.data.empty()){
            YLOGWARN("Selected table is empty; skipping");
            continue;
        }

        const auto schedule = ScheduleCoverageCore::parse_schedule(t, RequirementRegexStr, HeaderRegexStr, terms);
        const auto model = ScheduleCoverageCore::build_requirement_model(schedule);
        const auto solutions = ScheduleCoverageCore::produce_variations(schedule, model, config);

        const std::string base_label = (t.metadata.count("TableLabel") != 0) ? t.metadata["TableLabel"] : "unspecified";

        for(size_t i = 0; i < solutions.size(); ++i){
            tables::table2 out = ScheduleCoverageCore::render_variation(t, schedule, solutions[i]);

            // Coalesce metadata for a fresh table, then stamp variation-specific keys.
            out.metadata = coalesce_metadata_for_basic_table(out.metadata, meta_evolve::iterate);

            const std::string suffix = " [schedule coverage variation " + std::to_string(i + 1) + "]";
            const std::string label = base_label + suffix;
            out.metadata["TableLabel"] = label;
            out.metadata["NormalizedTableLabel"] = X(label);
            out.metadata["ScheduleVariation"] = std::to_string(i + 1);
            out.metadata["ScheduleCoverageFairness"] = format_double(solutions[i].fairness);
            out.metadata["ScheduleCoverageOverrides"] = std::to_string(solutions[i].overrides);
            out.metadata["ScheduleCoverageViolations"] = join(solutions[i].violation_sum, ",");

            auto st = std::make_shared<Sparse_Table>();
            st->table = std::move(out);
            DICOM_data.table_data.emplace_back(std::move(st));
        }

        YLOGINFO("ScheduleCoverage: emitted " << solutions.size() << " schedule variation(s)");
    }

    return true;
}
