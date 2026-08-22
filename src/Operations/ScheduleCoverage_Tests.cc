// ScheduleCoverage_Tests.cc.

#include "../doctest20251212/doctest.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "ScheduleCoverage.h"
#include "../Tables.h"

using namespace ScheduleCoverageCore;

namespace {

// Build a table from a dense grid of cells; empty strings are left as absent cells (mirroring read_csv semantics).
tables::table2 make_table(const std::vector<std::vector<std::string>> &rows){
    tables::table2 t;
    for(size_t r = 0; r < rows.size(); ++r){
        for(size_t c = 0; c < rows[r].size(); ++c){
            if(!rows[r][c].empty()) t.inject(static_cast<int64_t>(r), static_cast<int64_t>(c), rows[r][c]);
        }
    }
    return t;
}

TermLists default_terms(){
    TermLists terms;
    terms.holiday = { "Holiday" };
    terms.immutable = { "Vac", "CTO", "Prim", "Sec" };
    terms.onsite = { "onsite" };
    terms.remote_pref = { "Remote" };
    terms.undecided = { "x" };
    terms.remote = { "remote" };
    return terms;
}

std::set<std::string> collect_values(const tables::table2 &t){
    std::set<std::string> out;
    for(const auto &cell : t.data) out.insert(cell.val);
    return out;
}

int64_t count_value(const tables::table2 &t, const std::string &v){
    int64_t n = 0;
    for(const auto &cell : t.data) if(cell.val == v) ++n;
    return n;
}

// A compact signature of a set of solutions, used to assert determinism.
std::string signature(const std::vector<Solution> &sols){
    std::stringstream ss;
    for(const auto &s : sols){
        ss << s.fairness << "/" << s.overrides << "/" << s.violation_sum.size() << ":";
        for(const int64_t v : s.staff_onsite) ss << v << ",";
        ss << ";";
        for(const auto &day : s.day_onsite){
            ss << "[";
            for(const int64_t idx : day) ss << idx << ",";
            ss << "]";
        }
        ss << "|";
    }
    return ss.str();
}

} // anonymous namespace


TEST_CASE("ScheduleCoverage: cell classification covers all categories and is case-insensitive"){
    const auto terms = default_terms();

    CHECK(classify_cell("Vac", terms) == CellClass::Immutable);
    CHECK(classify_cell("vac", terms) == CellClass::Immutable);
    CHECK(classify_cell("CTO", terms) == CellClass::Immutable);
    CHECK(classify_cell("Prim", terms) == CellClass::Immutable);
    CHECK(classify_cell("Sec", terms) == CellClass::Immutable);
    CHECK(classify_cell("Holiday", terms) == CellClass::Holiday);
    CHECK(classify_cell("holiday", terms) == CellClass::Holiday);
    CHECK(classify_cell("onsite", terms) == CellClass::Onsite);
    CHECK(classify_cell("ONSITE", terms) == CellClass::Onsite);
    CHECK(classify_cell("Remote", terms) == CellClass::RemotePreference);
    CHECK(classify_cell("x", terms) == CellClass::Undecided);
    CHECK(classify_cell("X", terms) == CellClass::Undecided);

    bool known = true;
    CHECK(classify_cell("UnrecognizedTerm", terms, &known) == CellClass::Immutable);
    CHECK(known == false);
    CHECK(classify_cell("Vac", terms, &known) == CellClass::Immutable);
    CHECK(known == true);
    CHECK(classify_cell("", terms, &known) == CellClass::Immutable);
    CHECK(known == false);
}

TEST_CASE("ScheduleCoverage: cell classification supports regex term mode"){
    TermLists terms;
    terms.undecided = { "x|\\?" };
    terms.regex_mode = true;

    CHECK(classify_cell("x", terms) == CellClass::Undecided);
    CHECK(classify_cell("?", terms) == CellClass::Undecided);
    CHECK(classify_cell("y", terms) == CellClass::Immutable);
}

TEST_CASE("ScheduleCoverage: fixed-remote terms are recognized when non-overlapping"){
    // The default term lists use 'Remote' (preference) and 'remote' (fixed); because matching is case-insensitive,
    // 'Remote' takes priority. Use a non-overlapping fixed-remote term to exercise that category directly.
    TermLists terms;
    terms.remote_pref = { "Remote" };
    terms.remote = { "home" };

    CHECK(classify_cell("Remote", terms) == CellClass::RemotePreference);
    CHECK(classify_cell("home", terms) == CellClass::Remote);
    CHECK(classify_cell("Home", terms) == CellClass::Remote);
}

