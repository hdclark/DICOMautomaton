// ScheduleCoverage_Tests.cc.

#include "../doctest20251212/doctest.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ScheduleCoverage.h"
#include "../Tables.h"

using namespace ScheduleCoverageCore;

namespace {

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
    terms.vacation = { "Vac" };
    terms.immutable = { "CTO", "Prim", "Sec" };
    terms.onsite = { "onsite" };
    terms.remote_pref = { "Remote" };
    terms.undecided = { "x" };
    terms.remote = { "remote" };
    return terms;
}

const std::string constraint_re = "^(Hard|Soft)\\s+Constraint";

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

bool any_value_contains(const tables::table2 &t, const std::string &needle){
    for(const auto &cell : t.data) if(cell.val.find(needle) != std::string::npos) return true;
    return false;
}

std::string signature(const std::vector<Solution> &sols){
    std::stringstream ss;
    for(const auto &sol : sols){
        ss << sol.annealing_cost << '/' << sol.hard_violation_units << '/' << sol.soft_constraint_cost
           << '/' << sol.fairness << '/' << sol.overrides << ':';
        for(const auto &day : sol.day_onsite){
            ss << '[';
            for(const auto idx : day) ss << idx << ',';
            ss << ']';
        }
        ss << '|';
    }
    return ss.str();
}

} // anonymous namespace

TEST_CASE("ScheduleCoverage: cell classification remains configurable and case-insensitive"){
    const auto terms = default_terms();
    CHECK(classify_cell("Vac", terms) == CellClass::Vacation);
    CHECK(classify_cell("vac", terms) == CellClass::Vacation);
    CHECK(classify_cell("CTO", terms) == CellClass::Immutable);
    CHECK(classify_cell("Prim", terms) == CellClass::Immutable);
    CHECK(classify_cell("Sec", terms) == CellClass::Immutable);
    CHECK(classify_cell("Holiday", terms) == CellClass::Holiday);
    CHECK(classify_cell("ONSITE", terms) == CellClass::Onsite);
    CHECK(classify_cell("Remote", terms) == CellClass::RemotePreference);
    CHECK(classify_cell("X", terms) == CellClass::Undecided);

    TermLists custom = terms;
    custom.vacation = { "Leave", "PTO" };
    CHECK(classify_cell("leave", custom) == CellClass::Vacation);
    CHECK(classify_cell("Vac", custom) == CellClass::Immutable);
}

TEST_CASE("ScheduleCoverage: quota parsing supports all hard-constraint forms"){
    int64_t min = 0;
    std::vector<std::string> subset;
    CHECK(parse_quota("any 2", min, subset));
    CHECK(min == 2);
    CHECK(subset.empty());
    CHECK(parse_quota("any", min, subset));
    CHECK(min == 1);
    CHECK(parse_quota("3", min, subset));
    CHECK(min == 3);
    CHECK(parse_quota("XC OR XD OR XE", min, subset));
    CHECK(min == 1);
    CHECK(subset == std::vector<std::string>({"XC", "XD", "XE"}));
    CHECK(parse_quota("any 2 of XC OR XD", min, subset));
    CHECK(min == 2);
    CHECK(subset == std::vector<std::string>({"XC", "XD"}));
    CHECK(!parse_quota("garbage", min, subset));
}

TEST_CASE("ScheduleCoverage: parser distinguishes hard and weighted soft constraints"){
    const auto t = make_table({
        { "Hard Constraint 1", "onsite", "any 2" },
        { "Hard Constraint 2", "srs", "XA OR XB" },
        { "Soft Constraint 1", "max_consecutive_remote", "2", "Weight=1.0" },
        { "Soft Constraint 2", "exclusivity", "XA XOR XB", "Weight=1.5" },
        { "Soft Constraint 3", "max_weekly_remote", "XA = 2", "Weight=5" },
        {},
        { "Date", "XA", "XB" },
        { "Mon", "x", "Remote" },
    });

    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    REQUIRE(schedule.requirements.size() == 2);
    CHECK(schedule.requirements[0].label == "Hard Constraint 1");
    CHECK(schedule.requirements[0].min_onsite == 2);
    CHECK(schedule.requirements[1].subset == std::vector<std::string>({"XA", "XB"}));

    REQUIRE(schedule.soft_constraints.size() == 3);
    CHECK(schedule.soft_constraints[0].kind == SoftConstraintKind::MaxConsecutiveRemote);
    CHECK(schedule.soft_constraints[0].limit == 2);
    CHECK(schedule.soft_constraints[0].weight == doctest::Approx(1.0));
    CHECK(schedule.soft_constraints[1].kind == SoftConstraintKind::Exclusivity);
    CHECK(schedule.soft_constraints[1].staff == std::vector<std::string>({"XA", "XB"}));
    CHECK(schedule.soft_constraints[1].weight == doctest::Approx(1.5));
    CHECK(schedule.soft_constraints[2].kind == SoftConstraintKind::MaxWeeklyRemote);
    CHECK(schedule.soft_constraints[2].staff == std::vector<std::string>({"XA"}));
    CHECK(schedule.soft_constraints[2].limit == 2);
    CHECK(schedule.soft_constraints[2].weight == doctest::Approx(5.0));
}

