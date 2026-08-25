// OptimizeSchedule.cc - Automated onsite/remote schedule optimization.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <numeric>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "Explicator.h"
#include "YgorLog.h"

#include "../Regex_Selectors.h"
#include "../Structs.h"
#include "../Tables.h"
#include "OptimizeSchedule.h"

namespace {

constexpr double comparison_tolerance = 1.0e-12;
constexpr const char *report_marker = "Schedule Optimizer Report";

std::string trim(const std::string &s){
    const auto is_space = [](unsigned char c){ return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; };
    std::size_t a = 0;
    while(a < s.size() && is_space(static_cast<unsigned char>(s[a]))) ++a;
    std::size_t b = s.size();
    while(a < b && is_space(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string fold(const std::string &s){
    auto out = trim(s);
    for(auto &c : out){
        const auto u = static_cast<unsigned char>(c);
        if(u >= 'A' && u <= 'Z') c = static_cast<char>(u - 'A' + 'a');
    }
    return out;
}

std::string canonical_status(const std::string &s){
    auto out = fold(s);
    if(out == "onsite*") out = "onsite";
    return out;
}

std::string number(double x){
    std::ostringstream os;
    os.imbue(std::locale::classic());
    os << std::setprecision(std::numeric_limits<double>::max_digits10) << x;
    return os.str();
}

std::string status_set_text(const std::set<std::string> &statuses){
    std::string out;
    for(const auto &status : statuses){
        if(!out.empty()) out += '|';
        out += status == "onsite" ? "Onsite" : (status == "remote" ? "Remote" : status);
    }
    return out;
}

uint64_t parse_u64(const std::string &text, const std::string &what){
    const auto s = trim(text);
    if(s.empty() || !std::all_of(s.begin(), s.end(), [](unsigned char c){ return c >= '0' && c <= '9'; })){
        throw std::invalid_argument(what + " must be an unsigned decimal integer, got '" + text + "'");
    }
    uint64_t n = 0;
    for(const char c : s){
        const auto d = static_cast<uint64_t>(c - '0');
        if(n > (std::numeric_limits<uint64_t>::max() - d) / 10U){
            throw std::invalid_argument(what + " overflows an unsigned 64-bit integer: '" + text + "'");
        }
        n = n * 10U + d;
    }
    return n;
}

double parse_decimal(const std::string &text, const std::string &what){
    static const std::regex grammar(R"(^[+-]?(?:(?:[0-9]+(?:\.[0-9]*)?)|(?:\.[0-9]+))(?:[eE][+-]?[0-9]+)?$)");
    const auto s = trim(text);
    if(!std::regex_match(s, grammar)) throw std::invalid_argument(what + " must be a finite decimal number, got '" + text + "'");
    std::istringstream is(s);
    is.imbue(std::locale::classic());
    double v = 0.0;
    is >> v;
    if(!is || !is.eof() || !std::isfinite(v)) throw std::invalid_argument(what + " must be finite, got '" + text + "'");
    return v;
}

struct CivilDate {
    int year = 0;
    int month = 0;
    int day = 0;
    int64_t key = 0;
    int iso_year = 0;
    int iso_week = 0;
    std::string iso;
};

bool leap(int y){ return ((y % 4) == 0 && (y % 100) != 0) || ((y % 400) == 0); }

int64_t days_from_civil(int y, unsigned m, unsigned d){
    y -= (m <= 2);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned adjusted_month = m > 2 ? m - 3U : m + 9U;
    const unsigned doy = (153U * adjusted_month + 2U) / 5U + d - 1U;
    const unsigned doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
    return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

std::array<int, 3> civil_from_days(int64_t z){
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460U + doe / 36524U - doe / 146096U) / 365U;
    int y = static_cast<int>(yoe) + static_cast<int>(era) * 400;
    const unsigned doy = doe - (365U * yoe + yoe / 4U - yoe / 100U);
    const unsigned mp = (5U * doy + 2U) / 153U;
    const unsigned d = doy - (153U * mp + 2U) / 5U + 1U;
    const unsigned m = mp < 10 ? mp + 3U : mp - 9U;
    y += (m <= 2);
    return {{y, static_cast<int>(m), static_cast<int>(d)}};
}

int weekday_monday0(int64_t key){
    int v = static_cast<int>((key + 3) % 7);
    if(v < 0) v += 7;
    return v;
}

CivilDate make_date(int y, int m, int d, const std::string &source){
    if(y < 1 || y > 9999 || m < 1 || m > 12) throw std::invalid_argument("invalid Gregorian date '" + source + "'");
    static const int month_days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    const int md = month_days[m - 1] + ((m == 2 && leap(y)) ? 1 : 0);
    if(d < 1 || d > md) throw std::invalid_argument("invalid Gregorian date '" + source + "'");
    CivilDate out;
    out.year = y; out.month = m; out.day = d; out.key = days_from_civil(y, static_cast<unsigned>(m), static_cast<unsigned>(d));
    const int wd = weekday_monday0(out.key);
    const auto thursday = civil_from_days(out.key + (3 - wd));
    out.iso_year = thursday[0];
    const int64_t jan4 = days_from_civil(out.iso_year, 1, 4);
    const int64_t week1 = jan4 - weekday_monday0(jan4);
    out.iso_week = static_cast<int>((out.key - week1) / 7 + 1);
    std::ostringstream os;
    os.imbue(std::locale::classic());
    os << std::setfill('0') << std::setw(4) << y << '-' << std::setw(2) << m << '-' << std::setw(2) << d;
    out.iso = os.str();
    return out;
}

bool try_parse_date(const std::string &source, CivilDate &out, std::string &error){
    const auto s = trim(source);
    std::smatch m;
    try {
        static const std::regex iso(R"(^([0-9]{4})-([0-9]{2})-([0-9]{2})$)", std::regex::icase);
        if(std::regex_match(s, m, iso)){
            out = make_date(static_cast<int>(parse_u64(m[1], "year")), static_cast<int>(parse_u64(m[2], "month")), static_cast<int>(parse_u64(m[3], "day")), source);
            return true;
        }
        static const std::regex words(R"(^(?:([A-Za-z]+)\s*,\s*)?([A-Za-z]+)\s+([0-9]{1,2})\s*,\s*([0-9]{4})$)");
        if(!std::regex_match(s, m, words)) return false;
        static const std::map<std::string, int> months = {
            {"jan",1},{"january",1},{"feb",2},{"february",2},{"mar",3},{"march",3},{"apr",4},{"april",4},
            {"may",5},{"jun",6},{"june",6},{"jul",7},{"july",7},{"aug",8},{"august",8},{"sep",9},{"sept",9},
            {"september",9},{"oct",10},{"october",10},{"nov",11},{"november",11},{"dec",12},{"december",12}
        };
        const auto mi = months.find(fold(m[2]));
        if(mi == months.end()) throw std::invalid_argument("unknown English month in date '" + source + "'");
        out = make_date(static_cast<int>(parse_u64(m[4], "year")), mi->second, static_cast<int>(parse_u64(m[3], "day")), source);
        if(m[1].matched){
            static const std::map<std::string, int> weekdays = {
                {"mon",0},{"monday",0},{"tue",1},{"tues",1},{"tuesday",1},{"wed",2},{"wednesday",2},
                {"thu",3},{"thur",3},{"thurs",3},{"thursday",3},{"fri",4},{"friday",4},{"sat",5},{"saturday",5},
                {"sun",6},{"sunday",6}
            };
            const auto wi = weekdays.find(fold(m[1]));
            if(wi == weekdays.end()) throw std::invalid_argument("unknown weekday in date '" + source + "'");
            if(wi->second != weekday_monday0(out.key)) throw std::invalid_argument("weekday does not match Gregorian date '" + source + "'");
        }
        return true;
    } catch(const std::invalid_argument &e){ error = e.what(); return false; }
}

bool looks_date_like(const std::string &source){
    const auto s = trim(source);
    static const std::regex numeric(R"(^[0-9]{4}\s*[-/]\s*[0-9]{1,2}\s*[-/]\s*[0-9]{1,2}$)");
    static const std::regex english(R"(^(?:[A-Za-z]+\s*,\s*)?[A-Za-z]+\s+[0-9]{1,2}\s*,?\s+[0-9]{4}$)");
    return std::regex_match(s, numeric) || std::regex_match(s, english);
}

enum class ConstraintKind { Minimum, Group, Consecutive, Exclusivity, Weekly, FairRemote, FairOverrides };

struct Constraint {
    ConstraintKind kind = ConstraintKind::Minimum;
    int64_t row = 0;
    std::string label;
    std::string name;
    double weight = 0.0;
    std::set<std::string> statuses;
    std::vector<std::size_t> staff;
    uint64_t requirement = 0;
    std::vector<std::pair<std::size_t, uint64_t>> weekly_limits;
};

struct Cell {
    std::string original;
    std::string status;
    bool mutable_cell = false;
    bool preference = false;
    std::size_t variable = std::numeric_limits<std::size_t>::max();
};

struct Staff { std::string label; std::string key; int64_t column = 0; };

struct Day {
    CivilDate date;
    std::string date_text;
    int64_t row = 0;
    bool active = true;
    std::string exclusion;
    std::vector<Cell> cells;
};

struct Variable { std::size_t day = 0; std::size_t staff = 0; bool preference = false; int64_t row = 0; int64_t col = 0; };
struct Feasibility { std::size_t day = 0; std::size_t constraint = 0; uint64_t maximum = 0; };
struct Week { int year = 0; int week = 0; std::vector<std::size_t> days; };

struct Problem {
    std::vector<Staff> staff;
    std::vector<Day> days;
    std::vector<Variable> variables;
    std::vector<std::vector<std::size_t>> vars_by_day;
    std::vector<std::vector<std::size_t>> vars_by_staff;
    std::vector<Week> active_weeks;
    std::vector<Constraint> constraints;
    std::vector<Feasibility> feasibility;
};

struct RawConstraint { int64_t row = 0; int64_t base_col = 0; std::vector<std::string> fields; };

std::string row_error(int64_t row, int64_t col, const std::string &message){
    return "OptimizeSchedule table row " + std::to_string(row) + ", column " + std::to_string(col) + ": " + message;
}

std::vector<std::string> split_statuses(const std::string &text, int64_t row, int64_t col){
    std::vector<std::string> out;
    std::size_t pos = 0;
    while(true){
        const auto next = text.find('|', pos);
        auto v = canonical_status(text.substr(pos, next == std::string::npos ? next : next - pos));
        if(v.empty()) throw std::invalid_argument(row_error(row, col, "statuses policy contains an empty status"));
        out.push_back(v);
        if(next == std::string::npos) break;
        pos = next + 1;
    }
    return out;
}

std::size_t lookup_staff(const Problem &p, const std::string &name, int64_t row, int64_t col){
    const auto key = fold(name);
    for(std::size_t i = 0; i < p.staff.size(); ++i) if(p.staff[i].key == key) return i;
    throw std::invalid_argument(row_error(row, col, "unknown staff identifier '" + trim(name) + "'"));
}

std::vector<std::string> regex_list(const std::string &expr, const std::string &delimiter){
    std::vector<std::string> out;
    const std::regex re(delimiter == "," ? "\\s*,\\s*" : "\\s+" + delimiter + "\\s+", std::regex::icase);
    std::sregex_token_iterator it(expr.begin(), expr.end(), re, -1), end;
    for(; it != end; ++it) out.push_back(trim(*it));
    return out;
}

void reject_duplicate_staff(const std::vector<std::size_t> &staff, int64_t row, int64_t col){
    std::set<std::size_t> unique(staff.begin(), staff.end());
    if(unique.size() != staff.size()) throw std::invalid_argument(row_error(row, col, "constraint expression repeats a staff identifier"));
}

Constraint parse_constraint(const RawConstraint &raw, const Problem &p){
    const auto field = [&](std::size_t i) -> std::string { return i < raw.fields.size() ? raw.fields[i] : ""; };
    Constraint c;
    c.row = raw.row;
    c.label = trim(field(1));
    c.weight = parse_decimal(field(2), row_error(raw.row, raw.base_col + 2, "constraint weight"));
    if(c.weight < 0.0) throw std::invalid_argument(row_error(raw.row, raw.base_col + 2, "constraint weight must be non-negative"));
    std::smatch m;
    const auto type = trim(field(1));
    static const std::regex named(R"(^\s*(group|exclusivity)\s*\(\s*([^()]+)\s*\)\s*$)", std::regex::icase);
    if(std::regex_match(type, m, named)){
        c.name = trim(m[2]);
        if(c.name.empty()) throw std::invalid_argument(row_error(raw.row, raw.base_col + 1, "constraint name cannot be empty"));
        c.kind = fold(m[1]) == "group" ? ConstraintKind::Group : ConstraintKind::Exclusivity;
    }else{
        const auto t = fold(type);
        if(t == "minimum_onsite") c.kind = ConstraintKind::Minimum;
        else if(t == "max_consecutive_remote") c.kind = ConstraintKind::Consecutive;
        else if(t == "max_weekly_remote") c.kind = ConstraintKind::Weekly;
        else if(t == "fairness_remote") c.kind = ConstraintKind::FairRemote;
        else if(t == "fairness_overrides") c.kind = ConstraintKind::FairOverrides;
        else throw std::invalid_argument(row_error(raw.row, raw.base_col + 1, "unknown constraint type '" + type + "'"));
    }
    if(c.kind == ConstraintKind::Minimum || c.kind == ConstraintKind::Group) c.statuses = {"onsite"};
    if(c.kind == ConstraintKind::Exclusivity) c.statuses = {"onsite", "prim", "sec"};
    if(c.kind == ConstraintKind::Consecutive || c.kind == ConstraintKind::Weekly || c.kind == ConstraintKind::FairRemote) c.statuses = {"remote"};
    for(std::size_t i = 4; i < raw.fields.size(); ++i){
        if(trim(raw.fields[i]).empty()) continue;
        const auto eq = raw.fields[i].find('=');
        if(eq == std::string::npos) throw std::invalid_argument(row_error(raw.row, raw.base_col + static_cast<int64_t>(i), "policy field must be key=value"));
        const auto key = fold(raw.fields[i].substr(0, eq));
        if(key != "statuses") throw std::invalid_argument(row_error(raw.row, raw.base_col + static_cast<int64_t>(i), "unknown policy key '" + trim(raw.fields[i].substr(0, eq)) + "'"));
        if(c.kind == ConstraintKind::FairOverrides) throw std::invalid_argument(row_error(raw.row, raw.base_col + static_cast<int64_t>(i), "fairness_overrides does not accept statuses"));
        c.statuses.clear();
        for(auto &s : split_statuses(raw.fields[i].substr(eq + 1), raw.row, raw.base_col + static_cast<int64_t>(i))) c.statuses.insert(std::move(s));
    }
    const auto expr = trim(field(3));
    const bool no_expr = c.kind == ConstraintKind::FairRemote || c.kind == ConstraintKind::FairOverrides;
    if(no_expr){
        if(!expr.empty()) throw std::invalid_argument(row_error(raw.row, raw.base_col + 3, "this constraint does not accept an expression"));
        return c;
    }
    if(expr.empty()) throw std::invalid_argument(row_error(raw.row, raw.base_col + 3, "constraint expression is required"));
    if(c.kind == ConstraintKind::Consecutive){
        c.requirement = parse_u64(expr, row_error(raw.row, raw.base_col + 3, "consecutive limit"));
        return c;
    }
    if(c.kind == ConstraintKind::Weekly){
        const auto assignments = regex_list(expr, ",");
        static const std::regex assignment(R"(^\s*([A-Za-z0-9_.-]+)\s*=\s*([0-9]+)\s*$)");
        for(const auto &a : assignments){
            if(!std::regex_match(a, m, assignment)) throw std::invalid_argument(row_error(raw.row, raw.base_col + 3, "expected '<staff> = <non-negative integer>' list"));
            c.weekly_limits.emplace_back(lookup_staff(p, m[1], raw.row, raw.base_col + 3),
                                         parse_u64(m[2], row_error(raw.row, raw.base_col + 3, "weekly remote limit")));
        }
        std::vector<std::size_t> ids;
        for(const auto &x : c.weekly_limits) ids.push_back(x.first);
        reject_duplicate_staff(ids, raw.row, raw.base_col + 3);
        return c;
    }
    const char *joiner = c.kind == ConstraintKind::Exclusivity ? "xor" : "or";
    const std::regex any_re(c.kind == ConstraintKind::Exclusivity
        ? R"(^\s*any\s+1\s+of\s+(.+)\s*$)" : R"(^\s*any\s+([0-9]+)\s+of\s+(.+)\s*$)", std::regex::icase);
    if(!std::regex_match(expr, m, any_re)) throw std::invalid_argument(row_error(raw.row, raw.base_col + 3, "malformed any-of constraint expression"));
    if(c.kind == ConstraintKind::Exclusivity){ c.requirement = 1; }
    else {
        c.requirement = parse_u64(m[1], row_error(raw.row, raw.base_col + 3, "coverage requirement"));
        if(c.requirement == 0) throw std::invalid_argument(row_error(raw.row, raw.base_col + 3, "coverage requirement must be positive"));
    }
    const std::string candidates = c.kind == ConstraintKind::Exclusivity ? m[1].str() : m[2].str();
    const bool all_candidates = c.kind != ConstraintKind::Exclusivity && fold(candidates) == "all";
    if(all_candidates){
        c.staff.resize(p.staff.size());
        std::iota(c.staff.begin(), c.staff.end(), 0);
    }else{
        for(const auto &name : regex_list(candidates, joiner)){
            static const std::regex staff_grammar(R"(^[A-Za-z0-9_.-]+$)");
            if(!std::regex_match(name, staff_grammar)) throw std::invalid_argument(row_error(raw.row, raw.base_col + 3, "invalid staff identifier or delimiter in expression"));
            c.staff.push_back(lookup_staff(p, name, raw.row, raw.base_col + 3));
        }
    }
    reject_duplicate_staff(c.staff, raw.row, raw.base_col + 3);
    if(!all_candidates && c.staff.size() < 2) throw std::invalid_argument(row_error(raw.row, raw.base_col + 3, "explicit any-of and xor expressions require at least two staff identifiers"));
    if(c.staff.empty() || c.requirement > c.staff.size()) throw std::invalid_argument(row_error(raw.row, raw.base_col + 3, "requirement exceeds candidate staff count"));
    return c;
}

Problem parse_problem(const Sparse_Table &source, const std::set<std::string> &excluded_statuses){
    const auto &t = source.table;
    if(t.data.empty()) throw std::invalid_argument("OptimizeSchedule selected table is empty");
    for(const auto &cell : t.data) if(cell.val == report_marker) throw std::invalid_argument("selected table already contains a Schedule Optimizer Report; select the original template");
    const auto rb = t.min_max_row();
    const auto cb = t.min_max_col();
    Problem p;
    std::vector<RawConstraint> raw_constraints;
    bool in_block = false;
    bool have_header = false;
    int64_t header_date_column = 0;
    std::vector<std::pair<int64_t, std::string>> header_layout;
    std::set<int64_t> staff_columns;
    std::set<int64_t> seen_dates;
    int64_t previous_date = std::numeric_limits<int64_t>::min();
    for(int64_t row = rb.first;; ++row){
        int64_t first_col = 0;
        std::string first;
        bool nonempty = false;
        for(int64_t col = cb.first;; ++col){
            const auto v = t.value(row, col).value_or("");
            if(!trim(v).empty()){ first_col = col; first = v; nonempty = true; break; }
            if(col == cb.second) break;
        }
        if(!nonempty){
            if(row == rb.second) break;
            continue;
        }
        const auto first_key = fold(first);
        if(first_key == "constraint"){
            in_block = false;
            RawConstraint raw;
            raw.row = row; raw.base_col = first_col;
            for(int64_t col = first_col; col <= cb.second; ++col) raw.fields.push_back(t.value(row, col).value_or(""));
            raw_constraints.push_back(std::move(raw));
        }else if(first_key == "date"){
            in_block = true;
            std::vector<std::pair<int64_t, std::string>> layout;
            for(int64_t col = first_col + 1; col <= cb.second; ++col){
                const auto label = trim(t.value(row, col).value_or(""));
                if(!label.empty()) layout.emplace_back(col, label);
            }
            if(layout.empty()) throw std::invalid_argument(row_error(row, first_col, "Date header must declare at least one staff column"));
            static const std::regex staff_grammar(R"(^[A-Za-z0-9_.-]+$)");
            std::set<std::string> keys;
            for(const auto &entry : layout){
                if(!std::regex_match(entry.second, staff_grammar)) throw std::invalid_argument(row_error(row, entry.first, "staff label must match [A-Za-z0-9_.-]+"));
                if(!keys.insert(fold(entry.second)).second) throw std::invalid_argument(row_error(row, entry.first, "duplicate case-insensitive staff label '" + entry.second + "'"));
            }
            if(!have_header){
                have_header = true; header_date_column = first_col; header_layout = layout;
                for(const auto &entry : layout){ p.staff.push_back({entry.second, fold(entry.second), entry.first}); staff_columns.insert(entry.first); }
            }else{
                if(first_col != header_date_column) throw std::invalid_argument(row_error(row, first_col, "Date column differs from the first Date header"));
                if(layout.size() != header_layout.size()) throw std::invalid_argument(row_error(row, first_col, "schedule header differs from the first Date header"));
                for(std::size_t i = 0; i < layout.size(); ++i){
                    if(layout[i].first != header_layout[i].first || fold(layout[i].second) != fold(header_layout[i].second))
                        throw std::invalid_argument(row_error(row, layout[i].first, "schedule header differs from the first Date header"));
                }
            }
        }else{
            CivilDate date;
            std::string date_error;
            const bool valid_date = try_parse_date(first, date, date_error);
            if(!in_block){
                if(valid_date || !date_error.empty() || looks_date_like(first))
                    throw std::invalid_argument(row_error(row, first_col, date_error.empty() ? "schedule date-like row appears before a Date header or is malformed" : date_error));
            }else if(valid_date){
                if(first_col != header_date_column) throw std::invalid_argument(row_error(row, first_col, "date is not in the Date header column"));
                if(!seen_dates.insert(date.key).second) throw std::invalid_argument(row_error(row, first_col, "duplicate schedule date '" + first + "'"));
                if(date.key <= previous_date) throw std::invalid_argument(row_error(row, first_col, "schedule dates must be strictly increasing"));
                previous_date = date.key;
                Day day;
                day.date = date; day.date_text = first; day.row = row;
                for(std::size_t s = 0; s < p.staff.size(); ++s){
                    const auto val = t.value(row, p.staff[s].column).value_or("");
                    if(trim(val).empty()) throw std::invalid_argument(row_error(row, p.staff[s].column, "missing staff status cell"));
                    Cell cell;
                    cell.original = val; cell.status = canonical_status(val);
                    cell.mutable_cell = cell.status == "x" || cell.status == "pref";
                    cell.preference = cell.status == "pref";
                    day.cells.push_back(std::move(cell));
                }
                for(int64_t col = cb.first; col <= cb.second; ++col){
                    if(col == first_col || staff_columns.count(col) != 0) continue;
                    if(!trim(t.value(row, col).value_or("")).empty()) throw std::invalid_argument(row_error(row, col, "populated cell lies outside declared staff columns"));
                }
                bool unanimous = !excluded_statuses.empty();
                const auto status = day.cells.front().status;
                for(const auto &cell : day.cells) unanimous = unanimous && cell.status == status;
                if(unanimous && excluded_statuses.count(status) != 0){ day.active = false; day.exclusion = status; }
                p.days.push_back(std::move(day));
            }else{
                bool staff_populated = false;
                for(const auto &staff : p.staff) staff_populated = staff_populated || !trim(t.value(row, staff.column).value_or("")).empty();
                if(staff_populated || !date_error.empty()) throw std::invalid_argument(row_error(row, first_col, date_error.empty() ? "non-date row has populated staff columns inside a schedule block" : date_error));
            }
        }
        if(row == rb.second) break;
    }
    if(!have_header) throw std::invalid_argument("OptimizeSchedule found no Date schedule header");
    if(p.days.empty()) throw std::invalid_argument("OptimizeSchedule found no schedule days");
    p.vars_by_day.resize(p.days.size());
    p.vars_by_staff.resize(p.staff.size());
    std::size_t active_days = 0;
    for(std::size_t d = 0; d < p.days.size(); ++d){
        if(p.days[d].active) ++active_days;
        for(std::size_t s = 0; s < p.staff.size(); ++s){
            auto &cell = p.days[d].cells[s];
            if(cell.mutable_cell){
                cell.variable = p.variables.size();
                p.variables.push_back({d, s, cell.preference, p.days[d].row, p.staff[s].column});
                p.vars_by_day[d].push_back(cell.variable);
                p.vars_by_staff[s].push_back(cell.variable);
            }
        }
    }
    if(active_days == 0) throw std::invalid_argument("ExcludeUnanimousStatuses leaves no active schedule days");
    if(p.variables.empty()) throw std::invalid_argument("OptimizeSchedule found no mutable x or Pref schedule cells");
    std::map<std::pair<int, int>, std::vector<std::size_t>> active_weeks;
    for(std::size_t d = 0; d < p.days.size(); ++d){
        if(p.days[d].active) active_weeks[{p.days[d].date.iso_year, p.days[d].date.iso_week}].push_back(d);
    }
    for(auto &entry : active_weeks) p.active_weeks.push_back({entry.first.first, entry.first.second, std::move(entry.second)});
    for(const auto &raw : raw_constraints) p.constraints.push_back(parse_constraint(raw, p));
    for(std::size_t ci = 0; ci < p.constraints.size(); ++ci){
        const auto &c = p.constraints[ci];
        if(c.kind != ConstraintKind::Minimum && c.kind != ConstraintKind::Group) continue;
        for(std::size_t d = 0; d < p.days.size(); ++d){
            if(!p.days[d].active) continue;
            uint64_t maximum = 0;
            for(const auto s : c.staff){
                const auto &cell = p.days[d].cells[s];
                if(cell.mutable_cell){
                    if(c.statuses.count("onsite") != 0 || c.statuses.count("remote") != 0) ++maximum;
                }else if(c.statuses.count(cell.status) != 0) ++maximum;
            }
            if(maximum < c.requirement) p.feasibility.push_back({d, ci, maximum});
        }
    }
    return p;
}

struct DayViolation { std::size_t day = 0; std::size_t constraint = 0; uint64_t observed = 0; uint64_t required = 0; std::string description; };
struct WeeklyRecord { int year = 0; int week = 0; std::size_t staff = 0; std::size_t constraint = 0; uint64_t count = 0; uint64_t limit = 0; uint64_t excess = 0; };
struct FairDetail { std::size_t staff = 0; uint64_t numerator = 0; uint64_t denominator = 0; double ratio = 0.0; };
struct ConstraintDetail { std::vector<FairDetail> fairness; double mean = 0.0; double deviation = 0.0; };

struct Score {
    double objective = 0.0;
    std::vector<double> components;
    std::vector<DayViolation> violations;
    std::vector<WeeklyRecord> weekly;
    std::vector<ConstraintDetail> details;
};

std::string semantic_status(const Cell &cell, const std::vector<uint8_t> &assignment){
    if(!cell.mutable_cell) return cell.status;
    return assignment[cell.variable] ? "onsite" : "remote";
}

Score score_candidate(const Problem &p, const std::vector<uint8_t> &a, bool detailed){
    if(a.size() != p.variables.size()) throw std::logic_error("OptimizeSchedule internal assignment size mismatch");
    Score out;
    out.components.assign(p.constraints.size(), 0.0);
    if(detailed) out.details.resize(p.constraints.size());
    const double active_days = static_cast<double>(std::count_if(p.days.begin(), p.days.end(), [](const Day &d){ return d.active; }));
    for(std::size_t ci = 0; ci < p.constraints.size(); ++ci){
        const auto &c = p.constraints[ci];
        double component = 0.0;
        if(c.kind == ConstraintKind::Minimum || c.kind == ConstraintKind::Group || c.kind == ConstraintKind::Exclusivity){
            for(std::size_t d = 0; d < p.days.size(); ++d){
                if(!p.days[d].active) continue;
                uint64_t count = 0;
                std::vector<std::string> present;
                for(const auto s : c.staff){
                    if(c.statuses.count(semantic_status(p.days[d].cells[s], a)) != 0){ ++count; present.push_back(p.staff[s].label); }
                }
                if(c.kind == ConstraintKind::Exclusivity){
                    const uint64_t excess = count > 1 ? count - 1 : 0;
                    component += static_cast<double>(excess) / static_cast<double>(std::max<std::size_t>(1, c.staff.size() - 1));
                    if(detailed && c.weight > 0.0 && excess > 0){
                        std::ostringstream os; os << "simultaneously present:";
                        for(const auto &name : present) os << ' ' << name;
                        out.violations.push_back({d, ci, count, 1, os.str()});
                    }
                }else{
                    const uint64_t deficit = count < c.requirement ? c.requirement - count : 0;
                    component += static_cast<double>(deficit) / static_cast<double>(c.requirement);
                    if(detailed && c.weight > 0.0 && deficit > 0) out.violations.push_back({d, ci, count, c.requirement, "coverage deficit of " + std::to_string(deficit) + " staff using statuses=" + status_set_text(c.statuses)});
                }
            }
            component /= active_days;
        }else if(c.kind == ConstraintKind::Consecutive){
            uint64_t excess_total = 0;
            for(std::size_t s = 0; s < p.staff.size(); ++s){
                uint64_t run = 0;
                for(std::size_t d = 0; d < p.days.size(); ++d){
                    if(!p.days[d].active){ run = 0; continue; }
                    if(c.statuses.count(semantic_status(p.days[d].cells[s], a)) != 0){
                        ++run;
                        if(run > c.requirement){
                            ++excess_total;
                            if(detailed && c.weight > 0.0) out.violations.push_back({d, ci, run, c.requirement, p.staff[s].label + " remote-status run length " + std::to_string(run)});
                        }
                    }else run = 0;
                }
            }
            component = static_cast<double>(excess_total) / (active_days * static_cast<double>(p.staff.size()));
        }else if(c.kind == ConstraintKind::Weekly){
            uint64_t excess_total = 0, denominator = 0;
            for(const auto &limit : c.weekly_limits){
                for(const auto &week : p.active_weeks){
                    uint64_t count = 0;
                    denominator += week.days.size();
                    for(const auto d : week.days){
                        if(c.statuses.count(semantic_status(p.days[d].cells[limit.first], a)) != 0){
                            ++count;
                            if(detailed && c.weight > 0.0 && count > limit.second) out.violations.push_back({d, ci, count, limit.second, p.staff[limit.first].label + " exceeds weekly remote-status limit"});
                        }
                    }
                    const uint64_t excess = count > limit.second ? count - limit.second : 0;
                    excess_total += excess;
                    if(detailed) out.weekly.push_back({week.year, week.week, limit.first, ci, count, limit.second, excess});
                }
            }
            component = denominator == 0 ? 0.0 : static_cast<double>(excess_total) / static_cast<double>(denominator);
        }else if(c.kind == ConstraintKind::FairRemote || c.kind == ConstraintKind::FairOverrides){
            std::vector<double> ratios;
            ratios.reserve(p.staff.size());
            for(std::size_t s = 0; s < p.staff.size(); ++s){
                uint64_t numerator = 0, denominator = 0;
                for(const auto v : p.vars_by_staff[s]){
                    const auto &var = p.variables[v];
                    if(!p.days[var.day].active || (c.kind == ConstraintKind::FairOverrides && !var.preference)) continue;
                    ++denominator;
                    if(c.kind == ConstraintKind::FairOverrides ? (a[v] != 0) : (c.statuses.count(a[v] ? "onsite" : "remote") != 0)) ++numerator;
                }
                if(denominator != 0){
                    const double ratio = static_cast<double>(numerator) / static_cast<double>(denominator);
                    ratios.push_back(ratio);
                    if(detailed) out.details[ci].fairness.push_back({s, numerator, denominator, ratio});
                }
            }
            if(!ratios.empty()){
                double mean = std::accumulate(ratios.begin(), ratios.end(), 0.0) / static_cast<double>(ratios.size());
                double deviation = 0.0;
                for(const auto ratio : ratios) deviation += std::abs(ratio - mean);
                deviation /= static_cast<double>(ratios.size());
                if(detailed){ out.details[ci].mean = mean; out.details[ci].deviation = deviation; }
                component = c.kind == ConstraintKind::FairOverrides ? 0.5 * mean + 0.5 * deviation : deviation;
            }
        }
        if(!std::isfinite(component) || component < 0.0) throw std::runtime_error("OptimizeSchedule produced a non-finite component score");
        out.components[ci] = component;
        out.objective += c.weight * component;
    }
    if(!std::isfinite(out.objective)) throw std::runtime_error("OptimizeSchedule objective is non-finite");
    return out;
}

std::vector<double> pareto_vector(const Problem &p, const Score &score){
    std::vector<double> out;
    for(std::size_t i = 0; i < p.constraints.size(); ++i) if(p.constraints[i].weight > 0.0) out.push_back(score.components[i]);
    return out;
}

struct Entry { std::vector<uint8_t> assignment; Score score; std::vector<double> pareto; uint64_t sequence = 0; };

bool dominates(const Entry &a, const Entry &b){
    if(a.pareto.empty()) return false;
    bool strict = false;
    for(std::size_t i = 0; i < a.pareto.size(); ++i){
        if(a.pareto[i] > b.pareto[i] + comparison_tolerance) return false;
        strict = strict || a.pareto[i] + comparison_tolerance < b.pareto[i];
    }
    return strict;
}

bool better_entry(const Entry &a, const Entry &b){
    if(a.score.objective < b.score.objective) return true;
    if(b.score.objective < a.score.objective) return false;
    return a.assignment < b.assignment;
}

class CandidateStore {
    const Problem &problem;
    std::size_t archive_limit;
    std::size_t elite_limit;
    uint64_t sequence = 0;

    void prune_archive(){
        if(archive.size() <= archive_limit) return;
        const auto best = static_cast<std::size_t>(std::distance(archive.begin(), std::min_element(archive.begin(), archive.end(), better_entry)));
        std::vector<double> crowd(archive.size(), 0.0);
        if(!archive.empty() && !archive.front().pareto.empty()){
            for(std::size_t dim = 0; dim < archive.front().pareto.size(); ++dim){
                std::vector<std::size_t> order(archive.size()); std::iota(order.begin(), order.end(), 0);
                std::sort(order.begin(), order.end(), [&](std::size_t x, std::size_t y){
                    if(archive[x].pareto[dim] != archive[y].pareto[dim]) return archive[x].pareto[dim] < archive[y].pareto[dim];
                    return archive[x].sequence < archive[y].sequence;
                });
                const double span = archive[order.back()].pareto[dim] - archive[order.front()].pareto[dim];
                if(span > 0.0){
                    const double lo = archive[order.front()].pareto[dim];
                    const double hi = archive[order.back()].pareto[dim];
                    for(const auto i : order) if(archive[i].pareto[dim] == lo || archive[i].pareto[dim] == hi) crowd[i] = std::numeric_limits<double>::infinity();
                    for(std::size_t i = 1; i + 1 < order.size(); ++i){
                        if(std::isfinite(crowd[order[i]])) crowd[order[i]] += (archive[order[i + 1]].pareto[dim] - archive[order[i - 1]].pareto[dim]) / span;
                    }
                }
            }
        }
        std::size_t victim = archive.size();
        for(std::size_t i = 0; i < archive.size(); ++i){
            if(i == best) continue;
            if(victim == archive.size() || crowd[i] < crowd[victim] ||
               (crowd[i] == crowd[victim] && archive[i].sequence > archive[victim].sequence)) victim = i;
        }
        if(victim != archive.size()) archive.erase(archive.begin() + static_cast<std::ptrdiff_t>(victim));
    }

public:
    std::vector<Entry> archive;
    std::vector<Entry> elite;
    Entry best;
    bool have_best = false;

    CandidateStore(const Problem &p, std::size_t a, std::size_t e) : problem(p), archive_limit(a), elite_limit(e) {}

    void insert(std::vector<uint8_t> assignment, Score score){
        Entry incoming{std::move(assignment), std::move(score), {}, sequence++};
        incoming.pareto = pareto_vector(problem, incoming.score);
        const bool scalar_best = !have_best || better_entry(incoming, best);
        if(scalar_best){ best = incoming; have_best = true; }
        const auto duplicate = [&](const Entry &e){ return e.assignment == incoming.assignment; };
        if(std::none_of(elite.begin(), elite.end(), duplicate)){
            elite.push_back(incoming);
            std::sort(elite.begin(), elite.end(), better_entry);
            if(elite.size() > elite_limit) elite.pop_back();
        }
        if(std::any_of(archive.begin(), archive.end(), duplicate)) return;
        const bool archive_dominates = std::any_of(archive.begin(), archive.end(), [&](const Entry &e){ return dominates(e, incoming); });
        if(archive_dominates && !scalar_best) return;
        archive.erase(std::remove_if(archive.begin(), archive.end(), [&](const Entry &e){
            return dominates(incoming, e) || (scalar_best && dominates(e, incoming));
        }), archive.end());
        archive.push_back(std::move(incoming));
        prune_archive();
    }
};

uint64_t mix_seed(uint64_t base, uint64_t chain){
    uint64_t z = base + 0x9e3779b97f4a7c15ULL * (chain + 1U);
    z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31U);
}

double uniform01(std::mt19937_64 &rng){ return static_cast<double>(rng() >> 11U) * (1.0 / 9007199254740992.0); }
std::size_t random_index(std::mt19937_64 &rng, std::size_t n){ return static_cast<std::size_t>(rng() % static_cast<uint64_t>(n)); }

std::vector<std::size_t> propose_move(const Problem &p, const std::vector<uint8_t> &a, Score &current,
                                      bool &violations_known, std::mt19937_64 &rng){
    const double choice = uniform01(rng);
    if(choice < 0.65) return {random_index(rng, p.variables.size())};
    if(choice < 0.80){
        const auto d = random_index(rng, p.days.size());
        std::vector<std::size_t> onsite, remote;
        for(const auto v : p.vars_by_day[d]) (a[v] ? onsite : remote).push_back(v);
        if(!onsite.empty() && !remote.empty()) return {onsite[random_index(rng, onsite.size())], remote[random_index(rng, remote.size())]};
    }else if(choice < 0.90){
        const auto s = random_index(rng, p.staff.size());
        std::vector<std::size_t> onsite, remote;
        for(const auto v : p.vars_by_staff[s]) (a[v] ? onsite : remote).push_back(v);
        if(!onsite.empty() && !remote.empty()) return {onsite[random_index(rng, onsite.size())], remote[random_index(rng, remote.size())]};
    }else{
        if(!violations_known){ current = score_candidate(p, a, true); violations_known = true; }
        if(current.violations.empty()) return {random_index(rng, p.variables.size())};
        const auto &violation = current.violations[random_index(rng, current.violations.size())];
        const auto &variables = p.vars_by_day[violation.day];
        if(!variables.empty()){
            const auto start = random_index(rng, variables.size());
            for(std::size_t i = 0; i < variables.size(); ++i){
                const auto v = variables[(start + i) % variables.size()];
                auto trial = a;
                trial[v] ^= 1U;
                if(score_candidate(p, trial, false).components[violation.constraint] < current.components[violation.constraint]) return {v};
            }
        }
    }
    return {random_index(rng, p.variables.size())};
}

std::vector<uint8_t> greedy_initial(const Problem &p, std::chrono::steady_clock::time_point deadline){
    std::vector<uint8_t> a(p.variables.size(), 0);
    Score current = score_candidate(p, a, false);
    std::vector<std::size_t> coverage;
    for(std::size_t i = 0; i < p.constraints.size(); ++i) if(p.constraints[i].weight > 0.0 && (p.constraints[i].kind == ConstraintKind::Minimum || p.constraints[i].kind == ConstraintKind::Group)) coverage.push_back(i);
    std::stable_sort(coverage.begin(), coverage.end(), [&](std::size_t x, std::size_t y){ return p.constraints[x].weight > p.constraints[y].weight; });
    for(std::size_t accepted = 0; accepted < p.variables.size(); ++accepted){
        if(std::chrono::steady_clock::now() >= deadline) break;
        double best_delta = std::numeric_limits<double>::infinity();
        std::size_t best_var = p.variables.size();
        Score best_score;
        for(const auto ci : coverage){
            if(current.components[ci] <= 0.0) continue;
            for(std::size_t v = 0; v < a.size(); ++v){
                if((v % 64U) == 0U && std::chrono::steady_clock::now() >= deadline) break;
                a[v] ^= 1U;
                auto trial = score_candidate(p, a, false);
                a[v] ^= 1U;
                const double delta = trial.objective - current.objective;
                if(trial.components[ci] < current.components[ci] &&
                   (delta < best_delta || (delta == best_delta && v < best_var))){
                    best_delta = delta; best_var = v; best_score = std::move(trial);
                }
            }
            if(best_var != a.size()) break;
        }
        if(best_var == a.size()) break;
        a[best_var] ^= 1U;
        current = std::move(best_score);
    }
    return a;
}

double calibrate_temperature(const Problem &p, const std::vector<uint8_t> &initial, uint64_t seed,
                             std::chrono::steady_clock::time_point deadline){
    std::mt19937_64 rng(mix_seed(seed, 0x43414c4942524154ULL));
    const auto base = score_candidate(p, initial, false);
    std::vector<double> uphill;
    const std::size_t samples = std::min<std::size_t>(128, std::max<std::size_t>(16, p.variables.size() * 2));
    for(std::size_t i = 0; i < samples; ++i){
        if(std::chrono::steady_clock::now() >= deadline) break;
        auto trial = initial;
        trial[random_index(rng, trial.size())] ^= 1U;
        const double delta = score_candidate(p, trial, false).objective - base.objective;
        if(delta > 0.0) uphill.push_back(delta);
    }
    if(uphill.empty()) return std::max(1.0e-9, std::abs(base.objective) * 1.0e-3);
    std::sort(uphill.begin(), uphill.end());
    const double median = uphill[(uphill.size() - 1) / 2];
    return -median / std::log(0.8);
}

struct Settings {
    uint64_t seed = 0;
    uint64_t iterations = 250000;
    double runtime = 0.0;
    std::size_t outputs = 3;
    std::size_t archive = 256;
    bool auto_temperature = true;
    double temperature = 0.0;
    double end_fraction = 0.001;
    bool auto_restarts = true;
    std::size_t restarts = 0;
    std::string label;
};

struct SearchResult { std::vector<Entry> selected; std::vector<Entry> archive; uint64_t proposals = 0; double elapsed = 0.0; };

SearchResult optimize(const Problem &p, const Settings &settings, std::chrono::steady_clock::time_point started){
    using clock = std::chrono::steady_clock;
    const auto hard_deadline = settings.runtime > 0.0 ? started + std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(settings.runtime)) : clock::time_point::max();
    const double reserve_seconds = settings.runtime > 0.0 ? std::min(settings.runtime, std::max(0.25, settings.runtime * 0.05)) : 0.0;
    const auto search_deadline = settings.runtime > 0.0 ? hard_deadline - std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(reserve_seconds)) : hard_deadline;
    const auto selection_deadline = settings.runtime > 0.0
        ? hard_deadline - std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(reserve_seconds * 0.5))
        : hard_deadline;
    CandidateStore store(p, settings.archive, settings.archive);
    std::vector<uint8_t> baseline(p.variables.size(), 0);
    store.insert(baseline, score_candidate(p, baseline, false));
    auto greedy = baseline;
    if(clock::now() < search_deadline) greedy = greedy_initial(p, search_deadline);
    store.insert(greedy, score_candidate(p, greedy, false));
    const double t_start = settings.auto_temperature
        ? (clock::now() < search_deadline ? calibrate_temperature(p, greedy, settings.seed, search_deadline) : 1.0e-9)
        : settings.temperature;
    const auto chains = settings.restarts;
    const double runtime_search_seconds = settings.runtime > 0.0 ? std::max(0.0, settings.runtime - reserve_seconds) : 0.0;
    uint64_t proposals = 0;
    for(std::size_t chain = 0; chain < chains; ++chain){
        if(settings.runtime > 0.0 && clock::now() >= search_deadline) break;
        const auto chain_start = settings.runtime > 0.0
            ? started + std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(runtime_search_seconds * static_cast<double>(chain) / static_cast<double>(chains)))
            : started;
        const auto chain_deadline = settings.runtime > 0.0
            ? started + std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(runtime_search_seconds * static_cast<double>(chain + 1) / static_cast<double>(chains)))
            : search_deadline;
        std::mt19937_64 rng(mix_seed(settings.seed, chain));
        auto current = (chain % 2 == 0) ? greedy : baseline;
        if(chain > 0){
            const std::size_t flips = 1 + random_index(rng, std::max<std::size_t>(1, std::min<std::size_t>(p.variables.size(), 2 + chain)));
            for(std::size_t i = 0; i < flips; ++i) current[random_index(rng, current.size())] ^= 1U;
        }
        auto current_score = score_candidate(p, current, true);
        bool violations_known = true;
        store.insert(current, score_candidate(p, current, false));
        const uint64_t chain_budget = settings.runtime > 0.0 ? std::numeric_limits<uint64_t>::max() : settings.iterations / chains + (chain < settings.iterations % chains ? 1U : 0U);
        for(uint64_t local = 0; local < chain_budget; ++local){
            if(settings.runtime > 0.0 && clock::now() >= chain_deadline) break;
            double progress = 0.0;
            if(settings.runtime > 0.0){
                const auto chain_seconds = std::max(1.0e-9, runtime_search_seconds / static_cast<double>(chains));
                progress = std::min(1.0, std::max(0.0, std::chrono::duration<double>(clock::now() - chain_start).count() / chain_seconds));
            }else progress = chain_budget <= 1 ? 1.0 : static_cast<double>(local) / static_cast<double>(chain_budget - 1);
            const double temperature = t_start * std::pow(settings.end_fraction, progress);
            auto move = propose_move(p, current, current_score, violations_known, rng);
            auto trial = current;
            for(const auto v : move) trial[v] ^= 1U;
            auto trial_score = score_candidate(p, trial, false);
            const double delta = trial_score.objective - current_score.objective;
            const bool accept = delta <= 0.0 || uniform01(rng) < std::exp(std::max(-745.0, -delta / std::max(temperature, 1.0e-300)));
            ++proposals;
            if(accept){
                current = std::move(trial);
                current_score = trial_score;
                violations_known = false;
                store.insert(current, std::move(trial_score));
            }
        }
        store.insert(current, score_candidate(p, current, false));
    }
    const auto encoded_bits = std::numeric_limits<std::size_t>::digits;
    const std::size_t decision_space = p.variables.size() < encoded_bits
        ? (std::size_t{1} << p.variables.size())
        : std::numeric_limits<std::size_t>::max();
    const std::size_t attainable = std::min(settings.outputs, decision_space);
    const auto fallback_base = store.best.assignment;
    std::vector<std::size_t> bit_order(p.variables.size());
    std::iota(bit_order.begin(), bit_order.end(), 0);
    std::mt19937_64 fallback_rng(mix_seed(settings.seed, 0x46414c4c4241434bULL));
    for(std::size_t i = bit_order.size(); i > 1; --i) std::swap(bit_order[i - 1], bit_order[random_index(fallback_rng, i)]);
    const std::size_t fallback_limit = std::min(decision_space, settings.outputs * 10U);
    for(std::size_t variant = 1; variant < fallback_limit && store.elite.size() < attainable; ++variant){
        if(settings.runtime > 0.0 && clock::now() >= selection_deadline) break;
        auto a = fallback_base;
        const std::size_t gray = variant ^ (variant >> 1U);
        for(std::size_t bit = 0; bit < bit_order.size() && bit < encoded_bits; ++bit){
            if(((gray >> bit) & 1U) != 0U) a[bit_order[bit]] ^= 1U;
        }
        store.insert(a, score_candidate(p, a, false));
    }
    std::vector<Entry> selected;
    selected.push_back(store.best);
    const auto add_unique = [&](std::vector<Entry> &dest, const Entry &e){
        if(std::none_of(dest.begin(), dest.end(), [&](const Entry &x){ return x.assignment == e.assignment; })) dest.push_back(e);
    };
    while(selected.size() < settings.outputs){
        if(settings.runtime > 0.0 && clock::now() >= selection_deadline) break;
        std::size_t pick = store.archive.size();
        double best_distance = -1.0;
        std::vector<double> mins, spans;
        if(!store.archive.empty() && !store.archive.front().pareto.empty()){
            mins.assign(store.archive.front().pareto.size(), std::numeric_limits<double>::infinity());
            std::vector<double> maxs(mins.size(), -std::numeric_limits<double>::infinity());
            for(const auto &e : store.archive) for(std::size_t j = 0; j < mins.size(); ++j){ mins[j] = std::min(mins[j], e.pareto[j]); maxs[j] = std::max(maxs[j], e.pareto[j]); }
            spans.resize(mins.size()); for(std::size_t j = 0; j < mins.size(); ++j) spans[j] = maxs[j] - mins[j];
        }
        for(std::size_t i = 0; i < store.archive.size(); ++i){
            if(std::any_of(selected.begin(), selected.end(), [&](const Entry &x){ return x.assignment == store.archive[i].assignment; })) continue;
            double nearest = std::numeric_limits<double>::infinity();
            for(const auto &chosen : selected){
                double sum = 0.0;
                for(std::size_t j = 0; j < spans.size(); ++j){ const double diff = spans[j] <= 0.0 ? 0.0 : (store.archive[i].pareto[j] - chosen.pareto[j]) / spans[j]; sum += diff * diff; }
                nearest = std::min(nearest, std::sqrt(sum));
            }
            if(pick == store.archive.size() || nearest > best_distance ||
               (nearest == best_distance &&
                (store.archive[i].score.objective < store.archive[pick].score.objective ||
                 (store.archive[i].score.objective == store.archive[pick].score.objective && store.archive[i].sequence < store.archive[pick].sequence)))){
                pick = i; best_distance = nearest;
            }
        }
        if(pick == store.archive.size()) break;
        add_unique(selected, store.archive[pick]);
    }
    for(const auto &e : store.elite) if(selected.size() < settings.outputs) add_unique(selected, e);
    std::sort(selected.begin(), selected.end(), better_entry);
    SearchResult result;
    result.selected = std::move(selected); result.archive = store.archive; result.proposals = proposals;
    result.elapsed = std::chrono::duration<double>(clock::now() - started).count();
    return result;
}