TEST_CASE("ScheduleCoverage: quota parsing"){
    int64_t min = 0;
    std::vector<std::string> subset;

    CHECK(parse_quota("any 2", min, subset));
    CHECK(min == 2);
    CHECK(subset.empty());

    CHECK(parse_quota("any", min, subset));
    CHECK(min == 1);
    CHECK(subset.empty());

    CHECK(parse_quota("3", min, subset));
    CHECK(min == 3);
    CHECK(subset.empty());

    CHECK(parse_quota("XC OR XD OR XE", min, subset));
    CHECK(min == 1);
    CHECK(subset == std::vector<std::string>({"XC", "XD", "XE"}));

    CHECK(parse_quota("any 2 of XC OR XD", min, subset));
    CHECK(min == 2);
    CHECK(subset == std::vector<std::string>({"XC", "XD"}));

    CHECK(parse_quota("XC OR XD", min, subset));
    CHECK(min == 1);
    CHECK(subset == std::vector<std::string>({"XC", "XD"}));

    CHECK(!parse_quota("", min, subset));
    CHECK(!parse_quota("garbage !!", min, subset));
    CHECK(!parse_quota("any xyz", min, subset));
}

TEST_CASE("ScheduleCoverage: schedule parsing detects requirements, headers, staff, and days"){
    const auto t = make_table({
        { "Requirement 1", "onsite", "any 2" },
        { "Requirement 2", "srs", "XC OR XD" },
        {},
        { "Date", "XA", "XB" },
        { "Mon, Aug 31, 2026", "x", "Remote" },
        { "Tues, Sept 1, 2026", "Holiday", "Holiday" },
    });

    const auto schedule = parse_schedule(t, "^Requirement", "^Date$", default_terms());

    REQUIRE(schedule.requirements.size() == 2);
    CHECK(schedule.requirements[0].label == "Requirement 1");
    CHECK(schedule.requirements[0].type == "onsite");
    CHECK(schedule.requirements[0].min_onsite == 2);
    CHECK(schedule.requirements[1].label == "Requirement 2");
    CHECK(schedule.requirements[1].subset == std::vector<std::string>({"XC", "XD"}));

    REQUIRE(schedule.staff.size() == 2);
    CHECK(schedule.staff[0] == "XA");
    CHECK(schedule.staff[1] == "XB");

    REQUIRE(schedule.days.size() == 2);
    CHECK(schedule.days[0].date == "Mon, Aug 31, 2026");
    CHECK(schedule.days[0].classes[0] == CellClass::Undecided);
    CHECK(schedule.days[0].classes[1] == CellClass::RemotePreference);
    CHECK(schedule.days[0].holiday == false);

    CHECK(schedule.days[1].holiday == true);
}

TEST_CASE("ScheduleCoverage: repeated headers parse as a continuous day list"){
    const auto t = make_table({
        { "Requirement 1", "onsite", "any 1" },
        {},
        { "Date", "XA", "XB" },
        { "Mon", "x", "x" },
        { "Tues", "x", "x" },
        {},
        { "Date", "XA", "XB" },
        { "Wed", "x", "x" },
    });

    const auto schedule = parse_schedule(t, "^Requirement", "^Date$", default_terms());
    REQUIRE(schedule.staff.size() == 2);
    REQUIRE(schedule.days.size() == 3);
    CHECK(schedule.days[0].date == "Mon");
    CHECK(schedule.days[1].date == "Tues");
    CHECK(schedule.days[2].date == "Wed");
}

TEST_CASE("ScheduleCoverage: empty table throws"){
    const tables::table2 t;
    CHECK_THROWS_AS(parse_schedule(t, "^Requirement", "^Date$", default_terms()), std::runtime_error);
}

TEST_CASE("ScheduleCoverage: requirement referencing unknown staff throws"){
    const auto t = make_table({
        { "Requirement 1", "srs", "XC OR XD" },
        {},
        { "Date", "XA", "XB" },
        { "Mon", "x", "x" },
    });

    const auto schedule = parse_schedule(t, "^Requirement", "^Date$", default_terms());
    CHECK_THROWS_AS(build_requirement_model(schedule), std::runtime_error);
}