TEST_CASE("ScheduleCoverage: soft constraints require valid inline weights and expressions"){
    const auto missing_weight = make_table({
        { "Soft Constraint 1", "exclusivity", "XA XOR XB" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "x" },
    });
    CHECK_THROWS_AS(parse_schedule(missing_weight, constraint_re, "^Date$", default_terms()), std::runtime_error);

    const auto negative_weight = make_table({
        { "Soft Constraint 1", "exclusivity", "XA XOR XB", "Weight=-1" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "x" },
    });
    CHECK_THROWS_AS(parse_schedule(negative_weight, constraint_re, "^Date$", default_terms()), std::runtime_error);

    const auto bad_xor = make_table({
        { "Soft Constraint 1", "exclusivity", "XA OR XB", "Weight=1" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "x" },
    });
    CHECK_THROWS_AS(parse_schedule(bad_xor, constraint_re, "^Date$", default_terms()), std::runtime_error);

    const auto bad_weekly = make_table({
        { "Soft Constraint 1", "max_weekly_remote", "XA two", "Weight=1" },
        { "Date", "XA" },
        { "Mon", "x" },
    });
    CHECK_THROWS_AS(parse_schedule(bad_weekly, constraint_re, "^Date$", default_terms()), std::runtime_error);
}

TEST_CASE("ScheduleCoverage: constraint model rejects unknown staff in hard and soft constraints"){
    const auto hard_bad = make_table({
        { "Hard Constraint 1", "srs", "XC OR XD" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "x" },
    });
    CHECK_THROWS_AS(build_constraint_model(parse_schedule(hard_bad, constraint_re, "^Date$", default_terms())), std::runtime_error);

    const auto soft_bad = make_table({
        { "Soft Constraint 1", "exclusivity", "XA XOR ZZ", "Weight=1" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "x" },
    });
    CHECK_THROWS_AS(build_constraint_model(parse_schedule(soft_bad, constraint_re, "^Date$", default_terms())), std::runtime_error);
}