bool is_pareto(const Entry &entry, const std::vector<Entry> &archive){
    return std::none_of(archive.begin(), archive.end(), [&](const Entry &e){ return dominates(e, entry); });
}

void report_row(tables::table2 &t, int64_t &row, const std::vector<std::string> &fields){
    for(std::size_t i = 0; i < fields.size(); ++i) t.inject(row, static_cast<int64_t>(i), fields[i]);
    ++row;
}

std::shared_ptr<Sparse_Table> render(const Sparse_Table &source, const Problem &p, const Entry &entry,
                                      const Settings &settings, const SearchResult &search, std::size_t index,
                                      std::size_t count, const std::string &normalized_label,
                                      int64_t &result_summary_row, int64_t &elapsed_summary_row){
    auto out = std::make_shared<Sparse_Table>(source);
    for(std::size_t v = 0; v < p.variables.size(); ++v){
        const auto &var = p.variables[v];
        out->table.inject(var.row, var.col, entry.assignment[v] ? (var.preference ? "Onsite*" : "Onsite") : "Remote");
    }
    const std::string label = settings.label + " " + std::to_string(index + 1);
    out->table.metadata["TableLabel"] = label;
    out->table.metadata["NormalizedTableLabel"] = normalized_label;
    out->table.metadata["Description"] = "Onsite/remote schedule optimized by OptimizeSchedule; decision-support result, not proof of staffing safety or global optimality";
    out->table.metadata["ScheduleOptimizerSeed"] = std::to_string(settings.seed);
    out->table.metadata["ScheduleOptimizerObjective"] = number(entry.score.objective);
    out->table.metadata["ScheduleOptimizerResultIndex"] = std::to_string(index + 1);
    const auto full = score_candidate(p, entry.assignment, true);
    int64_t row = out->table.next_empty_row() + 1;
    const auto summary = [&](const std::string &key, const std::string &value){ report_row(out->table, row, {report_marker, "Summary", key, value}); };
    result_summary_row = row;
    summary("result", std::to_string(index + 1) + "/" + std::to_string(count));
    summary("seed", std::to_string(settings.seed));
    summary("search-mode", settings.runtime > 0.0 ? "runtime" : "iterations");
    summary("actual-iterations", std::to_string(search.proposals));
    elapsed_summary_row = row;
    summary("actual-seconds", "pending final rendering");
    summary("weighted-objective", number(full.objective));
    summary("Pareto", is_pareto(entry, search.archive) ? "yes (relative to final retained archive)" : "no (dominated elite fallback)");
    summary("archive-size", std::to_string(search.archive.size()));
    summary("mutable-cells", std::to_string(p.variables.size()));
    summary("active-days", std::to_string(std::count_if(p.days.begin(), p.days.end(), [](const Day &d){ return d.active; })));
    summary("direct-violations", std::to_string(full.violations.size()));
    const auto overrides = static_cast<std::size_t>(std::count_if(p.variables.begin(), p.variables.end(), [&](const Variable &v){ return v.preference && entry.assignment[&v - p.variables.data()] != 0; }));
    summary("preference-overrides", std::to_string(overrides));
    summary("override-cost-enabled", std::any_of(p.constraints.begin(), p.constraints.end(), [](const Constraint &c){ return c.kind == ConstraintKind::FairOverrides && c.weight > 0.0; }) ? "yes" : "no");
    for(std::size_t ci = 0; ci < p.constraints.size(); ++ci){
        const auto &c = p.constraints[ci];
        const auto &detail = full.details[ci];
        if(detail.fairness.empty()) continue;
        summary("fairness-source-row-" + std::to_string(c.row), "mean=" + number(detail.mean) + "; deviation=" + number(detail.deviation));
        for(const auto &staff : detail.fairness){
            summary("fairness-source-row-" + std::to_string(c.row) + "-staff-" + p.staff[staff.staff].label,
                    "numerator=" + std::to_string(staff.numerator) + "; denominator=" + std::to_string(staff.denominator) + "; ratio=" + number(staff.ratio));
        }
    }
    for(std::size_t ci = 0; ci < p.constraints.size(); ++ci){
        const auto &c = p.constraints[ci];
        report_row(out->table, row, {report_marker, "Component", std::to_string(c.row), c.label, number(c.weight), number(full.components[ci]), number(c.weight * full.components[ci]), c.weight == 0.0 ? "disabled/advisory" : "active"});
    }
    auto violations = full.violations;
    std::sort(violations.begin(), violations.end(), [&](const DayViolation &a, const DayViolation &b){ return std::tie(p.days[a.day].date.key, p.constraints[a.constraint].row) < std::tie(p.days[b.day].date.key, p.constraints[b.constraint].row); });
    for(const auto &v : violations) report_row(out->table, row, {report_marker, "DayViolation", p.days[v.day].date.iso, std::to_string(p.constraints[v.constraint].row), p.constraints[v.constraint].label, std::to_string(v.observed), std::to_string(v.required), v.description});
    for(std::size_t d = 0; d < p.days.size(); ++d) for(std::size_t s = 0; s < p.staff.size(); ++s){
        const auto &cell = p.days[d].cells[s];
        if(cell.preference && entry.assignment[cell.variable]) report_row(out->table, row, {report_marker, "Override", p.days[d].date.iso, p.staff[s].label, "Pref", "Onsite*", "remote preference overridden"});
    }
    auto weekly = full.weekly;
    std::sort(weekly.begin(), weekly.end(), [](const WeeklyRecord &a, const WeeklyRecord &b){ return std::tie(a.year,a.week,a.staff,a.constraint) < std::tie(b.year,b.week,b.staff,b.constraint); });
    for(const auto &w : weekly){
        std::ostringstream wk; wk.imbue(std::locale::classic()); wk << w.year << "-W" << std::setfill('0') << std::setw(2) << w.week;
        report_row(out->table, row, {report_marker, "Weekly", wk.str(), p.staff[w.staff].label, p.constraints[w.constraint].label, std::to_string(w.count), std::to_string(w.limit), std::to_string(w.excess)});
    }
    for(std::size_t s = 0; s < p.staff.size(); ++s){
        uint64_t assigned_on = 0, assigned_remote = 0, pref = 0, override = 0, total_on = 0, total_remote = 0;
        for(std::size_t d = 0; d < p.days.size(); ++d){
            if(!p.days[d].active) continue;
            const auto &cell = p.days[d].cells[s];
            if(cell.mutable_cell){
                total_on += entry.assignment[cell.variable] != 0;
                total_remote += entry.assignment[cell.variable] == 0;
            }else{
                total_on += cell.status == "onsite";
                total_remote += cell.status == "remote";
            }
            if(cell.mutable_cell){ assigned_on += entry.assignment[cell.variable] != 0; assigned_remote += entry.assignment[cell.variable] == 0; pref += cell.preference; override += cell.preference && entry.assignment[cell.variable] != 0; }
        }
        const auto eligible = assigned_on + assigned_remote;
        report_row(out->table, row, {report_marker, "StaffTally", p.staff[s].label, std::to_string(assigned_on), std::to_string(assigned_remote), std::to_string(pref), std::to_string(override), std::to_string(assigned_remote), eligible ? number(static_cast<double>(assigned_remote) / eligible) : "0", std::to_string(total_on), std::to_string(total_remote)});
    }
    for(const auto &day : p.days) if(!day.active) report_row(out->table, row, {report_marker, "ExcludedDay", day.date.iso, day.exclusion, "unanimous explicitly excluded status"});
    for(const auto &f : p.feasibility) report_row(out->table, row, {report_marker, "Feasibility", p.days[f.day].date.iso, std::to_string(p.constraints[f.constraint].row), p.constraints[f.constraint].label, std::to_string(f.maximum), std::to_string(p.constraints[f.constraint].requirement), "daily coverage requirement is provably impossible independently of other constraints"});
    return out;
}

