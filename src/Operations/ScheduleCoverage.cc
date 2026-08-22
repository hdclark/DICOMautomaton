//ScheduleCoverage.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
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

std::string normalize_identifier(const std::string &s){
    std::string out;
    for(const char c : s){
        if(std::isalnum(static_cast<unsigned char>(c))){
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
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

std::string join_strings(const std::vector<std::string> &v, const std::string &sep){
    std::stringstream ss;
    for(size_t i = 0; i < v.size(); ++i){
        if(i != 0) ss << sep;
        ss << v[i];
    }
    return ss.str();
}

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
    ss.precision(8);
    ss << d;
    return ss.str();
}

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
        }else if(to_lower(cell_trimmed) == to_lower(term)){
            return true;
        }
    }
    return false;
}

bool is_all_digits(const std::string &s){
    if(s.empty()) return false;
    for(const char c : s){
        if(!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

bool label_is_hard(const std::string &label){
    const auto n = normalize_identifier(label);
    return n.rfind("hardconstraint", 0) == 0 || n.rfind("requirement", 0) == 0;
}

bool label_is_soft(const std::string &label){
    const auto n = normalize_identifier(label);
    return n.rfind("softconstraint", 0) == 0;
}

bool parse_weight(const std::string &raw, double &weight){
    static const std::regex re(
        "^\\s*[Ww][Ee][Ii][Gg][Hh][Tt]\\s*=\\s*([+]?([0-9]+(\\.[0-9]*)?|\\.[0-9]+)([eE][+-]?[0-9]+)?)\\s*$",
        std::regex::ECMAScript);
    std::smatch m;
    if(!std::regex_match(raw, m, re)) return false;
    try{
        weight = std::stod(m[1].str());
    }catch(const std::exception &){
        return false;
    }
    return std::isfinite(weight) && weight >= 0.0;
}

bool is_max_consecutive_remote_type(const std::string &type){
    const auto n = normalize_identifier(type);
    return n == "maxconsecutiveremote" || n == "maxconsecutiveremotedays" || n == "consecutiveremote";
}

bool is_exclusivity_type(const std::string &type){
    const auto n = normalize_identifier(type);
    return n == "exclusivity" || n == "mutualexclusion" || n == "exclusive";
}

bool is_max_weekly_remote_type(const std::string &type){
    const auto n = normalize_identifier(type);
    return n == "maxweeklyremote" || n == "maximumweeklyremote";
}

std::vector<std::string> parse_xor_staff(const std::string &expr){
    const std::regex re("\\s+[Xx][Oo][Rr]\\s+", std::regex::ECMAScript);
    std::sregex_token_iterator it(expr.begin(), expr.end(), re, -1);
    const std::sregex_token_iterator end;
    std::vector<std::string> out;
    for(; it != end; ++it){
        const auto s = trim(it->str());
        if(!s.empty()) out.push_back(s);
    }
    if(out.size() < 2) return {};
    return out;
}

bool parse_staff_limit(const std::string &expr, std::string &staff, int64_t &limit){
    static const std::regex re("^\\s*([^=\\s]+)\\s*=\\s*([0-9]+)\\s*$", std::regex::ECMAScript);
    std::smatch m;
    if(!std::regex_match(expr, m, re)) return false;
    staff = trim(m[1].str());
    try{
        limit = std::stoll(m[2].str());
    }catch(const std::exception &){
        return false;
    }
    return !staff.empty();
}

bool contains_index(const std::vector<int64_t> &v, const int64_t idx){
    return std::find(v.begin(), v.end(), idx) != v.end();
}

bool immutable_role_counts_present(const Day &day, const size_t staff_index){
    if(staff_index >= day.cells.size()) return false;
    const auto raw = normalize_identifier(day.cells[staff_index]);
    return raw == "prim" || raw == "sec";
}

bool is_remote_workday(const Day &day,
                       const size_t staff_index,
                       const std::vector<int64_t> &onsite){
    if(day.holiday || staff_index >= day.classes.size()) return false;
    const auto cls = day.classes[staff_index];
    if(cls == CellClass::Holiday || cls == CellClass::Vacation) return false;
    const bool is_on = contains_index(onsite, static_cast<int64_t>(staff_index));
    return (cls == CellClass::Remote)
        || ((cls == CellClass::RemotePreference || cls == CellClass::Undecided) && !is_on);
}

std::string hard_display_name(const Requirement &req){
    const auto type = trim(req.type);
    if(type.empty()) return req.label;
    return "hard constraint '" + type + "' (" + req.label + ")";
}

std::string soft_display_name(const SoftConstraint &req){
    const auto type = trim(req.type);
    if(type.empty()) return req.label;
    return "soft constraint '" + type + "' (" + req.label + ")";
}

int64_t sum_ints(const std::vector<int64_t> &v){
    int64_t out = 0;
    for(const auto x : v) out += x;
    return out;
}

std::vector<std::string> soft_constraint_messages(const ParsedSchedule &schedule,
                                                  const ConstraintModel &model,
                                                  const std::vector<std::vector<int64_t>> &day_onsite,
                                                  const size_t soft_index){
    std::vector<std::string> messages;
    if(soft_index >= model.soft.size() || soft_index >= schedule.soft_constraints.size()) return messages;
    const auto &sc = model.soft[soft_index];

    if(sc.kind == SoftConstraintKind::MaxConsecutiveRemote){
        for(size_t s = 0; s < schedule.staff.size(); ++s){
            int64_t run = 0;
            std::string first_date;
            std::string last_date;
            const auto flush = [&](){
                if(run > sc.limit){
                    messages.push_back(schedule.staff[s] + " has a remote-workday run of " + std::to_string(run)
                        + " from '" + first_date + "' through '" + last_date + "'; maximum="
                        + std::to_string(sc.limit) + ", excess=" + std::to_string(run - sc.limit));
                }
                run = 0;
                first_date.clear();
                last_date.clear();
            };
            for(size_t d = 0; d < schedule.days.size(); ++d){
                const auto &day = schedule.days[d];
                const auto cls = day.classes[s];
                if(day.holiday || cls == CellClass::Holiday || cls == CellClass::Vacation) continue;
                if(is_remote_workday(day, s, day_onsite[d])){
                    if(run == 0) first_date = day.date;
                    last_date = day.date;
                    ++run;
                }else{
                    flush();
                }
            }
            flush();
        }
    }else if(sc.kind == SoftConstraintKind::Exclusivity){
        for(size_t d = 0; d < schedule.days.size(); ++d){
            const auto &day = schedule.days[d];
            if(day.holiday) continue;
            std::vector<std::string> present;
            for(const int64_t idx : sc.staff){
                const size_t s = static_cast<size_t>(idx);
                if(contains_index(day_onsite[d], idx) || immutable_role_counts_present(day, s)){
                    present.push_back(schedule.staff[s]);
                }
            }
            if(present.size() > 1){
                messages.push_back("on '" + day.date + "', mutually exclusive staff " + join_strings(present, ", ")
                    + " are simultaneously present (onsite, Prim, or Sec); excess="
                    + std::to_string(static_cast<int64_t>(present.size()) - 1));
            }
        }
    }else if(sc.kind == SoftConstraintKind::MaxWeeklyRemote){
        if(sc.staff.empty()) return messages;
        const size_t s = static_cast<size_t>(sc.staff.front());
        std::map<int64_t, int64_t> counts;
        for(size_t d = 0; d < schedule.days.size(); ++d){
            if(is_remote_workday(schedule.days[d], s, day_onsite[d])){
                ++counts[schedule.days[d].week_index];
            }
        }
        for(const auto &kv : counts){
            if(kv.second > sc.limit){
                messages.push_back(schedule.staff[s] + " has " + std::to_string(kv.second)
                    + " remote workdays in week block " + std::to_string(kv.first + 1)
                    + "; maximum=" + std::to_string(sc.limit)
                    + ", excess=" + std::to_string(kv.second - sc.limit));
            }
        }
    }

    return messages;
}

} // anonymous namespace

CellClass classify_cell(const std::string &raw,
                        const TermLists &terms,
                        bool *matched_known){
    if(matched_known) *matched_known = false;
    const std::string cell = trim(raw);
    if(cell.empty()) return CellClass::Immutable;

    const auto mark = [&](bool b) -> bool {
        if(matched_known && b) *matched_known = true;
        return b;
    };

    if(mark(term_list_matches(cell, terms.holiday, terms.regex_mode)))     return CellClass::Holiday;
    if(mark(term_list_matches(cell, terms.vacation, terms.regex_mode)))    return CellClass::Vacation;
    if(mark(term_list_matches(cell, terms.onsite, terms.regex_mode)))      return CellClass::Onsite;
    if(mark(term_list_matches(cell, terms.remote_pref, terms.regex_mode))) return CellClass::RemotePreference;
    if(mark(term_list_matches(cell, terms.undecided, terms.regex_mode)))   return CellClass::Undecided;
    if(mark(term_list_matches(cell, terms.remote, terms.regex_mode)))      return CellClass::Remote;
    if(mark(term_list_matches(cell, terms.immutable, terms.regex_mode)))   return CellClass::Immutable;
    return CellClass::Immutable;
}

bool parse_quota(const std::string &quota,
                 int64_t &min_onsite,
                 std::vector<std::string> &subset){
    subset.clear();
    min_onsite = 1;
    const std::string s = trim(quota);
    if(s.empty()) return false;
    const auto tokens = split_ws(s);

    bool has_or = false;
    for(const auto &t : tokens) if(to_upper(t) == "OR") has_or = true;
    if(has_or){
        std::vector<std::string> parts;
        for(const auto &t : tokens) if(to_upper(t) != "OR") parts.push_back(t);
        size_t start = 0;
        if(parts.size() >= 3 && to_lower(parts[0]) == "any" && is_all_digits(parts[1]) && to_lower(parts[2]) == "of"){
            min_onsite = std::stoll(parts[1]);
            start = 3;
        }
        if(start >= parts.size()) return false;
        subset.assign(parts.begin() + static_cast<std::ptrdiff_t>(start), parts.end());
        return !subset.empty();
    }
    if(tokens.size() == 1 && to_lower(tokens[0]) == "any"){
        min_onsite = 1;
        return true;
    }
    if(tokens.size() == 2 && to_lower(tokens[0]) == "any" && is_all_digits(tokens[1])){
        min_onsite = std::stoll(tokens[1]);
        return true;
    }
    if(tokens.size() == 1 && is_all_digits(tokens[0])){
        min_onsite = std::stoll(tokens[0]);
        return true;
    }
    return false;
}

ParsedSchedule parse_schedule(const tables::table2 &table,
                              const std::string &constraint_regex,
                              const std::string &header_regex,
                              const TermLists &terms){
    ParsedSchedule out;
    if(table.data.empty()) throw std::runtime_error("The selected table is empty; there is no schedule to process");

    const auto re_constraint = Compile_Regex(constraint_regex);
    const auto re_hdr = Compile_Regex(header_regex);
    const auto [min_row, max_row] = table.min_max_row();
    const auto [min_col, max_col] = table.min_max_col();

    bool in_schedule = false;
    int64_t current_week = -1;
    std::vector<int64_t> staff_columns;

    for(int64_t r = min_row; r <= max_row; ++r){
        const auto first_cell_opt = table.value(r, min_col);

        if(!in_schedule){
            if(first_cell_opt && std::regex_search(first_cell_opt.value(), re_constraint)){
                const std::string label = first_cell_opt.value();
                const std::string type = table.value(r, min_col + 1).value_or("");
                const std::string expr = table.value(r, min_col + 2).value_or("");

                if(label_is_hard(label)){
                    Requirement req;
                    req.label = label;
                    req.type = type;
                    req.quota_raw = expr;
                    if(!parse_quota(req.quota_raw, req.min_onsite, req.subset)){
                        throw std::runtime_error("Unable to parse quota expression '" + req.quota_raw
                            + "' for hard constraint '" + req.label + "'");
                    }
                    out.requirements.push_back(std::move(req));
                    continue;
                }

                if(label_is_soft(label)){
                    SoftConstraint sc;
                    sc.label = label;
                    sc.type = type;
                    sc.expression_raw = expr;
                    const std::string weight_raw = table.value(r, min_col + 3).value_or("");
                    if(!parse_weight(weight_raw, sc.weight)){
                        throw std::runtime_error("Soft constraint '" + sc.label
                            + "' requires column 3 in the form Weight=<non-negative number>, but found '"
                            + weight_raw + "'");
                    }

                    if(is_max_consecutive_remote_type(sc.type)){
                        const auto raw_limit = trim(sc.expression_raw);
                        if(!is_all_digits(raw_limit)){
                            throw std::runtime_error("Soft constraint '" + sc.label + "' of type '" + sc.type
                                + "' requires a non-negative integer maximum, but found '" + sc.expression_raw + "'");
                        }
                        sc.kind = SoftConstraintKind::MaxConsecutiveRemote;
                        sc.limit = std::stoll(raw_limit);
                    }else if(is_exclusivity_type(sc.type)){
                        sc.kind = SoftConstraintKind::Exclusivity;
                        sc.staff = parse_xor_staff(sc.expression_raw);
                        if(sc.staff.size() < 2){
                            throw std::runtime_error("Soft constraint '" + sc.label
                                + "' requires an expression such as 'XA XOR XB'");
                        }
                    }else if(is_max_weekly_remote_type(sc.type)){
                        sc.kind = SoftConstraintKind::MaxWeeklyRemote;
                        std::string staff;
                        if(!parse_staff_limit(sc.expression_raw, staff, sc.limit)){
                            throw std::runtime_error("Soft constraint '" + sc.label
                                + "' requires an expression such as 'XG = 2'");
                        }
                        sc.staff = { staff };
                    }else{
                        throw std::runtime_error("Soft constraint type '" + sc.type + "' in '" + sc.label
                            + "' is not supported");
                    }
                    out.soft_constraints.push_back(std::move(sc));
                    continue;
                }

                throw std::runtime_error("Constraint label '" + label
                    + "' matched RequirementRegex but is neither a Hard Constraint nor a Soft Constraint");
            }

            if(first_cell_opt && std::regex_search(first_cell_opt.value(), re_hdr)){
                for(int64_t c = min_col + 1; c <= max_col; ++c){
                    const auto v_opt = table.value(r, c);
                    if(v_opt && !trim(v_opt.value()).empty()){
                        out.staff.push_back(v_opt.value());
                        staff_columns.push_back(c);
                    }
                }
                if(out.staff.empty()) throw std::runtime_error("A schedule header row was found, but no staff columns were identified");
                out.staff_columns = staff_columns;
                current_week = 0;
                in_schedule = true;
                continue;
            }
            continue;
        }

        if(first_cell_opt && std::regex_search(first_cell_opt.value(), re_hdr)){
            std::vector<std::string> header_staff;
            for(int64_t c = min_col + 1; c <= max_col; ++c){
                const auto v_opt = table.value(r, c);
                if(v_opt && !trim(v_opt.value()).empty()) header_staff.push_back(v_opt.value());
            }
            if(header_staff != out.staff){
                YLOGWARN("Repeated header row differs from the first schedule header; continuing with the first header mapping");
            }
            ++current_week;
            continue;
        }

        if(!first_cell_opt) continue;

        Day day;
        day.row = r;
        day.date = first_cell_opt.value();
        day.week_index = current_week;
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

        day.holiday = std::all_of(day.classes.begin(), day.classes.end(),
            [](CellClass c){ return c == CellClass::Holiday; });
        if(!day.holiday){
            const bool any_holiday = std::any_of(day.classes.begin(), day.classes.end(),
                [](CellClass c){ return c == CellClass::Holiday; });
            if(any_holiday){
                YLOGWARN("Day '" << day.date << "' mixes holiday and non-holiday cells; holiday cells will be non-counting");
            }
        }
        out.days.push_back(std::move(day));
    }

    if(!in_schedule) throw std::runtime_error("No schedule header row matching '" + header_regex + "' was found");
    if(out.days.empty()) throw std::runtime_error("No schedule date rows were found after the header");
    return out;
}

ConstraintModel build_constraint_model(const ParsedSchedule &schedule){
    ConstraintModel model;
    std::map<std::string, int64_t> name_to_idx;
    for(size_t i = 0; i < schedule.staff.size(); ++i) name_to_idx[schedule.staff[i]] = static_cast<int64_t>(i);

    for(const auto &req : schedule.requirements){
        std::vector<int64_t> subset;
        if(req.subset.empty()){
            for(size_t i = 0; i < schedule.staff.size(); ++i) subset.push_back(static_cast<int64_t>(i));
        }else{
            for(const auto &nm : req.subset){
                const auto it = name_to_idx.find(nm);
                if(it == name_to_idx.end()){
                    throw std::runtime_error("Hard constraint '" + req.label + "' references staff '" + nm
                        + "' which is not present in the schedule header");
                }
                subset.push_back(it->second);
            }
        }
        model.hard.subsets.push_back(std::move(subset));
        model.hard.min_onsite.push_back(req.min_onsite);
    }

    for(const auto &raw : schedule.soft_constraints){
        SoftConstraintModel sc;
        sc.label = raw.label;
        sc.type = raw.type;
        sc.weight = raw.weight;
        sc.kind = raw.kind;
        sc.limit = raw.limit;
        for(const auto &nm : raw.staff){
            const auto it = name_to_idx.find(nm);
            if(it == name_to_idx.end()){
                throw std::runtime_error("Soft constraint '" + raw.label + "' references staff '" + nm
                    + "' which is not present in the schedule header");
            }
            sc.staff.push_back(it->second);
        }
        model.soft.push_back(std::move(sc));
    }

    return model;
}

std::vector<int64_t> evaluate_violation(const std::vector<int64_t> &onsite,
                                        const RequirementModel &model){
    std::vector<int64_t> v(model.subsets.size(), 0);
    std::set<int64_t> on(onsite.begin(), onsite.end());
    for(size_t r = 0; r < model.subsets.size(); ++r){
        int64_t cnt = 0;
        for(const int64_t idx : model.subsets[r]) if(on.count(idx) != 0) ++cnt;
        v[r] = std::max<int64_t>(0, model.min_onsite[r] - cnt);
    }
    return v;
}

std::vector<int64_t> evaluate_soft_penalties(const ParsedSchedule &schedule,
                                             const ConstraintModel &model,
                                             const std::vector<std::vector<int64_t>> &day_onsite){
    if(day_onsite.size() < schedule.days.size()){
        throw std::runtime_error("Soft-constraint evaluation received fewer day assignments than the parsed schedule");
    }
    std::vector<int64_t> penalties(model.soft.size(), 0);

    for(size_t i = 0; i < model.soft.size(); ++i){
        const auto &sc = model.soft[i];
        if(sc.kind == SoftConstraintKind::MaxConsecutiveRemote){
            if(sc.limit < 0) continue;
            for(size_t s = 0; s < schedule.staff.size(); ++s){
                int64_t run = 0;
                const auto flush = [&](){
                    if(run > sc.limit) penalties[i] += run - sc.limit;
                    run = 0;
                };
                for(size_t d = 0; d < schedule.days.size(); ++d){
                    const auto &day = schedule.days[d];
                    const auto cls = day.classes[s];
                    if(day.holiday || cls == CellClass::Holiday || cls == CellClass::Vacation) continue;
                    if(is_remote_workday(day, s, day_onsite[d])) ++run;
                    else flush();
                }
                flush();
            }
        }else if(sc.kind == SoftConstraintKind::Exclusivity){
            for(size_t d = 0; d < schedule.days.size(); ++d){
                const auto &day = schedule.days[d];
                if(day.holiday) continue;
                int64_t present = 0;
                for(const int64_t idx : sc.staff){
                    const size_t s = static_cast<size_t>(idx);
                    if(contains_index(day_onsite[d], idx) || immutable_role_counts_present(day, s)) ++present;
                }
                if(present > 1) penalties[i] += present - 1;
            }
        }else if(sc.kind == SoftConstraintKind::MaxWeeklyRemote){
            if(sc.staff.empty()) continue;
            const size_t s = static_cast<size_t>(sc.staff.front());
            std::map<int64_t, int64_t> counts;
            for(size_t d = 0; d < schedule.days.size(); ++d){
                if(is_remote_workday(schedule.days[d], s, day_onsite[d])) ++counts[schedule.days[d].week_index];
            }
            for(const auto &kv : counts) if(kv.second > sc.limit) penalties[i] += kv.second - sc.limit;
        }
    }
    return penalties;
}

std::vector<DayCandidate> generate_day_candidates(const Day &day,
                                                  const RequirementModel &model){
    std::vector<DayCandidate> out;
    if(day.holiday) return out;

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
    if(M >= 63) throw std::runtime_error("A day has too many mutable cells to enumerate safely");
    const uint64_t total = (uint64_t{1} << M);
    out.reserve(static_cast<size_t>(total));
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
    return out;
}

std::vector<size_t> select_baseline(const std::vector<std::vector<DayCandidate>> &day_candidates){
    std::vector<size_t> choice(day_candidates.size(), 0);
    const auto lex_less = [](const DayCandidate &a, const DayCandidate &b){
        const size_t n = std::min(a.violation.size(), b.violation.size());
        for(size_t k = 0; k < n; ++k){
            if(a.violation[k] != b.violation[k]) return a.violation[k] < b.violation[k];
        }
        if(a.violation.size() != b.violation.size()) return a.violation.size() < b.violation.size();
        return a.overridden.size() < b.overridden.size();
    };
    for(size_t d = 0; d < day_candidates.size(); ++d){
        const auto &cs = day_candidates[d];
        if(cs.empty()) continue;
        size_t best = 0;
        for(size_t i = 1; i < cs.size(); ++i) if(lex_less(cs[i], cs[best])) best = i;
        choice[d] = best;
    }
    return choice;
}

double fairness_penalty(const std::vector<int64_t> &staff_onsite,
                        const std::string &metric){
    if(staff_onsite.empty()) return 0.0;
    if(metric == "range"){
        const auto mm = std::minmax_element(staff_onsite.begin(), staff_onsite.end());
        return static_cast<double>(*mm.second - *mm.first);
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
        for(const auto v : sorted) sum += v;
        if(sum == 0) return 0.0;
        int64_t numerator = 0;
        for(size_t i = 0; i < sorted.size(); ++i){
            for(size_t j = i + 1; j < sorted.size(); ++j) numerator += sorted[j] - sorted[i];
        }
        return static_cast<double>(numerator)
             / (static_cast<double>(sorted.size()) * static_cast<double>(sum));
    }
    throw std::runtime_error("Fairness metric '" + metric + "' not understood");
}

namespace {

Solution build_solution(const ParsedSchedule &schedule,
                        const ConstraintModel &model,
                        const std::vector<std::vector<DayCandidate>> &day_candidates,
                        const std::vector<size_t> &choice,
                        const SolverConfig &config){
    Solution sol;
    const size_t n_days = schedule.days.size();
    sol.day_onsite.resize(n_days);
    sol.day_overridden.resize(n_days);
    sol.day_violation.resize(n_days);
    sol.violation_sum.assign(model.hard.subsets.size(), 0);
    sol.staff_onsite.assign(schedule.staff.size(), 0);

    for(size_t d = 0; d < n_days; ++d){
        const auto &cs = day_candidates[d];
        if(cs.empty()) continue;
        const size_t idx = std::min(choice[d], cs.size() - 1);
        const auto &cand = cs[idx];
        sol.day_onsite[d] = cand.onsite;
        sol.day_overridden[d] = cand.overridden;
        sol.day_violation[d] = cand.violation;
        for(const int64_t s : cand.onsite) ++sol.staff_onsite[static_cast<size_t>(s)];
        for(size_t r = 0; r < cand.violation.size(); ++r) sol.violation_sum[r] += cand.violation[r];
        sol.overrides += static_cast<int64_t>(cand.overridden.size());
    }

    sol.hard_violation_units = sum_ints(sol.violation_sum);
    sol.fairness = fairness_penalty(sol.staff_onsite, config.fairness_metric);
    sol.soft_penalty = evaluate_soft_penalties(schedule, model, sol.day_onsite);
    for(size_t i = 0; i < sol.soft_penalty.size(); ++i){
        sol.soft_constraint_cost += model.soft[i].weight * static_cast<double>(sol.soft_penalty[i]);
    }
    sol.annealing_cost = config.requirement_violation_weight * static_cast<double>(sol.hard_violation_units)
                       + config.fairness_weight * sol.fairness
                       + config.preference_weight * static_cast<double>(sol.overrides)
                       + sol.soft_constraint_cost;
    return sol;
}

double search_cost(const Solution &sol,
                   const SolverConfig &config,
                   const double fairness_weight,
                   const double preference_weight){
    return config.requirement_violation_weight * static_cast<double>(sol.hard_violation_units)
         + fairness_weight * sol.fairness
         + preference_weight * static_cast<double>(sol.overrides)
         + sol.soft_constraint_cost;
}

Solution local_search(const ParsedSchedule &schedule,
                      const ConstraintModel &model,
                      const std::vector<std::vector<DayCandidate>> &day_candidates,
                      const std::vector<size_t> &initial_choice,
                      const SolverConfig &config,
                      const double fairness_weight,
                      const double preference_weight,
                      const uint64_t seed){
    std::vector<size_t> choice = initial_choice;
    Solution current = build_solution(schedule, model, day_candidates, choice, config);
    double current_cost = search_cost(current, config, fairness_weight, preference_weight);
    Solution best = current;
    double best_cost = current_cost;

    std::vector<size_t> mutable_days;
    for(size_t d = 0; d < day_candidates.size(); ++d) if(day_candidates[d].size() > 1) mutable_days.push_back(d);
    if(mutable_days.empty() || config.annealing_iterations <= 0) return best;

    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> U(0.0, 1.0);
    std::uniform_int_distribution<size_t> day_dist(0, mutable_days.size() - 1);
    const double T0 = std::max(1.0, std::abs(current_cost) * 0.25);

    for(int64_t it = 0; it < config.annealing_iterations; ++it){
        const size_t d = mutable_days[day_dist(rng)];
        const auto &pool = day_candidates[d];
        std::uniform_int_distribution<size_t> cand_dist(0, pool.size() - 1);
        const size_t old_idx = choice[d];
        size_t new_idx = cand_dist(rng);
        if(new_idx == old_idx) new_idx = (new_idx + 1) % pool.size();
        choice[d] = new_idx;

        Solution candidate = build_solution(schedule, model, day_candidates, choice, config);
        const double candidate_cost = search_cost(candidate, config, fairness_weight, preference_weight);
        const double fraction = static_cast<double>(it + 1) / static_cast<double>(config.annealing_iterations);
        const double T = std::max(1e-9, T0 * (1.0 - fraction));
        bool accept = candidate_cost <= current_cost;
        if(!accept){
            const double p = std::exp((current_cost - candidate_cost) / T);
            accept = U(rng) < p;
        }
        if(accept){
            current = std::move(candidate);
            current_cost = candidate_cost;
            if(current_cost < best_cost){
                best = current;
                best_cost = current_cost;
            }
        }else{
            choice[d] = old_idx;
        }
    }
    return best;
}

bool dominates(const Solution &a, const Solution &b){
    bool strict = false;
    if(a.violation_sum.size() != b.violation_sum.size() || a.soft_penalty.size() != b.soft_penalty.size()) return false;
    for(size_t i = 0; i < a.violation_sum.size(); ++i){
        if(a.violation_sum[i] > b.violation_sum[i]) return false;
        if(a.violation_sum[i] < b.violation_sum[i]) strict = true;
    }
    for(size_t i = 0; i < a.soft_penalty.size(); ++i){
        if(a.soft_penalty[i] > b.soft_penalty[i]) return false;
        if(a.soft_penalty[i] < b.soft_penalty[i]) strict = true;
    }
    if(a.fairness > b.fairness) return false;
    if(a.fairness < b.fairness) strict = true;
    if(a.overrides > b.overrides) return false;
    if(a.overrides < b.overrides) strict = true;
    return strict;
}

std::string solution_signature(const Solution &sol){
    std::stringstream key;
    for(const auto &day : sol.day_onsite){
        auto v = day;
        std::sort(v.begin(), v.end());
        for(const int64_t idx : v) key << idx << ',';
        key << ';';
    }
    return key.str();
}

std::string override_reason(const ParsedSchedule &schedule,
                            const ConstraintModel &model,
                            const Solution &solution,
                            const size_t day_index,
                            const int64_t staff_index){
    if(day_index >= solution.day_onsite.size()) return "reason=constraint explanation unavailable";

    auto counter_onsite = solution.day_onsite;
    auto &without_staff = counter_onsite[day_index];
    without_staff.erase(std::remove(without_staff.begin(), without_staff.end(), staff_index), without_staff.end());
    const auto counter_hard = evaluate_violation(without_staff, model.hard);

    std::vector<std::string> clauses;
    for(size_t r = 0; r < counter_hard.size() && r < schedule.requirements.size(); ++r){
        const int64_t current = (day_index < solution.day_violation.size() && r < solution.day_violation[day_index].size())
                              ? solution.day_violation[day_index][r] : 0;
        if(counter_hard[r] > current){
            if(current == 0) clauses.push_back("required to satisfy " + hard_display_name(schedule.requirements[r]));
            else clauses.push_back("reduces deficit for " + hard_display_name(schedule.requirements[r]));
        }
    }

    const auto counter_soft = evaluate_soft_penalties(schedule, model, counter_onsite);
    for(size_t i = 0; i < counter_soft.size() && i < solution.soft_penalty.size() && i < schedule.soft_constraints.size(); ++i){
        if(counter_soft[i] > solution.soft_penalty[i]){
            clauses.push_back("reduces " + soft_display_name(schedule.soft_constraints[i]) + " by "
                + std::to_string(counter_soft[i] - solution.soft_penalty[i]) + " penalty unit(s)");
        }
    }

    if(clauses.empty()){
        return "reason=secondary-objective trade-off; not individually required by any hard or soft constraint";
    }
    return "reason=" + join_strings(clauses, "; ");
}

} // anonymous namespace

std::vector<Solution> produce_variations(const ParsedSchedule &schedule,
                                         const ConstraintModel &model,
                                         const SolverConfig &config){
    const size_t n_days = schedule.days.size();
    std::vector<std::vector<DayCandidate>> day_candidates(n_days);
    for(size_t d = 0; d < n_days; ++d) day_candidates[d] = generate_day_candidates(schedule.days[d], model.hard);

    const auto baseline_choice = select_baseline(day_candidates);
    Solution baseline = build_solution(schedule, model, day_candidates, baseline_choice, config);

    size_t total_candidates = 0;
    for(const auto &v : day_candidates) total_candidates += v.size();
    YLOGINFO("ScheduleCoverage: parsed " << schedule.days.size() << " days, " << schedule.staff.size()
        << " staff, " << schedule.requirements.size() << " hard constraints and "
        << schedule.soft_constraints.size() << " weighted soft constraints; generated "
        << total_candidates << " per-day candidates");

    struct Weights { double fairness; double preference; };
    const std::vector<Weights> weight_combos = {
        { config.fairness_weight,       config.preference_weight },
        { 2.0 * config.fairness_weight, config.preference_weight },
        { 0.5 * config.fairness_weight, config.preference_weight },
        { config.fairness_weight, 2.0 * config.preference_weight },
        { config.fairness_weight, 0.5 * config.preference_weight },
    };

    const int64_t seed_count = std::max<int64_t>(1, std::min<int64_t>(8,
        (std::max<int64_t>(1, config.n_variations - 1) + static_cast<int64_t>(weight_combos.size()) - 1)
        / static_cast<int64_t>(weight_combos.size())));
    std::vector<Solution> explored;
    explored.push_back(baseline);
    for(const auto &wc : weight_combos){
        for(int64_t k = 0; k < seed_count; ++k){
            explored.push_back(local_search(schedule, model, day_candidates, baseline_choice, config,
                wc.fairness, wc.preference, static_cast<uint64_t>(config.seed + k)));
        }
    }

    std::vector<Solution> unique;
    std::set<std::string> seen;
    for(auto &sol : explored){
        const auto sig = solution_signature(sol);
        if(seen.insert(sig).second) unique.push_back(std::move(sol));
    }

    std::vector<size_t> front_idx;
    std::vector<size_t> dominated_idx;
    for(size_t i = 0; i < unique.size(); ++i){
        bool is_dominated = false;
        for(size_t j = 0; j < unique.size() && !is_dominated; ++j){
            if(i != j && dominates(unique[j], unique[i])) is_dominated = true;
        }
        unique[i].pareto_nondominated = !is_dominated;
        (is_dominated ? dominated_idx : front_idx).push_back(i);
    }

    const auto by_cost = [&](size_t ia, size_t ib){
        const auto &a = unique[ia];
        const auto &b = unique[ib];
        if(a.annealing_cost != b.annealing_cost) return a.annealing_cost < b.annealing_cost;
        if(a.hard_violation_units != b.hard_violation_units) return a.hard_violation_units < b.hard_violation_units;
        if(a.soft_constraint_cost != b.soft_constraint_cost) return a.soft_constraint_cost < b.soft_constraint_cost;
        if(a.fairness != b.fairness) return a.fairness < b.fairness;
        return a.overrides < b.overrides;
    };
    std::sort(front_idx.begin(), front_idx.end(), by_cost);
    std::sort(dominated_idx.begin(), dominated_idx.end(), by_cost);

    const size_t n_out = static_cast<size_t>(std::max<int64_t>(1, config.n_variations));
    std::vector<size_t> selected_idx;
    if(front_idx.size() <= n_out){
        selected_idx = front_idx;
    }else if(n_out == 1){
        selected_idx.push_back(front_idx.front());
    }else{
        for(size_t k = 0; k < n_out; ++k){
            const size_t pos = static_cast<size_t>(std::llround(
                static_cast<double>(k) * static_cast<double>(front_idx.size() - 1)
                / static_cast<double>(n_out - 1)));
            selected_idx.push_back(front_idx[pos]);
        }
    }
    for(const size_t idx : dominated_idx){
        if(selected_idx.size() >= n_out) break;
        selected_idx.push_back(idx);
    }

    std::vector<Solution> out;
    out.reserve(selected_idx.size());
    for(const size_t idx : selected_idx) out.push_back(unique[idx]);
    return out;
}

tables::table2 render_variation(const tables::table2 &original,
                                const ParsedSchedule &schedule,
                                const Solution &solution){
    tables::table2 out = original;
    for(size_t d = 0; d < schedule.days.size(); ++d){
        const auto &day = schedule.days[d];
        if(day.holiday || day.row < 0) continue;
        const std::set<int64_t> on(solution.day_onsite[d].begin(), solution.day_onsite[d].end());
        const std::set<int64_t> ov(solution.day_overridden[d].begin(), solution.day_overridden[d].end());
        for(size_t s = 0; s < day.classes.size(); ++s){
            const auto cls = day.classes[s];
            if(cls == CellClass::Undecided){
                out.inject(day.row, schedule.staff_columns[s], on.count(static_cast<int64_t>(s)) ? "onsite" : "remote");
            }else if(cls == CellClass::RemotePreference && ov.count(static_cast<int64_t>(s))){
                out.inject(day.row, schedule.staff_columns[s], "onsite");
            }
        }
    }

    const ConstraintModel model = build_constraint_model(schedule);
    int64_t r = out.next_empty_row() + 1;
    out.inject(r++, 0, "== Schedule Report ==");

    for(size_t d = 0; d < schedule.days.size(); ++d){
        if(schedule.days[d].holiday || d >= solution.day_violation.size()) continue;
        for(size_t i = 0; i < solution.day_violation[d].size() && i < schedule.requirements.size(); ++i){
            if(solution.day_violation[d][i] <= 0) continue;
            const auto &req = schedule.requirements[i];
            out.inject(r, 0, "FLAG");
            out.inject(r, 1, schedule.days[d].date);
            out.inject(r, 2, req.label);
            out.inject(r, 3, "type=" + req.type);
            out.inject(r, 4, "deficit=" + std::to_string(solution.day_violation[d][i]));
            out.inject(r, 5, "quota=" + req.quota_raw);
            ++r;
        }
    }

    for(size_t i = 0; i < schedule.soft_constraints.size() && i < solution.soft_penalty.size(); ++i){
        const auto &sc = schedule.soft_constraints[i];
        if(solution.soft_penalty[i] <= 0) continue;
        auto messages = soft_constraint_messages(schedule, model, solution.day_onsite, i);
        if(messages.empty()) messages.push_back("constraint has " + std::to_string(solution.soft_penalty[i]) + " penalty unit(s)");
        for(const auto &message : messages){
            out.inject(r, 0, "SOFT_FLAG");
            out.inject(r, 1, sc.label);
            out.inject(r, 2, "type=" + sc.type);
            out.inject(r, 3, "penalty=" + std::to_string(solution.soft_penalty[i]));
            out.inject(r, 4, "weight=" + format_double(sc.weight));
            out.inject(r, 5, message);
            ++r;
        }
    }

    for(size_t d = 0; d < schedule.days.size(); ++d){
        if(d >= solution.day_overridden.size()) continue;
        for(const int64_t idx : solution.day_overridden[d]){
            out.inject(r, 0, "OVERRIDE");
            out.inject(r, 1, schedule.days[d].date);
            out.inject(r, 2, schedule.staff[static_cast<size_t>(idx)]);
            out.inject(r, 3, "Remote -> onsite");
            out.inject(r, 4, override_reason(schedule, model, solution, d, idx));
            ++r;
        }
    }

    for(size_t s = 0; s < schedule.staff.size(); ++s){
        int64_t onsite = 0;
        int64_t remote = 0;
        int64_t vacation = 0;
        int64_t other = 0;
        for(size_t d = 0; d < schedule.days.size(); ++d){
            const auto &day = schedule.days[d];
            const auto cls = day.classes[s];
            const bool is_on = d < solution.day_onsite.size() && contains_index(solution.day_onsite[d], static_cast<int64_t>(s));
            const bool is_ov = d < solution.day_overridden.size() && contains_index(solution.day_overridden[d], static_cast<int64_t>(s));
            if(day.holiday) ++other;
            else if(cls == CellClass::Vacation) ++vacation;
            else if(cls == CellClass::Onsite) ++onsite;
            else if(cls == CellClass::Remote) ++remote;
            else if(cls == CellClass::Undecided){ if(is_on) ++onsite; else ++remote; }
            else if(cls == CellClass::RemotePreference){ if(is_ov) ++onsite; else ++remote; }
            else ++other;
        }
        out.inject(r, 0, "TALLY");
        out.inject(r, 1, schedule.staff[s]);
        out.inject(r, 2, "onsite=" + std::to_string(onsite));
        out.inject(r, 3, "remote=" + std::to_string(remote));
        out.inject(r, 4, "vacation=" + std::to_string(vacation));
        out.inject(r, 5, "other=" + std::to_string(other));
        ++r;
    }

    for(size_t i = 0; i < schedule.soft_constraints.size() && i < solution.soft_penalty.size(); ++i){
        const auto &sc = schedule.soft_constraints[i];
        out.inject(r, 0, "SOFT_OBJECTIVE");
        out.inject(r, 1, sc.label);
        out.inject(r, 2, "type=" + sc.type);
        out.inject(r, 3, "penalty=" + std::to_string(solution.soft_penalty[i]));
        out.inject(r, 4, "weight=" + format_double(sc.weight));
        out.inject(r, 5, "weighted_cost=" + format_double(sc.weight * static_cast<double>(solution.soft_penalty[i])));
        ++r;
    }

    out.inject(r, 0, "OBJECTIVES");
    out.inject(r, 1, "hard_violations=" + join(solution.violation_sum, ","));
    out.inject(r, 2, "hard_violation_units=" + std::to_string(solution.hard_violation_units));
    out.inject(r, 3, "fairness=" + format_double(solution.fairness));
    out.inject(r, 4, "overrides=" + std::to_string(solution.overrides));
    out.inject(r, 5, "soft_constraint_cost=" + format_double(solution.soft_constraint_cost));
    out.inject(r, 6, "annealing_cost=" + format_double(solution.annealing_cost));
    out.inject(r, 7, std::string("pareto_nondominated=") + (solution.pareto_nondominated ? "true" : "false"));
    return out;
}

} // namespace ScheduleCoverageCore

OperationDoc OpArgDocScheduleCoverage(){
    OperationDoc out;
    out.name = "ScheduleCoverage";
    out.tags.emplace_back("category: table processing");
    out.tags.emplace_back("category: medical physics");
    out.desc =
        "This operation ingests a staff-rostering table, parses hard and weighted soft constraints declared inline,"
        " assigns mutable schedule cells with simulated annealing, and emits auditable schedule variations. Hard"
        " constraints are per-day coverage rules. Soft constraints carry their weight in column 3 as Weight=<value>."
        " Supported soft constraints are max_consecutive_remote, exclusivity (for example XA XOR XB), and"
        " max_weekly_remote (for example XG = 2)."
        "\n\n"
        "For exclusivity, XOR denotes mutual exclusion rather than Boolean exactly-one: at most one listed staff member"
        " may be present on a day. Presence includes an assigned/fixed onsite cell and the fixed roles Prim and Sec."
        " max_weekly_remote uses repeated Date header blocks as week boundaries. Vacation and holiday cells neither"
        " count toward nor terminate a max_consecutive_remote run."
        "\n\n"
        "The final scalar annealing cost is RequirementViolationWeight * hard-violation units + FairnessWeight *"
        " fairness + PreferenceWeight * Remote-to-onsite overrides + the sum of each soft constraint's inline weight"
        " times its penalty units. The same end-user weights are used to report final annealing_cost even when several"
        " fairness/preference scalarizations are explored to discover tradeoffs. Every emitted table reports whether it"
        " is Pareto-nondominated among the unique schedules explored in the invocation.";

    out.args.emplace_back();
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "RequirementRegex";
    out.args.back().desc = "Regular expression identifying Hard Constraint and Soft Constraint label cells in column 0.";
    out.args.back().default_val = "^(Hard|Soft)\\s+Constraint";
    out.args.back().expected = true;
    out.args.back().examples = { "^(Hard|Soft)\\s+Constraint", "Constraint" };

    out.args.emplace_back();
    out.args.back().name = "HeaderRegex";
    out.args.back().desc = "Regular expression identifying schedule header rows whose remaining columns name staff.";
    out.args.back().default_val = "^Date$";
    out.args.back().expected = true;
    out.args.back().examples = { "^Date$", "^Week", "^Header" };

    out.args.emplace_back();
    out.args.back().name = "HolidayTerms";
    out.args.back().desc = "Comma-separated holiday terms. Whole-holiday days are skipped by optimization.";
    out.args.back().default_val = "Holiday";
    out.args.back().expected = true;
    out.args.back().examples = { "Holiday", "Holiday,Stat" };

    out.args.emplace_back();
    out.args.back().name = "VacationTerms";
    out.args.back().desc = "Comma-separated vacation/non-working terms. These are fixed and skipped by remote-run counting.";
    out.args.back().default_val = "Vac";
    out.args.back().expected = true;
    out.args.back().examples = { "Vac", "Vac,Leave", "Vacation,PTO" };

    out.args.emplace_back();
    out.args.back().name = "ImmutableTerms";
    out.args.back().desc = "Comma-separated fixed, non-coverage terms. Prim and Sec additionally count as present for exclusivity.";
    out.args.back().default_val = "CTO,Prim,Sec";
    out.args.back().expected = true;
    out.args.back().examples = { "CTO,Prim,Sec", "Clinic,Prim,Sec" };

    out.args.emplace_back();
    out.args.back().name = "OnsiteTerms";
    out.args.back().desc = "Comma-separated fixed onsite terms.";
    out.args.back().default_val = "onsite";
    out.args.back().expected = true;
    out.args.back().examples = { "onsite", "Onsite,OnSite" };

    out.args.emplace_back();
    out.args.back().name = "RemotePreferenceTerms";
    out.args.back().desc = "Comma-separated remote-preference terms that may be overridden to onsite.";
    out.args.back().default_val = "Remote";
    out.args.back().expected = true;
    out.args.back().examples = { "Remote", "Remote,Wfh" };

    out.args.emplace_back();
    out.args.back().name = "UndecidedTerms";
    out.args.back().desc = "Comma-separated mutable terms that must become onsite or remote.";
    out.args.back().default_val = "x";
    out.args.back().expected = true;
    out.args.back().examples = { "x", "x,TBD,?" };

    out.args.emplace_back();
    out.args.back().name = "RemoteTerms";
    out.args.back().desc = "Comma-separated fixed-remote terms.";
    out.args.back().default_val = "remote";
    out.args.back().expected = true;
    out.args.back().examples = { "remote", "remote,home" };

    out.args.emplace_back();
    out.args.back().name = "TermMatchMode";
    out.args.back().desc = "Whether term lists use exact or case-insensitive regex matching.";
    out.args.back().default_val = "exact";
    out.args.back().expected = true;
    out.args.back().examples = { "exact", "regex" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "NVariations";
    out.args.back().desc = "Number of schedule variations requested. Pareto-front schedules are preferred; if the explored"
                           " front contains fewer schedules, the lowest-cost dominated schedules are used to fill the request"
                           " and are explicitly marked pareto_nondominated=false.";
    out.args.back().default_val = "3";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "3", "5" };

    out.args.emplace_back();
    out.args.back().name = "FairnessMetric";
    out.args.back().desc = "Fairness metric: range, variance, or gini.";
    out.args.back().default_val = "range";
    out.args.back().expected = true;
    out.args.back().examples = { "range", "variance", "gini" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "FairnessWeight";
    out.args.back().desc = "Weight applied to the fairness objective.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "1", "2.5" };

    out.args.emplace_back();
    out.args.back().name = "PreferenceWeight";
    out.args.back().desc = "Weight applied to each Remote -> onsite preference override.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "1", "2.5" };

    out.args.emplace_back();
    out.args.back().name = "RequirementViolationWeight";
    out.args.back().desc = "Weight applied to each hard-constraint deficit unit during annealing. The default is deliberately"
                           " large so satisfiable hard constraints dominate ordinary soft tradeoffs; lowering it explicitly"
                           " allows the end-user to explore schedules that trade a hard deficit against soft objectives.";
    out.args.back().default_val = "1000.0";
    out.args.back().expected = true;
    out.args.back().examples = { "100", "1000", "10000" };

    out.args.emplace_back();
    out.args.back().name = "AnnealingIterations";
    out.args.back().desc = "Number of simulated-annealing proposals explored per scalarization/seed. Zero disables annealing."
                           " Larger values explore more schedules at additional runtime cost.";
    out.args.back().default_val = "100000";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "10000", "100000", "500000" };

    out.args.emplace_back();
    out.args.back().name = "Seed";
    out.args.back().desc = "Seed for deterministic simulated annealing.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "12345" };

    out.notes.emplace_back(
        "Soft-constraint weight is table-driven. For example: 'Soft Constraint 2, exclusivity, XA XOR XB, Weight=1.5'."
        " Missing, malformed, negative, or unsupported soft constraints are rejected with an error naming the row.");
    out.notes.emplace_back(
        "Report rows include FLAG for hard deficits, SOFT_FLAG with human-readable violation details, OVERRIDE with a"
        " counterfactual explanation, TALLY, SOFT_OBJECTIVE, and OBJECTIVES. OBJECTIVES includes the final annealing_cost"
        " and pareto_nondominated status. Pareto status is relative to the unique schedules actually explored, not a proof"
        " that no better schedule exists outside the explored search space.");
    return out;
}

bool ScheduleCoverage(Drover &DICOM_data,
                      const OperationArgPkg& OptArgs,
                      std::map<std::string, std::string>& /*InvocationMetadata*/,
                      const std::string& FilenameLex){
    Explicator X(FilenameLex);
    using namespace ScheduleCoverageCore;

    const auto TableSelectionStr = OptArgs.getValueStr("TableSelection").value();
    const auto RequirementRegexStr = OptArgs.getValueStr("RequirementRegex").value();
    const auto HeaderRegexStr = OptArgs.getValueStr("HeaderRegex").value();
    const auto TermMatchModeStr = OptArgs.getValueStr("TermMatchMode").value();

    TermLists terms;
    terms.holiday     = SplitStringToVector(OptArgs.getValueStr("HolidayTerms").value(), ',', 'd');
    terms.vacation    = SplitStringToVector(OptArgs.getValueStr("VacationTerms").value(), ',', 'd');
    terms.immutable   = SplitStringToVector(OptArgs.getValueStr("ImmutableTerms").value(), ',', 'd');
    terms.onsite      = SplitStringToVector(OptArgs.getValueStr("OnsiteTerms").value(), ',', 'd');
    terms.remote_pref = SplitStringToVector(OptArgs.getValueStr("RemotePreferenceTerms").value(), ',', 'd');
    terms.undecided   = SplitStringToVector(OptArgs.getValueStr("UndecidedTerms").value(), ',', 'd');
    terms.remote      = SplitStringToVector(OptArgs.getValueStr("RemoteTerms").value(), ',', 'd');

    const auto re_exact = Compile_Regex("^ex?a?c?t?$");
    const auto re_regex = Compile_Regex("^re?g?e?x?$");
    if(std::regex_match(TermMatchModeStr, re_exact)) terms.regex_mode = false;
    else if(std::regex_match(TermMatchModeStr, re_regex)) terms.regex_mode = true;
    else throw std::runtime_error("TermMatchMode argument not understood");

    SolverConfig config;
    config.fairness_metric = OptArgs.getValueStr("FairnessMetric").value();
    config.fairness_weight = std::stod(OptArgs.getValueStr("FairnessWeight").value());
    config.preference_weight = std::stod(OptArgs.getValueStr("PreferenceWeight").value());
    config.requirement_violation_weight = std::stod(OptArgs.getValueStr("RequirementViolationWeight").value());
    config.annealing_iterations = std::stoll(OptArgs.getValueStr("AnnealingIterations").value());
    config.n_variations = std::stoll(OptArgs.getValueStr("NVariations").value());
    config.seed = std::stoll(OptArgs.getValueStr("Seed").value());

    if(config.n_variations < 1) throw std::runtime_error("NVariations must be at least 1");
    if(config.annealing_iterations < 0) throw std::runtime_error("AnnealingIterations must be non-negative");
    if(config.fairness_weight < 0.0 || config.preference_weight < 0.0 || config.requirement_violation_weight < 0.0){
        throw std::runtime_error("FairnessWeight, PreferenceWeight, and RequirementViolationWeight must be non-negative");
    }
    if(!std::isfinite(config.fairness_weight) || !std::isfinite(config.preference_weight)
       || !std::isfinite(config.requirement_violation_weight)){
        throw std::runtime_error("Objective weights must be finite");
    }
    if(!(config.fairness_metric == "range" || config.fairness_metric == "variance" || config.fairness_metric == "gini")){
        throw std::runtime_error("FairnessMetric argument not understood");
    }

    auto STs_all = All_STs(DICOM_data);
    auto STs = Whitelist(STs_all, TableSelectionStr);
    if(STs.empty()) throw std::runtime_error("No table matched the TableSelection");

    for(auto &stp_it : STs){
        tables::table2 &t = (*stp_it)->table;
        if(t.data.empty()){
            YLOGWARN("Selected table is empty; skipping");
            continue;
        }

        const auto schedule = parse_schedule(t, RequirementRegexStr, HeaderRegexStr, terms);
        const auto model = build_constraint_model(schedule);
        const auto solutions = produce_variations(schedule, model, config);
        const std::string base_label = (t.metadata.count("TableLabel") != 0) ? t.metadata["TableLabel"] : "unspecified";

        for(size_t i = 0; i < solutions.size(); ++i){
            tables::table2 rendered = render_variation(t, schedule, solutions[i]);
            rendered.metadata = coalesce_metadata_for_basic_table(rendered.metadata, meta_evolve::iterate);
            const std::string suffix = " [schedule coverage variation " + std::to_string(i + 1) + "]";
            const std::string label = base_label + suffix;
            rendered.metadata["TableLabel"] = label;
            rendered.metadata["NormalizedTableLabel"] = X(label);
            rendered.metadata["ScheduleVariation"] = std::to_string(i + 1);
            rendered.metadata["ScheduleCoverageFairness"] = format_double(solutions[i].fairness);
            rendered.metadata["ScheduleCoverageOverrides"] = std::to_string(solutions[i].overrides);
            rendered.metadata["ScheduleCoverageHardViolations"] = join(solutions[i].violation_sum, ",");
            rendered.metadata["ScheduleCoverageHardViolationUnits"] = std::to_string(solutions[i].hard_violation_units);
            rendered.metadata["ScheduleCoverageSoftConstraintCost"] = format_double(solutions[i].soft_constraint_cost);
            rendered.metadata["ScheduleCoverageAnnealingCost"] = format_double(solutions[i].annealing_cost);
            rendered.metadata["ScheduleCoverageParetoNondominated"] = solutions[i].pareto_nondominated ? "true" : "false";
            rendered.metadata["ScheduleCoverageRequirementViolationWeight"] = format_double(config.requirement_violation_weight);
            rendered.metadata["ScheduleCoverageAnnealingIterations"] = std::to_string(config.annealing_iterations);

            auto st = std::make_shared<Sparse_Table>();
            st->table = std::move(rendered);
            DICOM_data.table_data.emplace_back(std::move(st));
        }
        YLOGINFO("ScheduleCoverage: emitted " << solutions.size() << " schedule variation(s)");
    }
    return true;
}