TEST_CASE("ScheduleCoverage: repeated headers define week blocks"){
    const auto t = make_table({
        { "Soft Constraint 1", "max_weekly_remote", "XA = 1", "Weight=5" },
        {},
        { "Date", "XA" },
        { "Mon", "x" },
        { "Tue", "x" },
        {},
        { "Date", "XA" },
        { "Wed", "x" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    REQUIRE(schedule.days.size() == 3);
    CHECK(schedule.days[0].week_index == 0);
    CHECK(schedule.days[1].week_index == 0);
    CHECK(schedule.days[2].week_index == 1);
}

TEST_CASE("ScheduleCoverage: soft max-consecutive-remote skips vacation and holiday days"){
    const auto t = make_table({
        { "Soft Constraint 1", "max_consecutive_remote", "2", "Weight=1" },
        { "Date", "XA" },
        { "Mon", "x" },
        { "Tue", "Vac" },
        { "Wed", "x" },
        { "Thu", "Holiday" },
        { "Fri", "x" },
        { "Sat", "onsite" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    const auto model = build_constraint_model(schedule);
    std::vector<std::vector<int64_t>> all_remote(schedule.days.size());
    const auto p = evaluate_soft_penalties(schedule, model, all_remote);
    REQUIRE(p.size() == 1);
    CHECK(p[0] == 1); // Mon/Wed/Fri form a three-remote-workday run; Vac/Holiday are skipped.
}

TEST_CASE("ScheduleCoverage: exclusivity is at-most-one and counts Prim and Sec as present"){
    const auto t = make_table({
        { "Soft Constraint 1", "exclusivity", "XA XOR XB", "Weight=1.5" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "Prim" },
        { "Tue", "x", "Sec" },
        { "Wed", "x", "x" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    const auto model = build_constraint_model(schedule);

    std::vector<std::vector<int64_t>> assignment = {
        { 0 }, // XA onsite + XB Prim -> violation.
        {},    // XA remote + XB Sec -> no violation.
        {},    // Neither present -> no violation (mutual exclusion, not Boolean exactly-one).
    };
    const auto p = evaluate_soft_penalties(schedule, model, assignment);
    REQUIRE(p.size() == 1);
    CHECK(p[0] == 1);
}

TEST_CASE("ScheduleCoverage: max-weekly-remote is evaluated separately for each header week"){
    const auto t = make_table({
        { "Soft Constraint 1", "max_weekly_remote", "XA = 1", "Weight=5" },
        { "Date", "XA" },
        { "Mon", "x" },
        { "Tue", "x" },
        { "Date", "XA" },
        { "Wed", "x" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    const auto model = build_constraint_model(schedule);
    std::vector<std::vector<int64_t>> all_remote(schedule.days.size());
    const auto p = evaluate_soft_penalties(schedule, model, all_remote);
    REQUIRE(p.size() == 1);
    CHECK(p[0] == 1); // Week 1 has two remote days; week 2 has one.
}

TEST_CASE("ScheduleCoverage: candidate generation keeps hard-violation tradeoffs available to annealing"){
    Day day;
    day.cells = { "x" };
    day.classes = { CellClass::Undecided };
    RequirementModel model;
    model.subsets = { { 0 } };
    model.min_onsite = { 1 };
    const auto cands = generate_day_candidates(day, model);
    REQUIRE(cands.size() == 2);
    CHECK(std::count_if(cands.begin(), cands.end(), [](const DayCandidate &c){ return c.violation == std::vector<int64_t>({0}); }) == 1);
    CHECK(std::count_if(cands.begin(), cands.end(), [](const DayCandidate &c){ return c.violation == std::vector<int64_t>({1}); }) == 1);
}

TEST_CASE("ScheduleCoverage: RequirementViolationWeight changes the annealing tradeoff"){
    // XA is the only mutable person who can satisfy hard 'any 1'. XB is Prim, so assigning XA onsite violates the
    // high-weight exclusivity soft constraint. This makes the requirement weight directly observable.
    const auto t = make_table({
        { "Hard Constraint 1", "onsite", "any 1" },
        { "Soft Constraint 1", "exclusivity", "XA XOR XB", "Weight=100" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "Prim" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    const auto model = build_constraint_model(schedule);

    SolverConfig low;
    low.fairness_weight = 0.0;
    low.preference_weight = 0.0;
    low.requirement_violation_weight = 1.0;
    low.annealing_iterations = 100;
    low.n_variations = 1;
    low.seed = 2;
    const auto low_solutions = produce_variations(schedule, model, low);
    REQUIRE(low_solutions.size() == 1);
    CHECK(low_solutions[0].hard_violation_units == 1);
    CHECK(low_solutions[0].soft_constraint_cost == doctest::Approx(0.0));
    CHECK(low_solutions[0].annealing_cost == doctest::Approx(1.0));

    SolverConfig high = low;
    high.requirement_violation_weight = 1000.0;
    const auto high_solutions = produce_variations(schedule, model, high);
    REQUIRE(high_solutions.size() == 1);
    CHECK(high_solutions[0].hard_violation_units == 0);
    CHECK(high_solutions[0].soft_constraint_cost == doctest::Approx(100.0));
    CHECK(high_solutions[0].annealing_cost == doctest::Approx(100.0));
}

TEST_CASE("ScheduleCoverage: inline soft weights contribute to final annealing cost"){
    const auto t = make_table({
        { "Soft Constraint 1", "exclusivity", "XA XOR XB", "Weight=1.5" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "Prim" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    const auto model = build_constraint_model(schedule);
    SolverConfig config;
    config.fairness_weight = 0.0;
    config.preference_weight = 0.0;
    config.requirement_violation_weight = 0.0;
    config.annealing_iterations = 0;
    config.n_variations = 1;

    // Baseline has no hard constraints and therefore chooses no onsite assignment, so soft cost is zero.
    auto solutions = produce_variations(schedule, model, config);
    REQUIRE(solutions.size() == 1);
    CHECK(solutions[0].annealing_cost == doctest::Approx(0.0));

    // Directly evaluate an onsite assignment to confirm 1 penalty unit * 1.5 table weight.
    std::vector<std::vector<int64_t>> onsite = { { 0 } };
    const auto penalties = evaluate_soft_penalties(schedule, model, onsite);
    REQUIRE(penalties.size() == 1);
    CHECK(penalties[0] == 1);
    CHECK(model.soft[0].weight * penalties[0] == doctest::Approx(1.5));
}

TEST_CASE("ScheduleCoverage: annealing is deterministic for a fixed seed and iteration count"){
    const auto t = make_table({
        { "Hard Constraint 1", "onsite", "any 1" },
        { "Soft Constraint 1", "max_consecutive_remote", "1", "Weight=2" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "Remote" },
        { "Tue", "Remote", "x" },
        { "Wed", "x", "x" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    const auto model = build_constraint_model(schedule);
    SolverConfig config;
    config.annealing_iterations = 300;
    config.n_variations = 3;
    config.seed = 12345;
    const auto a = produce_variations(schedule, model, config);
    const auto b = produce_variations(schedule, model, config);
    CHECK(signature(a) == signature(b));
}

TEST_CASE("ScheduleCoverage: emitted solutions identify Pareto status among explored schedules"){
    const auto t = make_table({
        { "Hard Constraint 1", "onsite", "any 1" },
        { "Soft Constraint 1", "exclusivity", "XA XOR XB", "Weight=100" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "Prim" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    const auto model = build_constraint_model(schedule);
    SolverConfig config;
    config.fairness_weight = 0.0;
    config.preference_weight = 0.0;
    config.requirement_violation_weight = 10.0;
    config.annealing_iterations = 100;
    config.n_variations = 2;
    const auto solutions = produce_variations(schedule, model, config);
    REQUIRE(solutions.size() == 2);
    CHECK(solutions[0].pareto_nondominated);
    CHECK(solutions[1].pareto_nondominated);
}

TEST_CASE("ScheduleCoverage: rendering reports hard and soft violations and the final annealing cost"){
    const auto t = make_table({
        { "Hard Constraint 1", "onsite", "any 1" },
        { "Soft Constraint 1", "exclusivity", "XA XOR XB", "Weight=1.5" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "Prim" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());

    Solution sol;
    sol.day_onsite = { { 0 } };
    sol.day_overridden = { {} };
    sol.day_violation = { { 0 } };
    sol.violation_sum = { 0 };
    sol.soft_penalty = { 1 };
    sol.hard_violation_units = 0;
    sol.soft_constraint_cost = 1.5;
    sol.annealing_cost = 1.5;
    sol.pareto_nondominated = true;
    sol.staff_onsite = { 1, 0 };

    const auto out = render_variation(t, schedule, sol);
    const auto values = collect_values(out);
    CHECK(values.count("SOFT_FLAG") == 1);
    CHECK(values.count("SOFT_OBJECTIVE") == 1);
    CHECK(values.count("annealing_cost=1.5") == 1);
    CHECK(values.count("pareto_nondominated=true") == 1);
    CHECK(values.count("weight=1.5") == 1);
    CHECK(any_value_contains(out, "mutually exclusive staff XA, XB are simultaneously present"));
    CHECK(values.count("x") == 0);
}

TEST_CASE("ScheduleCoverage: rendering gives informative hard-deficit messages"){
    const auto t = make_table({
        { "Hard Constraint 1", "onsite", "any 2" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "x" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    Solution sol;
    sol.day_onsite = { { 0 } };
    sol.day_overridden = { {} };
    sol.day_violation = { { 1 } };
    sol.violation_sum = { 1 };
    sol.hard_violation_units = 1;
    sol.staff_onsite = { 1, 0 };
    const auto out = render_variation(t, schedule, sol);
    const auto values = collect_values(out);
    CHECK(values.count("FLAG") == 1);
    CHECK(values.count("Hard Constraint 1") == 1);
    CHECK(values.count("deficit=1") == 1);
    CHECK(values.count("quota=any 2") == 1);
}

TEST_CASE("ScheduleCoverage: override explanations can identify a soft constraint"){
    const auto t = make_table({
        { "Soft Constraint 1", "max_weekly_remote", "XA = 0", "Weight=5" },
        { "Date", "XA" },
        { "Mon", "Remote" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    Solution sol;
    sol.day_onsite = { { 0 } };
    sol.day_overridden = { { 0 } };
    sol.day_violation = { {} };
    sol.soft_penalty = { 0 };
    sol.overrides = 1;
    sol.staff_onsite = { 1 };
    const auto out = render_variation(t, schedule, sol);
    CHECK(count_value(out, "OVERRIDE") == 1);
    CHECK(any_value_contains(out, "reduces soft constraint 'max_weekly_remote' (Soft Constraint 1) by 1 penalty unit(s)"));
}

TEST_CASE("ScheduleCoverage: operation docs expose table-driven soft constraints and new annealing controls"){
    const auto doc = OpArgDocScheduleCoverage();
    const auto find_arg = [&](const std::string &name) -> const OperationArgDoc* {
        const auto it = std::find_if(doc.args.begin(), doc.args.end(), [&](const auto &arg){ return arg.name == name; });
        return it == doc.args.end() ? nullptr : &(*it);
    };

    REQUIRE(find_arg("RequirementRegex") != nullptr);
    CHECK(find_arg("RequirementRegex")->default_val == "^(Hard|Soft)\\s+Constraint");
    REQUIRE(find_arg("RequirementViolationWeight") != nullptr);
    CHECK(find_arg("RequirementViolationWeight")->default_val == "1000.0");
    REQUIRE(find_arg("AnnealingIterations") != nullptr);
    CHECK(find_arg("AnnealingIterations")->default_val == "100000");
    CHECK(find_arg("MaxConsecutiveRemoteDays") == nullptr);
    CHECK(find_arg("ConsecutiveRemoteWeight") == nullptr);
}