std::string required_arg(const OperationArgPkg &args, const std::string &name){
    const auto value = args.getValueStr(name);
    if(!value) throw std::invalid_argument("OptimizeSchedule missing required argument '" + name + "'");
    return *value;
}

Settings parse_settings(const OperationArgPkg &args){
    if(!args.containsExactly({"TableSelection", "RandomSeed", "Iterations", "RuntimeSeconds", "OutputSchedules", "ParetoArchiveSize", "TemperatureStart", "TemperatureEnd", "RestartCount", "ExcludeUnanimousStatuses", "TableLabel"}))
        throw std::invalid_argument("OptimizeSchedule requires exactly its documented arguments; an argument is missing or unknown");
    Settings s;
    s.seed = parse_u64(required_arg(args, "RandomSeed"), "RandomSeed");
    s.iterations = parse_u64(required_arg(args, "Iterations"), "Iterations");
    s.runtime = parse_decimal(required_arg(args, "RuntimeSeconds"), "RuntimeSeconds");
    if(s.runtime < 0.0 || s.runtime > 30.0) throw std::invalid_argument("RuntimeSeconds must be in [0, 30]");
    if(s.runtime == 0.0 && s.iterations == 0) throw std::invalid_argument("Iterations must be positive in iteration mode");
    s.outputs = static_cast<std::size_t>(parse_u64(required_arg(args, "OutputSchedules"), "OutputSchedules"));
    if(s.outputs < 1 || s.outputs > 20) throw std::invalid_argument("OutputSchedules must be in [1, 20]");
    s.archive = static_cast<std::size_t>(parse_u64(required_arg(args, "ParetoArchiveSize"), "ParetoArchiveSize"));
    if(s.archive < s.outputs || s.archive > 4096) throw std::invalid_argument("ParetoArchiveSize must be in [OutputSchedules, 4096]");
    const auto temperature = fold(required_arg(args, "TemperatureStart"));
    s.auto_temperature = temperature == "auto";
    if(!s.auto_temperature){ s.temperature = parse_decimal(temperature, "TemperatureStart"); if(s.temperature <= 0.0) throw std::invalid_argument("TemperatureStart must be auto or positive"); }
    s.end_fraction = parse_decimal(required_arg(args, "TemperatureEnd"), "TemperatureEnd");
    if(s.end_fraction <= 0.0 || s.end_fraction >= 1.0) throw std::invalid_argument("TemperatureEnd must be in (0, 1)");
    const auto restarts = fold(required_arg(args, "RestartCount"));
    s.auto_restarts = restarts == "auto";
    s.restarts = s.auto_restarts ? std::min<std::size_t>(s.outputs, 8) : static_cast<std::size_t>(parse_u64(restarts, "RestartCount"));
    if(s.restarts < 1 || s.restarts > 64) throw std::invalid_argument("RestartCount must be auto or in [1, 64]");
    s.label = required_arg(args, "TableLabel");
    return s;
}

} // namespace