TEST_CASE("ScheduleCoverage: violation evaluation"){
    RequirementModel model;
    model.subsets = { { 0, 1 }, { 2 } };
    model.min_onsite = { 2, 1 };

    CHECK(evaluate_violation({}, model) == std::vector<int64_t>({ 2, 1 }));
    CHECK(evaluate_violation({ 0 }, model) == std::vector<int64_t>({ 1, 1 }));
    CHECK(evaluate_violation({ 0, 1 }, model) == std::vector<int64_t>({ 0, 1 }));
    CHECK(evaluate_violation({ 0, 1, 2 }, model) == std::vector<int64_t>({ 0, 0 }));
}

TEST_CASE("ScheduleCoverage: holiday days produce no candidates"){
    Day day;
    day.holiday = true;
    day.cells = { "Holiday" };
    day.classes = { CellClass::Holiday };

    RequirementModel model;
    model.subsets = { { 0 } };
    model.min_onsite = { 1 };

    CHECK(generate_day_candidates(day, model).empty());
}

TEST_CASE("ScheduleCoverage: candidate generation prunes dominated assignments"){
    Day day;
    day.classes = { CellClass::Undecided };
    day.cells = { "x" };

    RequirementModel model;
    model.subsets = { { 0 } };
    model.min_onsite = { 1 };

    const auto cands = generate_day_candidates(day, model);

    // Feasible candidate (on-site) dominates the infeasible (remote) candidate.
    REQUIRE(cands.size() == 1);
    CHECK(cands[0].onsite == std::vector<int64_t>({ 0 }));
    CHECK(cands[0].violation == std::vector<int64_t>({ 0 }));
}

TEST_CASE("ScheduleCoverage: baseline selection is lexicographic and prefers fewer overrides"){
    RequirementModel model;
    model.subsets = { { 0, 1 } };
    model.min_onsite = { 1 };

    // Day: XA is a RemotePreference, XB is undecided. Both {XA} (one override) and {XB} (no overrides) satisfy the
    // quota; the baseline should prefer {XB}.
    Day day;
    day.classes = { CellClass::RemotePreference, CellClass::Undecided };
    day.cells = { "Remote", "x" };

    auto cands = generate_day_candidates(day, model);
    REQUIRE(!cands.empty());

    std::vector<std::vector<DayCandidate>> per_day = { cands };
    const auto choice = select_baseline(per_day);
    REQUIRE(choice.size() == 1);

    const auto &best = cands[choice[0]];
    CHECK(best.violation == std::vector<int64_t>({ 0 }));
    CHECK(best.overridden.empty());
    CHECK(best.onsite == std::vector<int64_t>({ 1 }));
}

TEST_CASE("ScheduleCoverage: infeasible requirement yields its minimal lexicographic deficit"){
    // Two requirements: R1 = "any 1" (all staff), R2 = "at least 1 of {XD,XE}". XD and XE are immutable (Prim), so R2
    // is always violated by 1 while R1 is satisfiable.
    const auto t = make_table({
        { "Requirement 1", "onsite", "any 1" },
        { "Requirement 2", "sub", "XD OR XE" },
        {},
        { "Date", "XA", "XB", "XD", "XE" },
        { "Mon", "x", "x", "Prim", "Prim" },
    });

    const auto schedule = parse_schedule(t, "^Requirement", "^Date$", default_terms());
    const auto model = build_requirement_model(schedule);

    SolverConfig config;
    config.fairness_metric = "range";
    config.n_variations = 1;
    config.seed = 0;

    const auto solutions = produce_variations(schedule, model, config);
    REQUIRE(solutions.size() == 1);
    CHECK(solutions[0].violation_sum == std::vector<int64_t>({ 0, 1 }));
}

TEST_CASE("ScheduleCoverage: fairness is balanced on a two-staff two-day toy problem"){
    const auto t = make_table({
        { "Requirement 1", "onsite", "any 1" },
        {},
        { "Date", "XA", "XB" },
        { "Mon", "x", "x" },
        { "Tues", "x", "x" },
    });

    const auto schedule = parse_schedule(t, "^Requirement", "^Date$", default_terms());
    const auto model = build_requirement_model(schedule);

    SolverConfig config;
    config.fairness_metric = "range";
    config.fairness_weight = 1.0;
    config.preference_weight = 0.0;
    config.n_variations = 3;
    config.seed = 0;

    const auto solutions = produce_variations(schedule, model, config);
    REQUIRE(!solutions.empty());
    for(const auto &s : solutions){
        CHECK(s.violation_sum == std::vector<int64_t>({ 0 }));
    }

    // The Pareto front should contain the perfectly balanced solution (fairness range == 0).
    const auto it = std::find_if(solutions.begin(), solutions.end(), [](const Solution &s){
        return s.fairness == 0.0;
    });
    CHECK(it != solutions.end());
}

