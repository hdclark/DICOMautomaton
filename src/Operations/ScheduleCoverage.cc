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

int64_t sum_ints(const std::vector<int64_t> &v){
    int64_t out = 0;
    for(const auto x : v) out += x;
    return out;
}

bool is_all_digits(const std::string &s){
    if(s.empty()) return false;
    for(const char c : s){
        if(!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

bool contains_index(const std::vector<int64_t> &v, const int64_t idx){
    return std::find(v.begin(), v.end(), idx) != v.end();
}

bool exact_case_term_match(const std::string &cell,
                           const std::vector<std::string> &terms){
    const auto c = trim(cell);
    for(const auto &raw : terms){
        const auto t = trim(raw);
        if(!t.empty() && c == t) return true;
    }
    return false;
}

bool term_list_matches(const std::string &cell,
                       const std::vector<std::string> &terms,
                       bool regex_mode){
    const std::string c = trim(cell);
    for(const auto &raw : terms){
        const std::string term = trim(raw);
        if(term.empty()) continue;
        if(regex_mode){
            try{
                const auto re = std::regex(term, std::regex::icase | std::regex::optimize | std::regex::ECMAScript);
                if(std::regex_match(c, re)) return true;
            }catch(const std::regex_error &){
                throw std::runtime_error("Unable to compile cell-classification regex '" + term + "'");
            }
        }else if(to_lower(c) == to_lower(term)){
            return true;
        }
    }
    return false;
}

void validate_exact_term_lists(const TermLists &terms){
    struct NamedTerms {
        const char *name;
        const std::vector<std::string> *terms;
    };
    const std::vector<NamedTerms> groups = {
        { "HolidayTerms", &terms.holiday },
        { "VacationTerms", &terms.vacation },
        { "ImmutableTerms", &terms.immutable },
        { "OnsiteTerms", &terms.onsite },
        { "RemotePreferenceTerms", &terms.remote_pref },
        { "UndecidedTerms", &terms.undecided },
        { "RemoteTerms", &terms.remote },
    };

    // Exact spelling duplicates make one parameter unreachable because classification has a deterministic
    // category priority. Case-only variants are intentionally allowed: they disambiguate the default
    // Remote (preference) from remote (final/fixed remote) while retaining case-insensitive fallback.
    std::map<std::string, std::string> owner;
    for(const auto &g : groups){
        for(const auto &raw : *g.terms){
            const auto term = trim(raw);
            if(term.empty()) continue;
            const auto it = owner.find(term);
            if(it != owner.end()){
                throw std::runtime_error("Cell term '" + term + "' appears in both " + it->second
                    + " and " + g.name + "; use distinct exact spellings so both parameters are effective");
            }
            owner.emplace(term, g.name);
        }
    }
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

int64_t parse_nonnegative_int_arg(const std::string &name, const std::string &raw){
    const auto s = trim(raw);
    if(!is_all_digits(s)){
        throw std::runtime_error(name + " must be a non-negative integer, but found '" + raw + "'");
    }
    try{
        return std::stoll(s);
    }catch(const std::exception &){
        throw std::runtime_error(name + " is outside the supported integer range");
    }
}

int64_t parse_signed_int_arg(const std::string &name, const std::string &raw){
    const auto s = trim(raw);
    static const std::regex re("^[+-]?[0-9]+$", std::regex::ECMAScript);
    if(!std::regex_match(s, re)){
        throw std::runtime_error(name + " must be an integer, but found '" + raw + "'");
    }
    try{
        return std::stoll(s);
    }catch(const std::exception &){
        throw std::runtime_error(name + " is outside the supported integer range");
    }
}

double parse_nonnegative_double_arg(const std::string &name, const std::string &raw){
    const auto s = trim(raw);
    static const std::regex re(
        "^[+]?([0-9]+(\\.[0-9]*)?|\\.[0-9]+)([eE][+-]?[0-9]+)?$",
        std::regex::ECMAScript);
    if(!std::regex_match(s, re)){
        throw std::runtime_error(name + " must be a non-negative finite number, but found '" + raw + "'");
    }
    double out = 0.0;
    try{
        out = std::stod(s);
    }catch(const std::exception &){
        throw std::runtime_error(name + " could not be converted to a number");
    }
    if(!std::isfinite(out) || out < 0.0){
        throw std::runtime_error(name + " must be a non-negative finite number");
    }
    return out;
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

bool lex_violation_less(const std::vector<int64_t> &a,
                        const std::vector<int64_t> &b){
    const size_t n = std::min(a.size(), b.size());
    for(size_t i = 0; i < n; ++i){
        if(a[i] != b[i]) return a[i] < b[i];
    }
    return a.size() < b.size();
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
                    messages.push_back(schedule.staff[s] + " has " + std::to_string(run)
                        + " consecutive remote workdays from '" + first_date + "' through '" + last_date
                        + "'; maximum=" + std::to_string(sc.limit)
                        + ", excess=" + std::to_string(run - sc.limit));
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
                messages.push_back("on '" + day.date + "', mutually exclusive staff "
                    + join_strings(present, ", ")
                    + " are simultaneously present (onsite, Prim, or Sec); excess="
                    + std::to_string(static_cast<int64_t>(present.size()) - 1));
            }
        }
    }else if(sc.kind == SoftConstraintKind::MaxWeeklyRemote){
        if(sc.staff.empty()) return messages;
        const size_t s = static_cast<size_t>(sc.staff.front());
        std::map<int64_t, int64_t> counts;
        std::map<int64_t, std::string> first_dates;
        std::map<int64_t, std::string> last_dates;
        for(size_t d = 0; d < schedule.days.size(); ++d){
            const auto week = schedule.days[d].week_index;
            if(first_dates.count(week) == 0) first_dates[week] = schedule.days[d].date;
            last_dates[week] = schedule.days[d].date;
            if(is_remote_workday(schedule.days[d], s, day_onsite[d])){
                ++counts[week];
            }
        }
        for(const auto &kv : counts){
            if(kv.second > sc.limit){
                messages.push_back(schedule.staff[s] + " has " + std::to_string(kv.second)
                    + " remote workdays in week block " + std::to_string(kv.first + 1)
                    + " ('" + first_dates[kv.first] + "' through '" + last_dates[kv.first] + "')"
                    + "; maximum=" + std::to_string(sc.limit)
                    + ", excess=" + std::to_string(kv.second - sc.limit));
            }
        }
    }

    return messages;
}

std::string hard_constraint_message(const ParsedSchedule &schedule,
                                    const ConstraintModel &model,
                                    const Solution &solution,
                                    const size_t day_index,
                                    const size_t req_index){
    if(day_index >= solution.day_onsite.size()
       || req_index >= model.hard.subsets.size()
       || req_index >= model.hard.min_onsite.size()){
        return "coverage details unavailable";
    }
    int64_t actual = 0;
    for(const auto idx : model.hard.subsets[req_index]){
        if(contains_index(solution.day_onsite[day_index], idx)) ++actual;
    }
    const int64_t required = model.hard.min_onsite[req_index];
    const int64_t deficit = std::max<int64_t>(0, required - actual);

    std::vector<std::string> eligible;
    for(const auto idx : model.hard.subsets[req_index]){
        if(idx >= 0 && static_cast<size_t>(idx) < schedule.staff.size()){
            eligible.push_back(schedule.staff[static_cast<size_t>(idx)]);
        }
    }

    std::stringstream ss;
    ss << "onsite_count=" << actual
       << ", required>=" << required
       << ", deficit=" << deficit;
    if(!eligible.empty()) ss << ", eligible_staff=" << join_strings(eligible, "|");
    return ss.str();
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

    // Honour exact spelling before any case-insensitive exact/regex fallback. This resolves the intentional
    // default distinction between Remote (overrideable preference) and remote (final/fixed remote), including
    // when TermMatchMode=regex. Regex semantics are used only after no literal exact spelling matches.
    if(mark(exact_case_term_match(cell, terms.holiday)))     return CellClass::Holiday;
    if(mark(exact_case_term_match(cell, terms.vacation)))    return CellClass::Vacation;
    if(mark(exact_case_term_match(cell, terms.onsite)))      return CellClass::Onsite;
    if(mark(exact_case_term_match(cell, terms.remote_pref))) return CellClass::RemotePreference;
    if(mark(exact_case_term_match(cell, terms.undecided)))   return CellClass::Undecided;
    if(mark(exact_case_term_match(cell, terms.remote)))      return CellClass::Remote;
    if(mark(exact_case_term_match(cell, terms.immutable)))   return CellClass::Immutable;

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
        if(parts.size() >= 3
           && to_lower(parts[0]) == "any"
           && is_all_digits(parts[1])
           && to_lower(parts[2]) == "of"){
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
    if(table.data.empty()){
        throw std::runtime_error("The selected table is empty; there is no schedule to process");
    }

    validate_exact_term_lists(terms);

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
            if(first_cell_opt){
                const auto label = first_cell_opt.value();
                const bool constraint_like = label_is_hard(label) || label_is_soft(label);
                const bool regex_matches = std::regex_search(label, re_constraint);

                if(constraint_like && !regex_matches){
                    throw std::runtime_error("Constraint row '" + label + "' was excluded by RequirementRegex='"
                        + constraint_regex + "'. Refusing to silently ignore declared constraints");
                }

                if(regex_matches){
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

                    throw std::runtime_error("Row '" + label
                        + "' matched RequirementRegex but is not a recognized Hard Constraint or Soft Constraint");
                }

                if(std::regex_search(label, re_hdr)){
                    std::set<std::string> unique_staff;
                    for(int64_t c = min_col + 1; c <= max_col; ++c){
                        const auto v_opt = table.value(r, c);
                        if(v_opt && !trim(v_opt.value()).empty()){
                            const auto staff = trim(v_opt.value());
                            if(!unique_staff.insert(staff).second){
                                throw std::runtime_error("Schedule header contains duplicate staff name '" + staff + "'");
                            }
                            out.staff.push_back(staff);
                            staff_columns.push_back(c);
                        }
                    }
                    if(out.staff.empty()){
                        throw std::runtime_error("A schedule header row was found, but no staff columns were identified");
                    }
                    out.staff_columns = staff_columns;
                    current_week = 0;
                    in_schedule = true;
                    continue;
                }
            }
            continue;
        }

        if(!first_cell_opt) continue;
        const auto first_cell = first_cell_opt.value();

        if(label_is_hard(first_cell) || label_is_soft(first_cell)){
            throw std::runtime_error("Constraint row '" + first_cell
                + "' appears after the schedule has started; all constraints must precede the first header");
        }

        if(std::regex_search(first_cell, re_hdr)){
            std::vector<std::string> header_staff;
            for(int64_t c = min_col + 1; c <= max_col; ++c){
                const auto v_opt = table.value(r, c);
                if(v_opt && !trim(v_opt.value()).empty()) header_staff.push_back(trim(v_opt.value()));
            }
            if(header_staff != out.staff){
                throw std::runtime_error("Repeated schedule header differs from the first header; staff columns must remain identical");
            }
            ++current_week;
            continue;
        }

        Day day;
        day.row = r;
        day.date = first_cell;
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
                YLOGWARN("Day '" << day.date << "' mixes holiday and non-holiday cells; holiday cells are non-counting");
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


ConstraintModel build_constraint_model(const ParsedSchedule &schedule){
    ConstraintModel model;
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
        for(const int64_t idx : model.subsets[r]){
            if(on.count(idx) != 0) ++cnt;
        }
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
                if(is_remote_workday(schedule.days[d], s, day_onsite[d])){
                    ++counts[schedule.days[d].week_index];
                }
            }
            for(const auto &kv : counts){
                if(kv.second > sc.limit) penalties[i] += kv.second - sc.limit;
            }
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
    if(M >= 63){
        throw std::runtime_error("A day has too many mutable cells to enumerate safely");
    }

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

    const auto better = [](const DayCandidate &a, const DayCandidate &b){
        if(a.violation != b.violation) return lex_violation_less(a.violation, b.violation);
        if(a.overridden.size() != b.overridden.size()) return a.overridden.size() < b.overridden.size();
        if(a.onsite.size() != b.onsite.size()) return a.onsite.size() < b.onsite.size();
        return a.onsite < b.onsite;
    };

    for(size_t d = 0; d < day_candidates.size(); ++d){
        const auto &cs = day_candidates[d];
        if(cs.empty()) continue;
        size_t best = 0;
        for(size_t i = 1; i < cs.size(); ++i){
            if(better(cs[i], cs[best])) best = i;
        }
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
            for(size_t j = i + 1; j < sorted.size(); ++j){
                numerator += sorted[j] - sorted[i];
            }
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
        if(d >= choice.size() || choice[d] >= cs.size()){
            throw std::runtime_error("Internal ScheduleCoverage error: candidate choice is out of range");
        }

        const auto &cand = cs[choice[d]];
        sol.day_onsite[d] = cand.onsite;
        sol.day_overridden[d] = cand.overridden;
        sol.day_violation[d] = cand.violation;

        for(const int64_t s : cand.onsite){
            if(s < 0 || static_cast<size_t>(s) >= sol.staff_onsite.size()){
                throw std::runtime_error("Internal ScheduleCoverage error: candidate references invalid staff index");
            }
            ++sol.staff_onsite[static_cast<size_t>(s)];
        }

        for(size_t r = 0; r < cand.violation.size(); ++r){
            if(r >= sol.violation_sum.size()){
                throw std::runtime_error("Internal ScheduleCoverage error: hard-violation vector size mismatch");
            }
            sol.violation_sum[r] += cand.violation[r];
        }
        sol.overrides += static_cast<int64_t>(cand.overridden.size());
    }

    sol.hard_violation_units = sum_ints(sol.violation_sum);
    sol.fairness = fairness_penalty(sol.staff_onsite, config.fairness_metric);
    sol.soft_penalty = evaluate_soft_penalties(schedule, model, sol.day_onsite);

    if(sol.soft_penalty.size() != model.soft.size()){
        throw std::runtime_error("Internal ScheduleCoverage error: soft-penalty vector size mismatch");
    }
    for(size_t i = 0; i < sol.soft_penalty.size(); ++i){
        sol.soft_constraint_cost += model.soft[i].weight * static_cast<double>(sol.soft_penalty[i]);
    }

    sol.annealing_cost =
          config.requirement_violation_weight * static_cast<double>(sol.hard_violation_units)
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

struct SearchResult {
    Solution best_hard_optimal;
    int64_t proposals = 0;
};

SearchResult local_search(const ParsedSchedule &schedule,
                          const ConstraintModel &model,
                          const std::vector<std::vector<DayCandidate>> &day_candidates,
                          const std::vector<size_t> &initial_choice,
                          const std::vector<int64_t> &target_hard_violation,
                          const SolverConfig &config,
                          const double fairness_weight,
                          const double preference_weight,
                          const uint64_t seed){
    SearchResult result;

    std::vector<size_t> choice = initial_choice;
    Solution current = build_solution(schedule, model, day_candidates, choice, config);
    double current_cost = search_cost(current, config, fairness_weight, preference_weight);

    result.best_hard_optimal = current;
    if(current.violation_sum != target_hard_violation){
        throw std::runtime_error("Internal ScheduleCoverage error: annealing did not start on the hard-optimal surface");
    }
    double best_valid_cost = current_cost;

    std::vector<size_t> mutable_days;
    for(size_t d = 0; d < day_candidates.size(); ++d){
        if(day_candidates[d].size() > 1) mutable_days.push_back(d);
    }
    if(mutable_days.empty() || config.annealing_iterations <= 0){
        return result;
    }

    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> U(0.0, 1.0);
    std::uniform_int_distribution<size_t> day_dist(0, mutable_days.size() - 1);

    // Temperature is based on the hard-optimal starting cost. RequirementViolationWeight then controls how
    // readily the chain can make temporary excursions away from hard feasibility.
    const double T0 = std::max(1.0, std::abs(current_cost) * 0.25 + 1.0);

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
        ++result.proposals;

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

            // Hard constraints remain genuinely hard at the output boundary. The chain may temporarily leave
            // this surface, but invalid states can never become returned solutions.
            if(current.violation_sum == target_hard_violation){
                if(current_cost < best_valid_cost){
                    result.best_hard_optimal = current;
                    best_valid_cost = current_cost;
                }
            }
        }else{
            choice[d] = old_idx;
        }
    }

    return result;
}

bool dominates(const Solution &a, const Solution &b){
    bool strict = false;
    if(a.violation_sum.size() != b.violation_sum.size()) return false;
    if(a.soft_penalty.size() != b.soft_penalty.size()) return false;

    // Hard objectives retain table order / lexicographic meaning. All normally returned schedules share the
    // same hard vector, but keep this comparison defensive and useful for tests.
    if(a.violation_sum != b.violation_sum){
        if(lex_violation_less(b.violation_sum, a.violation_sum)) return false;
        if(lex_violation_less(a.violation_sum, b.violation_sum)) strict = true;
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

std::vector<std::vector<size_t>> hard_optimal_options(
        const std::vector<std::vector<DayCandidate>> &day_candidates,
        const std::vector<size_t> &baseline_choice){
    std::vector<std::vector<size_t>> options(day_candidates.size());

    for(size_t d = 0; d < day_candidates.size(); ++d){
        const auto &pool = day_candidates[d];
        if(pool.empty()) continue;
        if(d >= baseline_choice.size() || baseline_choice[d] >= pool.size()){
            throw std::runtime_error("Internal ScheduleCoverage error: invalid baseline candidate");
        }

        const auto target = pool[baseline_choice[d]].violation;
        options[d].push_back(baseline_choice[d]);
        for(size_t i = 0; i < pool.size(); ++i){
            if(i == baseline_choice[d]) continue;
            if(pool[i].violation == target) options[d].push_back(i);
        }

        std::sort(options[d].begin() + 1, options[d].end(),
                  [&](const size_t a, const size_t b){
                      const auto &ca = pool[a];
                      const auto &cb = pool[b];
                      if(ca.overridden.size() != cb.overridden.size()){
                          return ca.overridden.size() < cb.overridden.size();
                      }
                      if(ca.onsite.size() != cb.onsite.size()) return ca.onsite.size() < cb.onsite.size();
                      return ca.onsite < cb.onsite;
                  });
    }

    return options;
}

int64_t structural_variation_count_capped(const std::vector<std::vector<size_t>> &options,
                                          const int64_t cap){
    if(cap <= 1) return 1;
    int64_t product = 1;
    for(const auto &v : options){
        const int64_t factor = static_cast<int64_t>(std::max<size_t>(1, v.size()));
        if(product >= cap) return cap;
        if(factor > 0 && product > cap / factor) return cap;
        product *= factor;
    }
    return std::min(product, cap);
}

void add_unique_solution(std::vector<Solution> &unique,
                         std::set<std::string> &seen,
                         Solution sol,
                         const std::vector<int64_t> &target_hard_violation){
    if(sol.violation_sum != target_hard_violation) return;
    sol.hard_optimal = true;
    const auto sig = solution_signature(sol);
    if(seen.insert(sig).second) unique.push_back(std::move(sol));
}

void deterministically_fill_variations(
        const ParsedSchedule &schedule,
        const ConstraintModel &model,
        const std::vector<std::vector<DayCandidate>> &day_candidates,
        const std::vector<size_t> &baseline_choice,
        const std::vector<std::vector<size_t>> &options,
        const std::vector<int64_t> &target_hard_violation,
        const SolverConfig &config,
        const size_t target_count,
        std::vector<Solution> &unique,
        std::set<std::string> &seen){
    if(unique.size() >= target_count) return;

    std::vector<size_t> variable_days;
    for(size_t d = 0; d < options.size(); ++d){
        if(options[d].size() > 1) variable_days.push_back(d);
    }
    if(variable_days.empty()) return;

    std::vector<size_t> pos(variable_days.size(), 0);
    bool exhausted = false;
    while(unique.size() < target_count && !exhausted){
        // Mixed-radix increment, with the first variable day as the least-significant digit.
        size_t digit = 0;
        for(; digit < variable_days.size(); ++digit){
            const size_t d = variable_days[digit];
            ++pos[digit];
            if(pos[digit] < options[d].size()) break;
            pos[digit] = 0;
        }
        if(digit == variable_days.size()){
            exhausted = true;
            break;
        }

        std::vector<size_t> choice = baseline_choice;
        for(size_t i = 0; i < variable_days.size(); ++i){
            const size_t d = variable_days[i];
            choice[d] = options[d][pos[i]];
        }

        auto sol = build_solution(schedule, model, day_candidates, choice, config);
        add_unique_solution(unique, seen, std::move(sol), target_hard_violation);
    }
}

std::string override_reason(const ParsedSchedule &schedule,
                            const ConstraintModel &model,
                            const Solution &solution,
                            const size_t day_index,
                            const int64_t staff_index){
    if(day_index >= solution.day_onsite.size()){
        return "reason=constraint explanation unavailable";
    }

    auto counter_onsite = solution.day_onsite;
    auto &without_staff = counter_onsite[day_index];
    without_staff.erase(std::remove(without_staff.begin(), without_staff.end(), staff_index), without_staff.end());

    const auto counter_hard = evaluate_violation(without_staff, model.hard);
    std::vector<std::string> clauses;

    for(size_t r = 0; r < counter_hard.size() && r < schedule.requirements.size(); ++r){
        const int64_t current =
            (day_index < solution.day_violation.size() && r < solution.day_violation[day_index].size())
            ? solution.day_violation[day_index][r] : 0;
        if(counter_hard[r] > current){
            if(current == 0){
                clauses.push_back("required to satisfy " + hard_display_name(schedule.requirements[r]));
            }else{
                clauses.push_back("reduces deficit for " + hard_display_name(schedule.requirements[r]));
            }
        }
    }

    const auto counter_soft = evaluate_soft_penalties(schedule, model, counter_onsite);
    for(size_t i = 0; i < counter_soft.size()
                    && i < solution.soft_penalty.size()
                    && i < schedule.soft_constraints.size(); ++i){
        if(counter_soft[i] > solution.soft_penalty[i]){
            clauses.push_back("reduces " + soft_display_name(schedule.soft_constraints[i]) + " by "
                + std::to_string(counter_soft[i] - solution.soft_penalty[i]) + " penalty unit(s)");
        }
    }

    if(clauses.empty()){
        return "reason=secondary-objective trade-off; not individually required by a hard or soft constraint";
    }
    return "reason=" + join_strings(clauses, "; ");
}

} // anonymous namespace


std::vector<Solution> produce_variations(const ParsedSchedule &schedule,
                                         const ConstraintModel &model,
                                         const SolverConfig &config){
    if(config.n_variations < 1){
        throw std::runtime_error("NVariations must be at least 1");
    }
    if(config.annealing_iterations < 0){
        throw std::runtime_error("AnnealingIterations must be non-negative");
    }

    const size_t n_days = schedule.days.size();
    std::vector<std::vector<DayCandidate>> day_candidates(n_days);
    for(size_t d = 0; d < n_days; ++d){
        day_candidates[d] = generate_day_candidates(schedule.days[d], model.hard);
    }

    const auto baseline_choice = select_baseline(day_candidates);
    Solution baseline = build_solution(schedule, model, day_candidates, baseline_choice, config);
    baseline.hard_optimal = true;
    const auto target_hard_violation = baseline.violation_sum;

    const auto optimal_options = hard_optimal_options(day_candidates, baseline_choice);
    const int64_t structural_capped =
        structural_variation_count_capped(optimal_options, config.n_variations);

    size_t total_candidates = 0;
    for(const auto &v : day_candidates) total_candidates += v.size();

    YLOGINFO("ScheduleCoverage: parsed " << schedule.days.size() << " days, " << schedule.staff.size()
        << " staff, " << schedule.requirements.size() << " hard constraints and "
        << schedule.soft_constraints.size() << " weighted soft constraints; generated "
        << total_candidates << " per-day candidates; hard-optimal violation vector=["
        << join(target_hard_violation, ",") << "]");

    // NVariations changes both the requested output count and search breadth. Keep the run count bounded so
    // large output requests do not multiply runtime without limit.
    int64_t annealing_runs = 0;
    if(config.annealing_iterations > 0){
        const int64_t requested_search_runs =
            (config.n_variations >= 6) ? 12 : (2 * config.n_variations);
        annealing_runs = std::min<int64_t>(12, std::max<int64_t>(5, requested_search_runs));
    }

    struct WeightFactors {
        double fairness;
        double preference;
    };
    const std::vector<WeightFactors> factors = {
        { 1.0,  1.0 },
        { 2.0,  1.0 },
        { 0.5,  1.0 },
        { 1.0,  2.0 },
        { 1.0,  0.5 },
        { 4.0,  1.0 },
        { 0.25, 1.0 },
        { 1.0,  4.0 },
        { 1.0,  0.25 },
    };

    std::vector<Solution> unique;
    std::set<std::string> seen;
    add_unique_solution(unique, seen, baseline, target_hard_violation);

    int64_t proposals_executed = 0;
    for(int64_t run = 0; run < annealing_runs; ++run){
        const auto f = factors[static_cast<size_t>(run) % factors.size()];
        const double w_fair = config.fairness_weight * f.fairness;
        const double w_pref = config.preference_weight * f.preference;
        const uint64_t run_seed =
            static_cast<uint64_t>(config.seed)
            + static_cast<uint64_t>(0x9E3779B97F4A7C15ULL) * static_cast<uint64_t>(run + 1);

        auto sr = local_search(schedule, model, day_candidates, baseline_choice,
                               target_hard_violation, config, w_fair, w_pref, run_seed);
        proposals_executed += sr.proposals;
        add_unique_solution(unique, seen, std::move(sr.best_hard_optimal), target_hard_violation);
    }

    // NVariations is an output contract whenever enough distinct hard-optimal schedules structurally exist.
    // If annealing converges repeatedly to the same optimum, deterministically enumerate neighbouring
    // hard-optimal choices rather than silently returning fewer tables.
    const size_t desired =
        static_cast<size_t>(std::min<int64_t>(config.n_variations, structural_capped));
    deterministically_fill_variations(schedule, model, day_candidates, baseline_choice,
                                      optimal_options, target_hard_violation, config, desired,
                                      unique, seen);

    if(unique.empty()){
        throw std::runtime_error("Internal ScheduleCoverage error: no hard-optimal schedule survived search");
    }

    // Every candidate admitted above is hard-optimal. Assert this again before Pareto projection so a future
    // maintenance regression cannot silently emit invalid schedules.
    for(auto &sol : unique){
        if(sol.violation_sum != target_hard_violation){
            throw std::runtime_error("Internal ScheduleCoverage error: non-optimal hard-constraint schedule reached output selection");
        }
        sol.hard_optimal = true;
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
        if(a.soft_constraint_cost != b.soft_constraint_cost) return a.soft_constraint_cost < b.soft_constraint_cost;
        if(a.fairness != b.fairness) return a.fairness < b.fairness;
        if(a.overrides != b.overrides) return a.overrides < b.overrides;
        return solution_signature(a) < solution_signature(b);
    };
    std::sort(front_idx.begin(), front_idx.end(), by_cost);
    std::sort(dominated_idx.begin(), dominated_idx.end(), by_cost);

    const size_t n_out = desired;
    std::vector<size_t> selected_idx;

    if(front_idx.size() <= n_out){
        selected_idx = front_idx;
    }else if(n_out == 1){
        selected_idx.push_back(front_idx.front());
    }else{
        // Spread requested outputs across the discovered front after sorting by the user's final scalar cost.
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

    if(selected_idx.size() < n_out){
        throw std::runtime_error("Internal ScheduleCoverage error: unable to satisfy requested distinct variation count");
    }

    std::vector<Solution> out;
    out.reserve(selected_idx.size());
    for(const size_t idx : selected_idx) out.push_back(unique[idx]);

    for(auto &sol : out){
        sol.annealing_runs = annealing_runs;
        sol.annealing_proposals = proposals_executed;
        sol.requested_variations = config.n_variations;
        sol.returned_variations = static_cast<int64_t>(out.size());
        sol.hard_optimal = true;
    }

    if(static_cast<int64_t>(out.size()) < config.n_variations){
        YLOGWARN("ScheduleCoverage: requested " << config.n_variations
            << " distinct variations, but only " << out.size()
            << " hard-optimal schedules structurally exist");
    }

    YLOGINFO("ScheduleCoverage: annealing runs=" << annealing_runs
        << ", iterations/run=" << config.annealing_iterations
        << ", proposals executed=" << proposals_executed
        << ", returned variations=" << out.size());

    return out;
}


tables::table2 render_variation(const tables::table2 &original,
                                const ParsedSchedule &schedule,
                                const Solution &solution,
                                const SolverConfig *config){
    tables::table2 out = original;

    if(solution.day_onsite.size() != schedule.days.size()
       || solution.day_violation.size() != schedule.days.size()){
        throw std::runtime_error("Internal ScheduleCoverage error: solution day count does not match parsed schedule");
    }
    if(solution.violation_sum.size() != schedule.requirements.size()){
        throw std::runtime_error("Internal ScheduleCoverage error: solution hard-constraint vector does not match parsed constraints");
    }

    for(size_t d = 0; d < schedule.days.size(); ++d){
        const auto &day = schedule.days[d];
        if(day.holiday || day.row < 0) continue;

        const std::set<int64_t> on(solution.day_onsite[d].begin(), solution.day_onsite[d].end());
        for(size_t s = 0; s < day.classes.size(); ++s){
            const auto cls = day.classes[s];
            if(cls == CellClass::Undecided || cls == CellClass::RemotePreference){
                // Resolve every mutable input into a final state. Honoured Remote preferences become lower-case
                // fixed remote, which the default exact-mode classifier distinguishes from Remote preferences.
                out.inject(day.row, schedule.staff_columns[s],
                           on.count(static_cast<int64_t>(s)) ? "onsite" : "remote");
            }
        }
    }

    const ConstraintModel model = build_constraint_model(schedule);
    int64_t r = out.next_empty_row() + 1;
    out.inject(r++, 0, "== Schedule Report ==");

    // Hard constraints: every deficit is explicitly acknowledged with actual and required coverage.
    for(size_t d = 0; d < schedule.days.size(); ++d){
        if(schedule.days[d].holiday) continue;
        if(d >= solution.day_violation.size()){
            throw std::runtime_error("Internal ScheduleCoverage error: missing day hard-violation vector");
        }
        if(solution.day_violation[d].size() != schedule.requirements.size()){
            throw std::runtime_error("Internal ScheduleCoverage error: day hard-violation vector size mismatch");
        }

        for(size_t i = 0; i < schedule.requirements.size(); ++i){
            if(solution.day_violation[d][i] <= 0) continue;
            const auto &req = schedule.requirements[i];
            out.inject(r, 0, "FLAG");
            out.inject(r, 1, schedule.days[d].date);
            out.inject(r, 2, req.label);
            out.inject(r, 3, "type=" + req.type);
            out.inject(r, 4, hard_constraint_message(schedule, model, solution, d, i));
            out.inject(r, 5, "quota=" + req.quota_raw);
            ++r;
        }
    }

    // Soft constraints: report every human-readable violation plus its table-specified weight.
    for(size_t i = 0; i < schedule.soft_constraints.size(); ++i){
        if(i >= solution.soft_penalty.size()){
            throw std::runtime_error("Internal ScheduleCoverage error: missing soft-constraint penalty");
        }
        if(solution.soft_penalty[i] <= 0) continue;

        const auto &sc = schedule.soft_constraints[i];
        auto messages = soft_constraint_messages(schedule, model, solution.day_onsite, i);
        if(messages.empty()){
            messages.push_back("constraint has " + std::to_string(solution.soft_penalty[i]) + " penalty unit(s)");
        }
        for(const auto &message : messages){
            out.inject(r, 0, "SOFT_FLAG");
            out.inject(r, 1, sc.label);
            out.inject(r, 2, "type=" + sc.type);
            out.inject(r, 3, "total_penalty=" + std::to_string(solution.soft_penalty[i]));
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
            const bool is_on = contains_index(solution.day_onsite[d], static_cast<int64_t>(s));

            if(day.holiday) ++other;
            else if(cls == CellClass::Vacation) ++vacation;
            else if(cls == CellClass::Onsite) ++onsite;
            else if(cls == CellClass::Remote) ++remote;
            else if(cls == CellClass::Undecided || cls == CellClass::RemotePreference){
                if(is_on) ++onsite;
                else ++remote;
            }else ++other;
        }

        out.inject(r, 0, "TALLY");
        out.inject(r, 1, schedule.staff[s]);
        out.inject(r, 2, "onsite=" + std::to_string(onsite));
        out.inject(r, 3, "remote=" + std::to_string(remote));
        out.inject(r, 4, "vacation=" + std::to_string(vacation));
        out.inject(r, 5, "other=" + std::to_string(other));
        ++r;
    }

    for(size_t i = 0; i < schedule.soft_constraints.size(); ++i){
        if(i >= solution.soft_penalty.size()){
            throw std::runtime_error("Internal ScheduleCoverage error: missing soft objective");
        }
        const auto &sc = schedule.soft_constraints[i];
        out.inject(r, 0, "SOFT_OBJECTIVE");
        out.inject(r, 1, sc.label);
        out.inject(r, 2, "type=" + sc.type);
        out.inject(r, 3, "penalty=" + std::to_string(solution.soft_penalty[i]));
        out.inject(r, 4, "weight=" + format_double(sc.weight));
        out.inject(r, 5, "weighted_cost="
            + format_double(sc.weight * static_cast<double>(solution.soft_penalty[i])));
        ++r;
    }

    out.inject(r, 0, "OBJECTIVES");
    out.inject(r, 1, "hard_violations=" + join(solution.violation_sum, ","));
    out.inject(r, 2, "hard_violation_units=" + std::to_string(solution.hard_violation_units));
    out.inject(r, 3, std::string("hard_optimal=") + (solution.hard_optimal ? "true" : "false"));
    out.inject(r, 4, "fairness=" + format_double(solution.fairness));
    out.inject(r, 5, "overrides=" + std::to_string(solution.overrides));
    out.inject(r, 6, "soft_constraint_cost=" + format_double(solution.soft_constraint_cost));
    out.inject(r, 7, "annealing_cost=" + format_double(solution.annealing_cost));
    out.inject(r, 8, std::string("pareto_nondominated=")
        + (solution.pareto_nondominated ? "true" : "false"));
    ++r;

    out.inject(r, 0, "SEARCH");
    out.inject(r, 1, "requested_variations=" + std::to_string(solution.requested_variations));
    out.inject(r, 2, "returned_variations=" + std::to_string(solution.returned_variations));
    out.inject(r, 3, "annealing_runs=" + std::to_string(solution.annealing_runs));
    out.inject(r, 4, "annealing_proposals=" + std::to_string(solution.annealing_proposals));
    if(config){
        out.inject(r, 5, "iterations_per_run=" + std::to_string(config->annealing_iterations));
        out.inject(r, 6, "seed=" + std::to_string(config->seed));
        out.inject(r, 7, "requirement_violation_weight=" + format_double(config->requirement_violation_weight));
        out.inject(r, 8, "fairness_weight=" + format_double(config->fairness_weight));
        out.inject(r, 9, "preference_weight=" + format_double(config->preference_weight));
        out.inject(r, 10, "fairness_metric=" + config->fairness_metric);
    }
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
        " assigns mutable schedule cells, and emits auditable schedule variations. Hard Constraint rows are"
        " lexicographically prioritized coverage rules and are never knowingly worsened in returned schedules."
        " Soft Constraint rows carry their own weight in column 3 as Weight=<value>."
        " Supported soft constraints are max_consecutive_remote, exclusivity (for example XA XOR XB), and"
        " max_weekly_remote (for example XG = 2)."
        "\n\n"
        "For exclusivity, XOR means mutual exclusion (at most one listed staff member present), not Boolean exactly-one."
        " Presence includes assigned/fixed onsite plus Prim and Sec. Repeated Date headers define week blocks for"
        " max_weekly_remote. Vacation and holiday cells neither count toward nor terminate max_consecutive_remote runs."
        "\n\n"
        "The solver first proves the lexicographically minimal hard-constraint violation vector by exhaustive per-day"
        " enumeration. Simulated annealing may temporarily explore worse hard states, with RequirementViolationWeight"
        " controlling that barrier, but only schedules on the proven hard-optimal surface are eligible for output."
        " Final annealing_cost is RequirementViolationWeight * unavoidable hard deficits + FairnessWeight * fairness"
        " + PreferenceWeight * Remote-to-onsite overrides + sum(inline soft weight * soft penalty)."
        "\n\n"
        "NVariations requests that many distinct hard-optimal output tables. If annealing converges to duplicates,"
        " deterministic hard-optimal diversification fills the request whenever that many distinct schedules exist."
        " AnnealingIterations is the exact number of proposals per annealing run; SEARCH rows and metadata report the"
        " number of runs and proposals actually executed.";

    out.args.emplace_back();
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "RequirementRegex";
    out.args.back().desc =
        "Legacy-named regular expression identifying Hard Constraint / Soft Constraint labels in column 0."
        " Constraint-looking rows excluded by this regex cause an error rather than being silently ignored.";
    out.args.back().default_val = "^(Hard|Soft)\\s+Constraint|^Requirement";
    out.args.back().expected = true;
    out.args.back().examples = { "^(Hard|Soft)\\s+Constraint|^Requirement", "Constraint" };

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
    out.args.back().desc =
        "Comma-separated vacation/non-working terms. These are fixed and skipped by remote-run counting.";
    out.args.back().default_val = "Vac";
    out.args.back().expected = true;
    out.args.back().examples = { "Vac", "Vac,Leave", "Vacation,PTO" };

    out.args.emplace_back();
    out.args.back().name = "ImmutableTerms";
    out.args.back().desc =
        "Comma-separated fixed, non-coverage terms. Prim and Sec additionally count as present for exclusivity.";
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
    out.args.back().desc =
        "Comma-separated overrideable remote-preference terms. In exact mode, exact spelling resolves case-only"
        " overlaps before case-insensitive fallback, so default Remote is distinct from final/fixed remote.";
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
    out.args.back().examples = { "remote", "home" };

    out.args.emplace_back();
    out.args.back().name = "TermMatchMode";
    out.args.back().desc = "Whether term lists use exact or case-insensitive regex matching.";
    out.args.back().default_val = "exact";
    out.args.back().expected = true;
    out.args.back().examples = { "exact", "regex" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "NVariations";
    out.args.back().desc =
        "Number of distinct hard-optimal schedule variations requested. The solver emits exactly this many when"
        " at least this many distinct hard-optimal schedules exist; otherwise it emits all available and warns.";
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
    out.args.back().desc = "Weight applied to the fairness objective and final annealing cost.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "1", "2.5" };

    out.args.emplace_back();
    out.args.back().name = "PreferenceWeight";
    out.args.back().desc = "Weight applied to each Remote -> onsite preference override and final annealing cost.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "1", "2.5" };

    out.args.emplace_back();
    out.args.back().name = "RequirementViolationWeight";
    out.args.back().desc =
        "Penalty applied to temporary hard-constraint deficits during annealing and to unavoidable deficits in the"
        " reported final cost. It does not permit an output schedule worse than the proven lexicographic hard optimum.";
    out.args.back().default_val = "1000.0";
    out.args.back().expected = true;
    out.args.back().examples = { "10", "1000", "10000" };

    out.args.emplace_back();
    out.args.back().name = "AnnealingIterations";
    out.args.back().desc =
        "Exact number of simulated-annealing proposals per search run. Zero disables annealing; deterministic"
        " hard-optimal diversification can still satisfy NVariations.";
    out.args.back().default_val = "100000";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "10000", "100000", "500000" };

    out.args.emplace_back();
    out.args.back().name = "Seed";
    out.args.back().desc = "Seed for deterministic simulated annealing. It has no effect when AnnealingIterations=0.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "12345" };

    out.notes.emplace_back(
        "Soft-constraint weights are table-driven. For example: "
        "'Soft Constraint 2, exclusivity, XA XOR XB, Weight=1.5'.");
    out.notes.emplace_back(
        "Hard constraints are fail-closed. If a declared Hard Constraint / Soft Constraint row is excluded by"
        " RequirementRegex, the operation throws rather than optimizing an unconstrained schedule.");
    out.notes.emplace_back(
        "Report rows include FLAG for each hard deficit, SOFT_FLAG for each soft violation, OVERRIDE with a"
        " counterfactual explanation, TALLY, SOFT_OBJECTIVE, OBJECTIVES, and SEARCH. OBJECTIVES states whether the"
        " schedule is hard_optimal and Pareto-nondominated among explored hard-optimal schedules.");
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

    const auto HolidayTermsStr = OptArgs.getValueStr("HolidayTerms").value();
    const auto VacationTermsStr = OptArgs.getValueStr("VacationTerms").value();
    const auto ImmutableTermsStr = OptArgs.getValueStr("ImmutableTerms").value();
    const auto OnsiteTermsStr = OptArgs.getValueStr("OnsiteTerms").value();
    const auto RemotePreferenceTermsStr = OptArgs.getValueStr("RemotePreferenceTerms").value();
    const auto UndecidedTermsStr = OptArgs.getValueStr("UndecidedTerms").value();
    const auto RemoteTermsStr = OptArgs.getValueStr("RemoteTerms").value();

    TermLists terms;
    terms.holiday     = SplitStringToVector(HolidayTermsStr, ',', 'd');
    terms.vacation    = SplitStringToVector(VacationTermsStr, ',', 'd');
    terms.immutable   = SplitStringToVector(ImmutableTermsStr, ',', 'd');
    terms.onsite      = SplitStringToVector(OnsiteTermsStr, ',', 'd');
    terms.remote_pref = SplitStringToVector(RemotePreferenceTermsStr, ',', 'd');
    terms.undecided   = SplitStringToVector(UndecidedTermsStr, ',', 'd');
    terms.remote      = SplitStringToVector(RemoteTermsStr, ',', 'd');

    const auto mode = normalize_identifier(TermMatchModeStr);
    if(mode == "exact") terms.regex_mode = false;
    else if(mode == "regex") terms.regex_mode = true;
    else throw std::runtime_error("TermMatchMode must be 'exact' or 'regex'");

    SolverConfig config;
    config.fairness_metric = to_lower(trim(OptArgs.getValueStr("FairnessMetric").value()));
    config.fairness_weight = parse_nonnegative_double_arg(
        "FairnessWeight", OptArgs.getValueStr("FairnessWeight").value());
    config.preference_weight = parse_nonnegative_double_arg(
        "PreferenceWeight", OptArgs.getValueStr("PreferenceWeight").value());
    config.requirement_violation_weight = parse_nonnegative_double_arg(
        "RequirementViolationWeight", OptArgs.getValueStr("RequirementViolationWeight").value());
    config.annealing_iterations = parse_nonnegative_int_arg(
        "AnnealingIterations", OptArgs.getValueStr("AnnealingIterations").value());
    config.n_variations = parse_nonnegative_int_arg(
        "NVariations", OptArgs.getValueStr("NVariations").value());
    config.seed = parse_signed_int_arg("Seed", OptArgs.getValueStr("Seed").value());

    if(config.n_variations < 1){
        throw std::runtime_error("NVariations must be at least 1");
    }
    if(!(config.fairness_metric == "range"
         || config.fairness_metric == "variance"
         || config.fairness_metric == "gini")){
        throw std::runtime_error("FairnessMetric must be 'range', 'variance', or 'gini'");
    }

    validate_exact_term_lists(terms);

    YLOGINFO("ScheduleCoverage configuration: TableSelection='" << TableSelectionStr
        << "', RequirementRegex='" << RequirementRegexStr
        << "', HeaderRegex='" << HeaderRegexStr
        << "', TermMatchMode='" << mode
        << "', NVariations=" << config.n_variations
        << ", FairnessMetric='" << config.fairness_metric
        << "', FairnessWeight=" << config.fairness_weight
        << ", PreferenceWeight=" << config.preference_weight
        << ", RequirementViolationWeight=" << config.requirement_violation_weight
        << ", AnnealingIterations=" << config.annealing_iterations
        << ", Seed=" << config.seed);

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

        const auto schedule = parse_schedule(t, RequirementRegexStr, HeaderRegexStr, terms);

        // Coverage scheduling without any hard coverage rule is unsafe: it trivially permits all-remote output.
        // Fail closed rather than silently generating a schedule that cannot satisfy the operation's purpose.
        if(schedule.requirements.empty()){
            throw std::runtime_error(
                "No hard constraints were parsed. Refusing to generate an unconstrained coverage schedule; "
                "check RequirementRegex and the Hard Constraint rows");
        }

        const auto model = build_constraint_model(schedule);
        const auto solutions = produce_variations(schedule, model, config);
        if(solutions.empty()){
            throw std::runtime_error("ScheduleCoverage did not produce any schedule variations");
        }

        const std::string base_label =
            (t.metadata.count("TableLabel") != 0) ? t.metadata[ "TableLabel" ] : "unspecified";

        for(size_t i = 0; i < solutions.size(); ++i){
            if(!solutions[i].hard_optimal){
                throw std::runtime_error("Internal ScheduleCoverage error: attempted to render a non-hard-optimal solution");
            }

            tables::table2 rendered = render_variation(t, schedule, solutions[i], &config);
            rendered.metadata = coalesce_metadata_for_basic_table(rendered.metadata, meta_evolve::iterate);

            const std::string suffix =
                " [schedule coverage variation " + std::to_string(i + 1) + "]";
            const std::string label = base_label + suffix;
            rendered.metadata["TableLabel"] = label;
            rendered.metadata["NormalizedTableLabel"] = X(label);
            rendered.metadata["ScheduleVariation"] = std::to_string(i + 1);

            rendered.metadata["ScheduleCoverageRequestedVariations"] = std::to_string(config.n_variations);
            rendered.metadata["ScheduleCoverageReturnedVariations"] = std::to_string(solutions.size());
            rendered.metadata["ScheduleCoverageFairnessMetric"] = config.fairness_metric;
            rendered.metadata["ScheduleCoverageFairnessWeight"] = format_double(config.fairness_weight);
            rendered.metadata["ScheduleCoveragePreferenceWeight"] = format_double(config.preference_weight);
            rendered.metadata["ScheduleCoverageRequirementViolationWeight"] =
                format_double(config.requirement_violation_weight);
            rendered.metadata["ScheduleCoverageAnnealingIterations"] =
                std::to_string(config.annealing_iterations);
            rendered.metadata["ScheduleCoverageAnnealingRuns"] =
                std::to_string(solutions[i].annealing_runs);
            rendered.metadata["ScheduleCoverageAnnealingProposals"] =
                std::to_string(solutions[i].annealing_proposals);
            rendered.metadata["ScheduleCoverageSeed"] = std::to_string(config.seed);

            rendered.metadata["ScheduleCoverageRequirementRegex"] = RequirementRegexStr;
            rendered.metadata["ScheduleCoverageHeaderRegex"] = HeaderRegexStr;
            rendered.metadata["ScheduleCoverageTermMatchMode"] = mode;
            rendered.metadata["ScheduleCoverageHolidayTerms"] = HolidayTermsStr;
            rendered.metadata["ScheduleCoverageVacationTerms"] = VacationTermsStr;
            rendered.metadata["ScheduleCoverageImmutableTerms"] = ImmutableTermsStr;
            rendered.metadata["ScheduleCoverageOnsiteTerms"] = OnsiteTermsStr;
            rendered.metadata["ScheduleCoverageRemotePreferenceTerms"] = RemotePreferenceTermsStr;
            rendered.metadata["ScheduleCoverageUndecidedTerms"] = UndecidedTermsStr;
            rendered.metadata["ScheduleCoverageRemoteTerms"] = RemoteTermsStr;
            rendered.metadata["ScheduleCoverageTableSelection"] = TableSelectionStr;

            rendered.metadata["ScheduleCoverageFairness"] = format_double(solutions[i].fairness);
            rendered.metadata["ScheduleCoverageOverrides"] = std::to_string(solutions[i].overrides);
            rendered.metadata["ScheduleCoverageHardViolations"] = join(solutions[i].violation_sum, ",");
            rendered.metadata["ScheduleCoverageHardViolationUnits"] =
                std::to_string(solutions[i].hard_violation_units);
            rendered.metadata["ScheduleCoverageHardOptimal"] =
                solutions[i].hard_optimal ? "true" : "false";
            rendered.metadata["ScheduleCoverageSoftConstraintCost"] =
                format_double(solutions[i].soft_constraint_cost);
            rendered.metadata["ScheduleCoverageAnnealingCost"] =
                format_double(solutions[i].annealing_cost);
            rendered.metadata["ScheduleCoverageParetoNondominated"] =
                solutions[i].pareto_nondominated ? "true" : "false";

            auto st = std::make_shared<Sparse_Table>();
            st->table = std::move(rendered);
            DICOM_data.table_data.emplace_back(std::move(st));
        }

        YLOGINFO("ScheduleCoverage: emitted " << solutions.size()
            << " schedule variation(s), all on hard-optimal violation vector=["
            << join(solutions.front().violation_sum, ",") << "]");
    }

    return true;
}
