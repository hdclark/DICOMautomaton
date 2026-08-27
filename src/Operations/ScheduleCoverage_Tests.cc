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

tables::table2 make_table(const std::vector<std::vector<std::string>> &rows){
    tables::table2 t;
    for(size_t r = 0; r < rows.size(); ++r){
        for(size_t c = 0; c < rows[r].size(); ++c){
            if(!rows[r][c].empty()){
                t.inject(static_cast<int64_t>(r), static_cast<int64_t>(c), rows[r][c]);
            }
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

const std::string constraint_re = "^(Hard|Soft)\\s+Constraint|^Requirement";

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

bool contains_prefix(const tables::table2 &t, const std::string &prefix){
    for(const auto &cell : t.data){
        if(cell.val.rfind(prefix, 0) == 0) return true;
    }
    return false;
}

std::string signature(const std::vector<Solution> &sols){
    std::stringstream ss;
    for(const auto &s : sols){
        ss << s.hard_optimal << "/" << s.fairness << "/" << s.overrides
           << "/" << s.soft_constraint_cost << "/" << s.annealing_cost << ":";
        for(const auto v : s.violation_sum) ss << v << ",";
        ss << ";";
        for(const auto &day : s.day_onsite){
            ss << "[";
            for(const auto idx : day) ss << idx << ",";
            ss << "]";
        }
        ss << "|";
    }
    return ss.str();
}

} // anonymous namespace


TEST_CASE("ScheduleCoverage: default exact terms distinguish Remote preference from final fixed remote"){
    const auto terms = default_terms();

    CHECK(classify_cell("Remote", terms) == CellClass::RemotePreference);
    CHECK(classify_cell("remote", terms) == CellClass::Remote);

    // Case-insensitive fallback remains available when there is no exact spelling match.
    CHECK(classify_cell("VAC", terms) == CellClass::Vacation);
    CHECK(classify_cell("ONSITE", terms) == CellClass::Onsite);
}

TEST_CASE("ScheduleCoverage: regex mode still gives exact spelling precedence to Remote versus remote"){
    auto terms = default_terms();
    terms.regex_mode = true;

    CHECK(classify_cell("Remote", terms) == CellClass::RemotePreference);
    CHECK(classify_cell("remote", terms) == CellClass::Remote);
}

TEST_CASE("ScheduleCoverage: quota parsing"){
    int64_t min = 0;
    std::vector<std::string> subset;

    CHECK(parse_quota("any 2", min, subset));
    CHECK(min == 2);
    CHECK(subset.empty());

    CHECK(parse_quota("any 2 of XC OR XD", min, subset));
    CHECK(min == 2);
    CHECK(subset == std::vector<std::string>({ "XC", "XD" }));

    CHECK(parse_quota("XC OR XD OR XE", min, subset));
    CHECK(min == 1);
    CHECK(subset == std::vector<std::string>({ "XC", "XD", "XE" }));

    CHECK(!parse_quota("garbage", min, subset));
}

TEST_CASE("ScheduleCoverage: parser reads hard and weighted soft constraints"){
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
    CHECK(schedule.requirements[1].subset == std::vector<std::string>({ "XA", "XB" }));

    REQUIRE(schedule.soft_constraints.size() == 3);
    CHECK(schedule.soft_constraints[0].kind == SoftConstraintKind::MaxConsecutiveRemote);
    CHECK(schedule.soft_constraints[0].limit == 2);
    CHECK(schedule.soft_constraints[0].weight == doctest::Approx(1.0));

    CHECK(schedule.soft_constraints[1].kind == SoftConstraintKind::Exclusivity);
    CHECK(schedule.soft_constraints[1].staff == std::vector<std::string>({ "XA", "XB" }));
    CHECK(schedule.soft_constraints[1].weight == doctest::Approx(1.5));

    CHECK(schedule.soft_constraints[2].kind == SoftConstraintKind::MaxWeeklyRemote);
    CHECK(schedule.soft_constraints[2].staff == std::vector<std::string>({ "XA" }));
    CHECK(schedule.soft_constraints[2].limit == 2);
    CHECK(schedule.soft_constraints[2].weight == doctest::Approx(5.0));
}

TEST_CASE("ScheduleCoverage: constraint-looking rows excluded by RequirementRegex fail closed"){
    const auto t = make_table({
        { "Hard Constraint 1", "onsite", "any 1" },
        { "Soft Constraint 1", "max_consecutive_remote", "2", "Weight=1" },
        { "Date", "XA" },
        { "Mon", "x" },
    });

    CHECK_THROWS_AS(parse_schedule(t, "^Requirement", "^Date$", default_terms()), std::runtime_error);
}

TEST_CASE("ScheduleCoverage: malformed soft constraints fail loudly"){
    const auto no_weight = make_table({
        { "Soft Constraint 1", "exclusivity", "XA XOR XB" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "x" },
    });
    CHECK_THROWS_AS(parse_schedule(no_weight, constraint_re, "^Date$", default_terms()), std::runtime_error);

    const auto negative_weight = make_table({
        { "Soft Constraint 1", "exclusivity", "XA XOR XB", "Weight=-1" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "x" },
    });
    CHECK_THROWS_AS(parse_schedule(negative_weight, constraint_re, "^Date$", default_terms()), std::runtime_error);

    const auto bad_weekly = make_table({
        { "Soft Constraint 1", "max_weekly_remote", "XA two", "Weight=1" },
        { "Date", "XA" },
        { "Mon", "x" },
    });
    CHECK_THROWS_AS(parse_schedule(bad_weekly, constraint_re, "^Date$", default_terms()), std::runtime_error);
}

TEST_CASE("ScheduleCoverage: repeated headers define weeks and must preserve staff mapping"){
    const auto t = make_table({
        { "Hard Constraint 1", "onsite", "any 1" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "x" },
        { "Date", "XA", "XB" },
        { "Tue", "x", "x" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    REQUIRE(schedule.days.size() == 2);
    CHECK(schedule.days[0].week_index == 0);
    CHECK(schedule.days[1].week_index == 1);

    const auto mismatch = make_table({
        { "Hard Constraint 1", "onsite", "any 1" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "x" },
        { "Date", "XB", "XA" },
        { "Tue", "x", "x" },
    });
    CHECK_THROWS_AS(parse_schedule(mismatch, constraint_re, "^Date$", default_terms()), std::runtime_error);
}

TEST_CASE("ScheduleCoverage: constraint model rejects unknown staff"){
    const auto t = make_table({
        { "Hard Constraint 1", "srs", "XA OR ZZ" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "x" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    CHECK_THROWS_AS(build_constraint_model(schedule), std::runtime_error);
}

TEST_CASE("ScheduleCoverage: max consecutive remote skips vacation and holidays"){
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
    CHECK(p[0] == 1);
}

TEST_CASE("ScheduleCoverage: exclusivity is at-most-one and counts onsite Prim and Sec as present"){
    const auto t = make_table({
        { "Soft Constraint 1", "exclusivity", "XA XOR XB", "Weight=1.5" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "Prim" },
        { "Tue", "x", "Sec" },
        { "Wed", "x", "x" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    const auto model = build_constraint_model(schedule);

    const std::vector<std::vector<int64_t>> assignment = {
        { 0 }, // XA onsite + XB Prim -> violation
        {},    // XA remote + XB Sec -> allowed
        {},    // neither onsite -> allowed: mutual exclusion, not exactly-one
    };
    const auto p = evaluate_soft_penalties(schedule, model, assignment);
    REQUIRE(p.size() == 1);
    CHECK(p[0] == 1);
}

TEST_CASE("ScheduleCoverage: max weekly remote uses repeated header blocks"){
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
    CHECK(p[0] == 1);
}

TEST_CASE("ScheduleCoverage: baseline is lexicographically hard optimal"){
    RequirementModel model;
    model.subsets = { { 0, 1 }, { 1 } };
    model.min_onsite = { 1, 1 };

    Day day;
    day.classes = { CellClass::Undecided, CellClass::Undecided };
    day.cells = { "x", "x" };

    const auto candidates = generate_day_candidates(day, model);
    REQUIRE(candidates.size() == 4);

    const auto choice = select_baseline({ candidates });
    REQUIRE(choice.size() == 1);
    CHECK(candidates[choice[0]].violation == std::vector<int64_t>({ 0, 0 }));
}

TEST_CASE("ScheduleCoverage: NVariations is exact when enough hard-optimal schedules exist even with zero annealing"){
    const auto t = make_table({
        { "Hard Constraint 1", "onsite", "any 1" },
        { "Date", "XA", "XB", "XC" },
        { "Mon", "x", "x", "x" },
        { "Tue", "x", "x", "x" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    const auto model = build_constraint_model(schedule);

    SolverConfig config;
    config.n_variations = 5;
    config.annealing_iterations = 0;

    const auto solutions = produce_variations(schedule, model, config);
    REQUIRE(solutions.size() == 5);

    std::set<std::string> seen;
    for(const auto &s : solutions){
        CHECK(s.hard_optimal);
        CHECK(s.violation_sum == std::vector<int64_t>({ 0 }));
        CHECK(s.annealing_runs == 0);
        CHECK(s.annealing_proposals == 0);
        seen.insert(signature({ s }));
    }
    CHECK(seen.size() == 5);
}

TEST_CASE("ScheduleCoverage: AnnealingIterations controls the exact proposal count"){
    const auto t = make_table({
        { "Hard Constraint 1", "onsite", "any 1" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "x" },
        { "Tue", "x", "x" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    const auto model = build_constraint_model(schedule);

    SolverConfig config;
    config.n_variations = 2;
    config.annealing_iterations = 7;
    config.seed = 12;

    const auto solutions = produce_variations(schedule, model, config);
    REQUIRE(solutions.size() == 2);
    for(const auto &s : solutions){
        CHECK(s.annealing_runs == 5);
        CHECK(s.annealing_proposals == 35);
        CHECK(s.requested_variations == 2);
        CHECK(s.returned_variations == 2);
    }
}

TEST_CASE("ScheduleCoverage: hard constraints stay hard even with zero RequirementViolationWeight"){
    // Hard any-1 requires XA onsite. XB is Prim, so the high-weight soft exclusivity rule wants XA remote.
    // The old implementation could return the hard-invalid all-remote state when hard weight was low.
    const auto t = make_table({
        { "Hard Constraint 1", "onsite", "any 1" },
        { "Soft Constraint 1", "exclusivity", "XA XOR XB", "Weight=1000" },
        { "Date", "XA", "XB" },
        { "Mon", "x", "Prim" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    const auto model = build_constraint_model(schedule);

    SolverConfig config;
    config.n_variations = 1;
    config.annealing_iterations = 200;
    config.requirement_violation_weight = 0.0;
    config.fairness_weight = 0.0;
    config.preference_weight = 0.0;
    config.seed = 3;

    const auto solutions = produce_variations(schedule, model, config);
    REQUIRE(solutions.size() == 1);
    CHECK(solutions[0].hard_optimal);
    CHECK(solutions[0].violation_sum == std::vector<int64_t>({ 0 }));
    CHECK(solutions[0].soft_penalty == std::vector<int64_t>({ 1 }));
}

TEST_CASE("ScheduleCoverage: RequirementViolationWeight changes final cost for unavoidable deficits"){
    const auto t = make_table({
        { "Hard Constraint 1", "onsite", "any 2" },
        { "Date", "XA" },
        { "Mon", "remote" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    const auto model = build_constraint_model(schedule);

    SolverConfig low;
    low.n_variations = 1;
    low.annealing_iterations = 0;
    low.fairness_weight = 0.0;
    low.preference_weight = 0.0;
    low.requirement_violation_weight = 2.0;

    SolverConfig high = low;
    high.requirement_violation_weight = 7.0;

    const auto a = produce_variations(schedule, model, low);
    const auto b = produce_variations(schedule, model, high);
    REQUIRE(a.size() == 1);
    REQUIRE(b.size() == 1);
    CHECK(a[0].violation_sum == std::vector<int64_t>({ 2 }));
    CHECK(b[0].violation_sum == std::vector<int64_t>({ 2 }));
    CHECK(a[0].annealing_cost == doctest::Approx(4.0));
    CHECK(b[0].annealing_cost == doctest::Approx(14.0));
}

TEST_CASE("ScheduleCoverage: deterministic for fixed seed and iteration count"){
    const auto t = make_table({
        { "Hard Constraint 1", "onsite", "any 1" },
        { "Soft Constraint 1", "max_consecutive_remote", "1", "Weight=2" },
        { "Date", "XA", "XB", "XC" },
        { "Mon", "x", "Remote", "x" },
        { "Tue", "Remote", "x", "x" },
        { "Wed", "x", "x", "Remote" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    const auto model = build_constraint_model(schedule);

    SolverConfig config;
    config.n_variations = 3;
    config.annealing_iterations = 100;
    config.seed = 12345;

    const auto a = produce_variations(schedule, model, config);
    const auto b = produce_variations(schedule, model, config);
    CHECK(signature(a) == signature(b));
}

TEST_CASE("ScheduleCoverage: rendering resolves all mutable cells and reports hard deficits"){
    const auto t = make_table({
        { "Hard Constraint 1", "onsite", "any 2" },
        { "Date", "XA", "XB" },
        { "Mon", "Remote", "remote" },
    });
    const auto schedule = parse_schedule(t, constraint_re, "^Date$", default_terms());
    const auto model = build_constraint_model(schedule);

    SolverConfig config;
    config.n_variations = 1;
    config.annealing_iterations = 0;

    const auto solutions = produce_variations(schedule, model, config);
    REQUIRE(solutions.size() == 1);
    REQUIRE(solutions[0].violation_sum == std::vector<int64_t>({ 1 }));

    const auto rendered = render_variation(t, schedule, solutions[0], &config);
    const auto values = collect_values(rendered);

    CHECK(values.count("Remote") == 0);  // Preference has been resolved to final onsite/remote state.
    CHECK(values.count("x") == 0);
    CHECK(count_value(rendered, "FLAG") == 1);
    CHECK(contains_prefix(rendered, "onsite_count=1, required>=2, deficit=1"));
    CHECK(values.count("OBJECTIVES") == 1);
    CHECK(values.count("SEARCH") == 1);
    CHECK(contains_prefix(rendered, "hard_optimal=true"));
    CHECK(contains_prefix(rendered, "annealing_proposals=0"));
}

TEST_CASE("ScheduleCoverage: operation docs expose every solver control and safe default constraint regex"){
    const auto doc = OpArgDocScheduleCoverage();

    const auto find_arg = [&](const std::string &name) -> const OperationArgDoc* {
        const auto it = std::find_if(doc.args.begin(), doc.args.end(),
            [&](const auto &arg){ return arg.name == name; });
        return (it == doc.args.end()) ? nullptr : &(*it);
    };

    REQUIRE(find_arg("RequirementRegex") != nullptr);
    CHECK(find_arg("RequirementRegex")->default_val.find("Hard") != std::string::npos);
    CHECK(find_arg("RequirementRegex")->default_val.find("Soft") != std::string::npos);

    CHECK(find_arg("NVariations") != nullptr);
    CHECK(find_arg("AnnealingIterations") != nullptr);
    CHECK(find_arg("RequirementViolationWeight") != nullptr);
    CHECK(find_arg("FairnessWeight") != nullptr);
    CHECK(find_arg("PreferenceWeight") != nullptr);
    CHECK(find_arg("Seed") != nullptr);

    CHECK(find_arg("HolidayTerms") != nullptr);
    CHECK(find_arg("VacationTerms") != nullptr);
    CHECK(find_arg("ImmutableTerms") != nullptr);
    CHECK(find_arg("OnsiteTerms") != nullptr);
    CHECK(find_arg("RemotePreferenceTerms") != nullptr);
    CHECK(find_arg("UndecidedTerms") != nullptr);
    CHECK(find_arg("RemoteTerms") != nullptr);
    CHECK(find_arg("TermMatchMode") != nullptr);
}