TEST_CASE("ScheduleCoverage: deterministic with a fixed seed"){
    const auto t = make_table({
        { "Requirement 1", "onsite", "any 1" },
        {},
        { "Date", "XA", "XB", "XC" },
        { "Mon", "x", "Remote", "x" },
        { "Tues", "Remote", "x", "x" },
        { "Wed", "x", "x", "Remote" },
    });

    const auto schedule = parse_schedule(t, "^Requirement", "^Date$", default_terms());
    const auto model = build_requirement_model(schedule);

    SolverConfig config;
    config.fairness_metric = "range";
    config.n_variations = 3;
    config.seed = 12345;

    const auto a = produce_variations(schedule, model, config);
    const auto b = produce_variations(schedule, model, config);

    REQUIRE(a.size() == b.size());
    CHECK(signature(a) == signature(b));
}

TEST_CASE("ScheduleCoverage: rendering preserves immutable cells and removes undecided terms"){
    const auto t = make_table({
        { "Requirement 1", "onsite", "any 1" },
        {},
        { "Date", "XA", "XB" },
        { "Mon", "x", "Vac" },
    });

    const auto schedule = parse_schedule(t, "^Requirement", "^Date$", default_terms());

    Solution sol;
    sol.day_onsite = { { 0 }, {} };
    sol.day_overridden = { {}, {} };
    sol.day_violation = { { 0 }, {} };
    sol.violation_sum = { 0 };
    sol.overrides = 0;
    sol.fairness = 0.0;
    sol.staff_onsite = { 1, 0 };

    const auto out = render_variation(t, schedule, sol);

    const auto values = collect_values(out);
    CHECK(values.count("onsite") == 1);   // XA's 'x' was replaced by 'onsite'.
    CHECK(values.count("Vac") == 1);      // Immutable cell preserved.
    CHECK(values.count("x") == 0);        // No undecided term remains.
    CHECK(values.count("== Schedule Report ==") == 1);
    CHECK(values.count("OBJECTIVES") == 1);
    CHECK(count_value(out, "TALLY") == 2); // One TALLY row per staff.
}

TEST_CASE("ScheduleCoverage: rendering reports flags, overrides, and tallies"){
    const auto t = make_table({
        { "Requirement 1", "onsite", "any 2" },
        {},
        { "Date", "XA", "XB" },
        { "Mon", "Remote", "x" },
    });

    // Hand-build a schedule and solution to exercise the FLAG/OVERRIDE/TALLY rows directly.
    ParsedSchedule ps;
    ps.staff = { "XA", "XB" };
    ps.staff_columns = { 1, 2 };
    Day day;
    day.date = "Mon";
    day.row = 3;
    day.holiday = false;
    day.cells = { "Remote", "x" };
    day.classes = { CellClass::RemotePreference, CellClass::Undecided };
    ps.days.push_back(day);

    Requirement r1;
    r1.label = "Requirement 1";
    r1.min_onsite = 1;
    Requirement r2;
    r2.label = "Requirement 2";
    r2.min_onsite = 2; // Impossible to fully meet with only 2 staff and XA overridden + XB on-site.
    ps.requirements = { r1, r2 };

    Solution sol;
    sol.day_onsite = { { 0, 1 } };
    sol.day_overridden = { { 0 } };
    sol.day_violation = { { 0, 1 } }; // R2 deficit = 1.
    sol.violation_sum = { 0, 1 };
    sol.overrides = 1;
    sol.fairness = 0.0;
    sol.staff_onsite = { 1, 1 };

    const auto out = render_variation(t, ps, sol);

    const auto values = collect_values(out);
    CHECK(values.count("FLAG") == 1);
    CHECK(values.count("OVERRIDE") == 1);
    CHECK(values.count("Remote -> onsite") == 1);
    CHECK(values.count("deficit=1") == 1);
    CHECK(count_value(out, "TALLY") == 2);
    CHECK(values.count("OBJECTIVES") == 1);

    // The remote preference cell was overridden to on-site.
    CHECK(values.count("onsite") == 1);
}
