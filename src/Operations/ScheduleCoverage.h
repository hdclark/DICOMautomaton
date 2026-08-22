// ScheduleCoverage.h.

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "../Structs.h"
#include "../Tables.h"


OperationDoc OpArgDocScheduleCoverage();

bool ScheduleCoverage(Drover &DICOM_data,
                      const OperationArgPkg& /*OptArgs*/,
                      std::map<std::string, std::string>& /*InvocationMetadata*/,
                      const std::string& /*FilenameLex*/);


// The core of this operation is factored into free functions (below) so that it can be exercised directly by unit
// tests without constructing a full Drover fixture.
namespace ScheduleCoverageCore {

// Classification of an individual schedule cell.
enum class CellClass {
    Immutable,          // Fixed, does not count toward on-site quotas (Vac, CTO, Prim, Sec, unknown, ...).
    Holiday,            // Day-level skip marker.
    Onsite,             // Fixed on-site.
    RemotePreference,   // Mutable; prefers remote but may be overridden to on-site.
    Undecided,          // Mutable; must be assigned on-site or remote.
    Remote,             // Fixed remote.
};

// User-overridable term lists used to classify cells.
struct TermLists {
    std::vector<std::string> holiday;
    std::vector<std::string> immutable;
    std::vector<std::string> onsite;
    std::vector<std::string> remote_pref;
    std::vector<std::string> undecided;
    std::vector<std::string> remote;
    bool regex_mode = false; // When true, terms are treated as (case-insensitive) regular expressions.
};

// A parsed coverage requirement.
struct Requirement {
    std::string label;
    std::string type;
    std::string quota_raw;
    std::vector<std::string> subset; // Staff names; empty means "all staff".
    int64_t min_onsite = 1;
};

// A parsed schedule day.
struct Day {
    std::string date;
    int64_t row = -1;               // Row index of this day in the source table.
    bool holiday = false;           // True when every cell of the day is a holiday marker.
    std::vector<std::string> cells; // Raw cell text, indexed by staff.
    std::vector<CellClass> classes; // Classification, indexed by staff.
};

// The fully parsed schedule.
struct ParsedSchedule {
    std::vector<Requirement> requirements;
    std::vector<std::string> staff;
    std::vector<int64_t> staff_columns; // Column index of each staff name in the source table.
    std::vector<Day> days;
};

// Requirement model with staff names resolved to indices.
struct RequirementModel {
    std::vector<std::vector<int64_t>> subsets; // Staff indices per requirement.
    std::vector<int64_t> min_onsite;
};

// A candidate assignment for a single day.
struct DayCandidate {
    std::vector<int64_t> onsite;     // Staff indices assigned on-site (fixed + chosen mutable).
    std::vector<int64_t> overridden; // Staff indices whose RemotePreference was overridden.
    std::vector<int64_t> violation;  // Per-requirement deficit.
};

// A complete solution (one schedule variation).
struct Solution {
    std::vector<std::vector<int64_t>> day_onsite;     // Per day (indexed like schedule.days; holiday => empty).
    std::vector<std::vector<int64_t>> day_overridden;
    std::vector<std::vector<int64_t>> day_violation;  // Per-day, per-requirement deficit (holiday => empty).
    std::vector<int64_t> violation_sum;               // Total per-requirement deficit.
    int64_t overrides = 0;
    double fairness = 0.0;                            // Unweighted fairness metric value.
    std::vector<int64_t> staff_onsite;                // Per-staff on-site tally.
};

// Solver configuration.
struct SolverConfig {
    std::string fairness_metric = "range"; // "range", "variance", or "gini".
    double fairness_weight = 1.0;
    double preference_weight = 1.0;
    int64_t seed = 0;
    int64_t n_variations = 3;
};

// -------------------------------- Parsing & classification ---------------------------------

// Classify a single cell. When 'matched_known' is non-null it is set to true iff the cell matched at least one of the
// provided term lists (i.e., it was not an unknown term that fell back to Immutable).
CellClass classify_cell(const std::string &raw,
                        const TermLists &terms,
                        bool *matched_known = nullptr);

// Parse the given table into requirements, staff, and days.
//
// Note: throws std::runtime_error on malformed input.
ParsedSchedule parse_schedule(const tables::table2 &table,
                              const std::string &requirement_regex,
                              const std::string &header_regex,
                              const TermLists &terms);

// Parse a quota expression into a (minimum on-site count, staff subset) pair. Returns false when the quota cannot be
// understood. An empty subset denotes "all staff".
bool parse_quota(const std::string &quota,
                 int64_t &min_onsite,
                 std::vector<std::string> &subset);

// Resolve requirement subsets (staff names) to staff indices. Throws when a requirement references unknown staff.
RequirementModel build_requirement_model(const ParsedSchedule &schedule);

// -------------------------------- Optimization ---------------------------------

// Evaluate the per-requirement deficit of a given on-site assignment.
std::vector<int64_t> evaluate_violation(const std::vector<int64_t> &onsite,
                                        const RequirementModel &model);

// Phase A: enumerate the feasible assignments of a day's mutable cells and prune strictly-dominated candidates.
std::vector<DayCandidate> generate_day_candidates(const Day &day,
                                                  const RequirementModel &model);

// Phase B: choose the per-day candidate that is lexicographically minimal in its violation vector (tie-broken by
// override count). The returned vector is indexed by day; an entry is meaningless for holiday days (empty candidate
// lists).
std::vector<size_t> select_baseline(const std::vector<std::vector<DayCandidate>> &day_candidates);

// Evaluate a fairness metric over a per-staff on-site tally.
double fairness_penalty(const std::vector<int64_t> &staff_onsite,
                        const std::string &metric);

// Phases B-D: produce up to 'config.n_variations' Pareto-spread solutions.
std::vector<Solution> produce_variations(const ParsedSchedule &schedule,
                                         const RequirementModel &model,
                                         const SolverConfig &config);

// -------------------------------- Rendering ---------------------------------

// Render a solution as a full table copy (with all immutable/holiday cells preserved verbatim) plus an appended report
// block.
tables::table2 render_variation(const tables::table2 &original,
                                const ParsedSchedule &schedule,
                                const Solution &solution);

} // namespace ScheduleCoverageCore