OperationDoc OpArgDocOptimizeSchedule(){
    OperationDoc out;
    out.name = "OptimizeSchedule";
    out.tags.emplace_back("category: table processing");
    out.tags.emplace_back("category: optimization");
    out.desc = "Optimizes mutable x and Pref schedule cells into Onsite/Remote assignments using exact soft-constraint scoring and seeded simulated annealing. The selected source table is never modified; each result is an auditable decision-support alternative.";
    out.notes.emplace_back("Pref is rendered Remote unless overridden as Onsite*. Override cost exists only when a positive-weight fairness_overrides row is supplied; there is no hidden objective.");
    out.notes.emplace_back("Constraint rows are: Constraint | type | non-negative weight | expression | optional policies. Types and expressions are minimum_onsite or group(name) with 'any N of all' or 'any N of A or B ...'; exclusivity(name) with 'any 1 of A xor B ...'; max_consecutive_remote with a non-negative integer; max_weekly_remote with 'staff=limit, ...'; and fairness_remote or fairness_overrides with an empty expression.");
    out.notes.emplace_back("Default counted statuses are Onsite for minimum_onsite/group, Onsite|Prim|Sec for exclusivity, and Remote for consecutive/weekly/fairness_remote. A trailing statuses=StatusA|StatusB policy replaces that default for every type except fairness_overrides; status matching is case-insensitive and Onsite* is canonicalized to Onsite.");
    out.notes.emplace_back("fairness_remote considers only active mutable cells for each staff member: its custom statuses policy selects which generated Onsite/Remote assignments form the numerator, while all eligible mutable cells form the denominator. fairness_overrides instead counts Pref cells assigned Onsite and combines override rate with staff-rate deviation.");
    out.notes.emplace_back("Report direct-violations counts emitted per-day/per-occurrence violation records and excludes aggregate fairness deviation. StaffTally reports generated mutable Onsite, generated mutable Remote, Pref cells, overridden Pref cells, mutable Remote count and fraction, then total active-day Onsite and Remote including fixed cells; Weekly and Feasibility records are separate.");
    out.notes.emplace_back("Pareto labels are relative to the final retained bounded archive, not a proven global Pareto front. Returned schedules do not prove staffing safety or global optimality.");
    out.notes.emplace_back("A seed and iteration count are reproducible only for the same inputs, build, standard-library/platform floating-point behaviour, and optimizer version. Runtime mode is deadline-driven and is not reproducible by seed alone.");
    out.notes.emplace_back("Each annealing chain uses mt19937_64 seeded by a fixed SplitMix64-style mix of RandomSeed and the zero-based chain index. auto temperature samples 16 to 128 seeded single flips, uses the lower median positive objective increase, and chooses the temperature that accepts that increase with probability 0.8; if none is uphill it uses max(1e-9, abs(objective)*1e-3).");
    out.notes.emplace_back("Move probabilities are single flip 65%, same-day swap 15%, same-staff swap 10%, and targeted repair 10%; unavailable moves fall back to a single flip.");
    out.args.emplace_back(STWhitelistOpArgDoc()); out.args.back().name = "TableSelection"; out.args.back().default_val = "last";
    const auto add = [&](const std::string &name, const std::string &desc, const std::string &value, std::list<std::string> examples){
        out.args.emplace_back(); auto &a = out.args.back(); a.name = name; a.desc = desc; a.default_val = value; a.expected = true; a.examples = std::move(examples);
    };
    add("RandomSeed", "Literal reproducible unsigned 64-bit seed.", "0", {"0", "12345"});
    add("Iterations", "Positive proposal count in deterministic iteration mode; ignored when RuntimeSeconds is positive.", "250000", {"10000", "250000"});
    add("RuntimeSeconds", "End-to-end steady-clock limit in [0,30]; 20 seconds is recommended interactively. Zero selects iteration mode.", "0", {"0", "20"});
    add("OutputSchedules", "Number of unique alternatives requested, in [1,20].", "3", {"1", "3", "10"});
    add("ParetoArchiveSize", "Bounded non-dominated archive size in [OutputSchedules,4096].", "256", {"64", "256"});
    add("TemperatureStart", "auto, or a finite positive initial annealing temperature.", "auto", {"auto", "1.0"});
    add("TemperatureEnd", "Finite positive ending-temperature fraction strictly below one.", "0.001", {"0.001", "0.01"});
    add("RestartCount", "auto uses min(OutputSchedules,8), otherwise an integer in [1,64].", "auto", {"auto", "4"});
    add("ExcludeUnanimousStatuses", "Case-insensitive | separated statuses whose unanimous fully populated days are inactive.", "", {"", "Holiday", "Holiday|Closure"});
    add("TableLabel", "Base label for emitted tables; a one-based result number is appended.", "Optimized Schedule", {"Optimized Schedule"});
    return out;
}

