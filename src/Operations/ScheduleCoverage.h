// ScheduleCoverage.h.

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "../Structs.h"
#include "../Tables.h"

OperationDoc OpArgDocScheduleCoverage();

bool ScheduleCoverage(Drover &DICOM_data,
                      const OperationArgPkg& /*OptArgs*/,
                      std::map<std::string, std::string>& /*InvocationMetadata*/,
                      const std::string& /*FilenameLex*/);

namespace ScheduleCoverageCore {

enum class CellClass {
    Immutable,
    Vacation,
    Holiday,
    Onsite,
    RemotePreference,
    Undecided,
    Remote,
};

struct TermLists {
    std::vector<std::string> holiday;
    std::vector<std::string> vacation;
    std::vector<std::string> immutable;
    std::vector<std::string> onsite;
    std::vector<std::string> remote_pref;
    std::vector<std::string> undecided;
    std::vector<std::string> remote;
    bool regex_mode = false;
};

// Hard constraints are per-day minimum on-site coverage requirements.
struct Requirement {
    std::string label;
    std::string type;
    std::string quota_raw;
    std::vector<std::string> subset; // Empty means all staff.
    int64_t min_onsite = 1;
};

enum class SoftConstraintKind {
    MaxConsecutiveRemote,
    Exclusivity,
    MaxWeeklyRemote,
};

// Soft constraints carry their own weight directly in the input table.
struct SoftConstraint {
    std::string label;
    std::string type;
    std::string expression_raw;
    double weight = 1.0;
    SoftConstraintKind kind = SoftConstraintKind::MaxConsecutiveRemote;
    std::vector<std::string> staff; // Empty for all-staff constraints.
    int64_t limit = 0;
};

struct Day {
    std::string date;
    int64_t row = -1;
    int64_t week_index = 0;          // Repeated Date headers start a new week block.
    bool holiday = false;
    std::vector<std::string> cells;
    std::vector<CellClass> classes;
};

struct ParsedSchedule {
    std::vector<Requirement> requirements;
    std::vector<SoftConstraint> soft_constraints;
    std::vector<std::string> staff;
    std::vector<int64_t> staff_columns;
    std::vector<Day> days;
};

struct RequirementModel {
    std::vector<std::vector<int64_t>> subsets;
    std::vector<int64_t> min_onsite;
};

struct SoftConstraintModel {
    std::string label;
    std::string type;
    double weight = 1.0;
    SoftConstraintKind kind = SoftConstraintKind::MaxConsecutiveRemote;
    std::vector<int64_t> staff;
    int64_t limit = 0;
};

struct ConstraintModel {
    RequirementModel hard;
    std::vector<SoftConstraintModel> soft;
};

struct DayCandidate {
    std::vector<int64_t> onsite;
    std::vector<int64_t> overridden;
    std::vector<int64_t> violation;
};

struct Solution {
    std::vector<std::vector<int64_t>> day_onsite;
    std::vector<std::vector<int64_t>> day_overridden;
    std::vector<std::vector<int64_t>> day_violation;
    std::vector<int64_t> violation_sum;
    std::vector<int64_t> soft_penalty;
    std::vector<int64_t> staff_onsite;

    int64_t hard_violation_units = 0;
    int64_t overrides = 0;
    double fairness = 0.0;
    double soft_constraint_cost = 0.0;
    double annealing_cost = 0.0;

    bool hard_optimal = false;       // Matches the provably lexicographic-optimal hard-violation vector.
    bool pareto_nondominated = false;

    // Search diagnostics are copied onto every returned solution so rendered tables are auditable.
    int64_t annealing_runs = 0;
    int64_t annealing_proposals = 0;
    int64_t requested_variations = 0;
    int64_t returned_variations = 0;
};

struct SolverConfig {
    std::string fairness_metric = "range";
    double fairness_weight = 1.0;
    double preference_weight = 1.0;

    // This weight controls the cost of *temporary* hard-constraint excursions during annealing and
    // contributes to the reported scalar cost for unavoidable deficits. Returned solutions are
    // always restricted to the lexicographically optimal hard-constraint surface.
    double requirement_violation_weight = 1000.0;

    int64_t annealing_iterations = 100000; // Proposals per annealing run. 0 disables annealing.
    int64_t seed = 0;
    int64_t n_variations = 3;
};

CellClass classify_cell(const std::string &raw,
                        const TermLists &terms,
                        bool *matched_known = nullptr);

bool parse_quota(const std::string &quota,
                 int64_t &min_onsite,
                 std::vector<std::string> &subset);

// The legacy argument name RequirementRegex now identifies both Hard Constraint and Soft Constraint rows.
// Constraint-looking rows excluded by this regex are rejected rather than silently ignored.
ParsedSchedule parse_schedule(const tables::table2 &table,
                              const std::string &constraint_regex,
                              const std::string &header_regex,
                              const TermLists &terms);

ConstraintModel build_constraint_model(const ParsedSchedule &schedule);

std::vector<int64_t> evaluate_violation(const std::vector<int64_t> &onsite,
                                        const RequirementModel &model);

std::vector<int64_t> evaluate_soft_penalties(const ParsedSchedule &schedule,
                                             const ConstraintModel &model,
                                             const std::vector<std::vector<int64_t>> &day_onsite);

std::vector<DayCandidate> generate_day_candidates(const Day &day,
                                                  const RequirementModel &model);

std::vector<size_t> select_baseline(const std::vector<std::vector<DayCandidate>> &day_candidates);

double fairness_penalty(const std::vector<int64_t> &staff_onsite,
                        const std::string &metric);

std::vector<Solution> produce_variations(const ParsedSchedule &schedule,
                                         const ConstraintModel &model,
                                         const SolverConfig &config);

tables::table2 render_variation(const tables::table2 &original,
                                const ParsedSchedule &schedule,
                                const Solution &solution,
                                const SolverConfig *config = nullptr);

} // namespace ScheduleCoverageCore