bool OptimizeSchedule(Drover &DICOM_data,
                      const OperationArgPkg& OptArgs,
                      std::map<std::string, std::string>& /*InvocationMetadata*/,
                      const std::string& FilenameLex){
    const auto settings = parse_settings(OptArgs);
    const auto table_selection = required_arg(OptArgs, "TableSelection");
    std::set<std::string> excluded;
    const auto excluded_text = required_arg(OptArgs, "ExcludeUnanimousStatuses");
    if(!trim(excluded_text).empty()) for(auto &status : split_statuses(excluded_text, 0, 0)) excluded.insert(std::move(status));
    auto selected = Whitelist(All_STs(DICOM_data), table_selection);
    if(selected.size() != 1) throw std::invalid_argument("OptimizeSchedule requires exactly one selected table; selected " + std::to_string(selected.size()));
    const auto source = *selected.front();
    if(!source) throw std::invalid_argument("OptimizeSchedule selected a null table");
    const auto problem = parse_problem(*source, excluded);
    for(const auto &f : problem.feasibility) YLOGWARN("OptimizeSchedule: " << problem.days[f.day].date.iso << " constraint row " << problem.constraints[f.constraint].row << " can provide at most " << f.maximum << " of required " << problem.constraints[f.constraint].requirement);
    if(settings.runtime > 0.0) YLOGINFO("OptimizeSchedule runtime mode ignores Iterations and reserves finalization time");
    YLOGINFO("OptimizeSchedule optimizing " << problem.variables.size() << " mutable cells across " << problem.days.size() << " schedule days with seed " << settings.seed);
    const auto started = std::chrono::steady_clock::now();
    auto search = optimize(problem, settings, started);
    if(search.selected.empty()) throw std::runtime_error("OptimizeSchedule search retained no candidates");
    Explicator X(FilenameLex);
    std::list<std::shared_ptr<Sparse_Table>> outputs;
    std::vector<std::pair<int64_t, int64_t>> summary_rows;
    const auto hard_deadline = settings.runtime > 0.0
        ? started + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(settings.runtime))
        : std::chrono::steady_clock::time_point::max();
    double previous_render_seconds = 0.0;
    for(std::size_t i = 0; i < search.selected.size(); ++i){
        const auto remaining_seconds = std::chrono::duration<double>(hard_deadline - std::chrono::steady_clock::now()).count();
        if(i != 0 && (remaining_seconds <= 0.0 || remaining_seconds < previous_render_seconds)) break;
        const auto normalized = X(settings.label + " " + std::to_string(i + 1));
        int64_t result_row = 0, elapsed_row = 0;
        const auto render_started = std::chrono::steady_clock::now();
        outputs.push_back(render(*source, problem, search.selected[i], settings, search, i, search.selected.size(), normalized, result_row, elapsed_row));
        previous_render_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - render_started).count();
        summary_rows.emplace_back(result_row, elapsed_row);
    }
    const auto emitted = outputs.size();
    search.elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    std::size_t output_index = 0;
    const auto reported_elapsed = settings.runtime > 0.0 ? number(search.elapsed) : "not-applicable (deterministic iteration mode)";
    for(auto &output : outputs){
        output->table.inject(summary_rows[output_index].first, 3, std::to_string(output_index + 1) + "/" + std::to_string(emitted));
        output->table.inject(summary_rows[output_index].second, 3, reported_elapsed);
        ++output_index;
    }
    DICOM_data.table_data.splice(DICOM_data.table_data.end(), outputs);
    const auto end_to_end = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    YLOGINFO("OptimizeSchedule completed " << search.proposals << " proposals in " << end_to_end << " seconds end-to-end; emitted " << emitted << " unique schedules; best objective " << search.selected.front().score.objective);
    return true;
}
