# Broad Plan: Automated On-Site Coverage Scheduler (DICOMautomaton Operation)

## 1. Purpose and Scope

Add a new DICOMautomaton *Operation* that ingests a staff-rostering schedule held in a
`Sparse_Table` (see `src/Tables.h`, `src/Structs.h`), classifies every cell robustly,
solves an optimization problem that fills in undecided/overrideable entries to satisfy a
prioritized list of on-site coverage requirements, balances long-term fairness across
staff, and emits several table variations near the Pareto front, each with an appended
report for auditing.

Primary requirements (from `prompt.md`):

1. Robustly accept the schedule as a `tables::table2` (as loaded by the existing
   `LoadFiles`/`CSV_File_Loader` path) in the format of
   `artifacts/test_files/20260821_DCMA_schedule_template.tsv`.
2. Robustly classify table cells (mutable vs. immutable, counts-as-onsite vs. not).
3. Produce high-quality optimization in <= ~30 s on a typical workstation.
4. Honor staff "Remote" preferences where possible.
5. Emit a user-selectable number of schedule variations on/near the Pareto front.
6. Append a per-variation report: (a) requirement-failure flags, (b) overridden
   remote-preference indications, (c) remote-day tally per user.
7. Use **no third-party dependency** other than the existing `Ygor` library
   (`https://github.com/hdclark/Ygor`).
8. Strict **C++17**, following local style/conventions.

## 2. Existing Infrastructure (what the new code reuses)

The operation follows the established pattern seen in `src/Operations/*.cc`:

- Function signature (see `src/Operations/WidenTable.cc`, `ConvertParametersToTable.cc`):
  ```cpp
  bool <Name>(Drover& DICOM_data,
              const OperationArgPkg& OptArgs,
              std::map<std::string, std::string>& InvocationMetadata,
              const std::string& FilenameLex);
  ```
- `OperationDoc OpArgDoc<Name>()` documents the operation and its arguments
  (`src/Structs.h`: `OperationDoc`, `OperationArgDoc`, `OpArgSamples`, `OpArgVisibility`).
- Table access via `Sparse_Table` -> `tables::table2` (`src/Structs.h:320`,
  `src/Tables.h`). Key API: `value(r,c)`, `inject(r,c,v)`, `min_max_row/col`,
  `next_empty_row`, `find_cells`, `get_specifiers`, `visit_standard_block`.
- Table selection via `All_STs(...)` + `Whitelist(...)` + `STWhitelistOpArgDoc()`
  (`src/Regex_Selectors.h:300-330`).
- Helper utilities: `Compile_Regex` (`src/Regex_Selectors.h`),
  `SplitStringToVector`, `Is_String_An_X`, `stringtoX` (from `YgorString.h`),
  logging macros `YLOGINFO`/`YLOGWARN`/`YLOGERR` (`YgorLog.h`, `YgorMisc.h`).
- Table metadata coalescing via `coalesce_metadata_for_basic_table` and `meta_evolve`
  (`src/Metadata.h`) and `coalesce_metadata_for_basic_table` usage in
  `ConvertParametersToTable.cc`.
- CSV/TSV parsing is already handled by `tables::table2::read_csv` (auto-detects tabs,
  handles quoted cells); the operation does not re-implement file reading.

## 3. Input Format and Parsing Model

The input table (see `artifacts/test_files/20260821_DCMA_schedule_template.tsv`) has two
logical regions interleaved in a single sparse table:

1. **Requirements region** (rows near the top). Each requirement row has:
   - column 0: a label matching `Requirement <N>` (e.g. `Requirement 1`);
   - column 1: a type/name: `onsite`, or a named subgroup (`srs`, `livsabr`);
   - column 2: a quota expression: `any 2`, or a staff OR-list
     (`XC OR XD OR XE OR XF OR XG`).
2. **Schedule region**. One or more "week" blocks separated by blank rows, each
   beginning with a header row (`Date`, `XA`, `XB`, ... `XK`) followed by date rows whose
   cells are per-staff statuses.

The parser must produce a normalized model:

- `requirements`: ordered list of `{ label, type, quota }` (order = priority).
- `staff`: ordered list of staff names (from header columns).
- `days`: ordered list of `{ date, per-staff cell }`, plus a per-day "holiday" flag.

### 3.1 Cell classification (robustness requirement)

Cell classification is driven by user-overridable term lists (comma-separated,
case-insensitive, matched exactly by default with an optional regex mode). Defaults that
mirror the template:

| Category                  | Default terms                | Semantics                                    |
|---------------------------|------------------------------|----------------------------------------------|
| Immutable / non-counting  | `Vac`, `CTO`, `Prim`, `Sec`  | Fixed; do not count toward on-site quotas    |
| Holiday (day-level)       | `Holiday`                    | Day skipped by optimization entirely         |
| Fixed on-site             | `onsite`                     | Counts on-site; not changed                  |
| Remote preference         | `Remote`                     | Counts remote; may be overridden to on-site  |
| Undecided                 | `x`                          | Must become on-site or remote                |
| Fixed remote              | `remote`                     | Counts remote; not changed by default        |

A term that matches no known category is treated as immutable/non-counting and a warning
is logged (robustness). Case is normalized before matching. The end-user can add/remove
terms per category via operation arguments, and can opt in to regex matching for each
category.

### 3.2 Requirement quota grammar

The `quota` string is parsed into a `(subset_of_staff, minimum_onsite_count)` pair:

- `any <N>` / `<N>` / `any` -> subset = all staff, minimum = N (default 1).
- `A OR B OR C` -> subset = {A,B,C}, minimum = 1 (default; extendable to a prefixed
  count, e.g. `any 2 of A OR B OR C`, but the minimum viable implementation only needs
  "at least 1 of" for OR-lists).
- Unknown/garbage quota -> rejected with a clear error naming the requirement row.

Each requirement thus evaluates to: on a given day, `onsite_count(subset) >= minimum`.

## 4. Optimization Model

### 4.1 Variables

For each day and each staff cell that is **mutable** (`x` or `Remote`), a binary decision
`onsite = 0/1`. Fixed cells are constants (on-site = 1 for `onsite`; 0 otherwise; holiday/
immutable days contribute no staff). Holiday days contribute no variables.

### 4.2 Objectives (lexicographic priority)

The overall objective is evaluated in priority order (matches "requirements in priority
order, then fairness"):

1. **Requirement satisfaction, lexicographic.** For each day, define the violation vector
   `v = (v_1, v_2, ..., v_R)` where `v_i = max(0, quota_i - onsite_count(subset_i))`.
   Minimize `sum over days of v_1` first, then `sum of v_2`, etc. (Requirement 1 is more
   important than 2, etc.). If feasible, all sums are zero.
2. **Fairness.** Minimize imbalance of the total assigned on-site days across staff.
   Configurable metric (see §6), default = range (max - min) with variance/Gini as
   alternatives. Only mutable and fixed `onsite` days count; `Prim`/`Sec`/`Vac`/`CTO`
   and holidays do not.
3. **Preference honoring.** Minimize the number of `Remote -> onsite` overrides (weighted
   higher than the "cost" of assigning `x -> onsite`, so preferences are honored before
   free cells are used).

### 4.3 Algorithm (decoupled per-day candidates + global selection)

The key structural observation: **coverage requirements are per-day and independent**;
only fairness and preference objectives couple across days. This lets the solver be fast,
deterministic, and explainable:

**Phase A — per-day candidate generation.**
For each non-holiday day, enumerate the feasible assignments of the day's mutable cells
(at most `2^M`, `M <= ~11` staff -> <= 2048, typically far fewer) and keep the
Pareto-minimal candidates in the space `(violation_vector, override_count)`. A candidate
records: its violation vector, its override count, and the set of staff it places on-site.
This reduces each day to a small set of candidate "cover patterns".

**Phase B — global multi-choice selection (feasibility).**
Choose one candidate per day to minimize the lexicographic sum of violation vectors.
Because the days are independent, the lexicographic optimum is obtained by, per objective
level in order, choosing per-day candidates that minimize that level's violation, subject
to not worsening already-fixed higher-priority levels. This yields a baseline solution
that is provably optimal for the requirement objective.

**Phase C — fairness/preference refinement (local search).**
Starting from the Phase B baseline, perform deterministic local search (tabu search or
simulated annealing with a fixed seed) that swaps per-day candidates to reduce the scalar
secondary objective `f = w_fair * FairnessPenalty + w_pref * OverrideCount`, never
increasing the (lexicographic) requirement objective. This maintains coverage optimality
while improving fairness/preference.

**Phase D — Pareto front / variations.**
Re-run Phase C over a small set of `(w_fair, w_pref)` weight combinations and/or distinct
seeds, collect the resulting solutions, drop duplicates, and project onto the Pareto front
(non-dominated in `(requirement_violation, fairness, overrides)`). Return up to
`NVariations` solutions (user-selected), prioritizing spread across the front.

Complexity: Phase A is trivial; Phases B/C/D operate on per-day candidate sets (a few
hundred total candidates) with a few thousand iterations — well within the 30 s budget
and easily parallelizable via the existing `Thread_Pool` if needed (not required).

## 5. Report Generation (appended to each output table)

Each output variation is a full copy of the input table with:
- every mutable `x` cell replaced by `onsite` or `remote`;
- every `Remote` cell either left as `remote` (preference honored) or replaced by `onsite`
  (override);
- all immutable/holiday cells preserved verbatim.

A report block is appended below the last schedule row, using reserved leading tokens in
column 0 so it is machine-parseable and clearly separated from schedule data:

```
(blank row)
"== Schedule Report =="
"FLAG",   <date>, <Requirement label>, "deficit"=<n>
"OVERRIDE", <date>, <staff>, "Remote -> onsite"
"TALLY",  <staff>, "onsite"=<n>, "remote"=<n>, "vacation"=<n>, "other"=<n>
"OBJECTIVES", "violations"=<v1>,<v2>,... , "fairness"=<metric value>, "overrides"=<n>
```

- `FLAG` rows appear for every day+requirement whose quota cannot be met (deficit > 0).
- `OVERRIDE` rows enumerate every `Remote -> onsite` change (requirement 2 of the report).
- `TALLY` rows provide the remote-day (and on-site/vacation) tally per staff (requirement 3).
- `OBJECTIVES` records the scalar objectives for downstream tooling/automated checks.

Output tables are appended to `DICOM_data.table_data`, each carrying metadata keys such as
`TableLabel`, `ScheduleVariation`, `NormalizedTableLabel`, and objective values.

## 6. Operation Interface (arguments)

| Argument | Default | Description |
|----------|---------|-------------|
| `TableSelection` | `last` | Standard ST whitelist (`STWhitelistOpArgDoc`). |
| `RequirementRegex` | `^Requirement` | Regex identifying requirement-label cells in column 0. |
| `HeaderRegex` | `^Date$` | Regex identifying schedule header rows. |
| `HolidayTerms` | `Holiday` | Day-level skip terms. |
| `ImmutableTerms` | `Vac,CTO,Prim,Sec` | Fixed, non-counting terms. |
| `OnsiteTerms` | `onsite` | Fixed on-site terms. |
| `RemotePreferenceTerms` | `Remote` | Mutable remote-preference terms. |
| `UndecidedTerms` | `x` | Mutable undecided terms. |
| `RemoteTerms` | `remote` | Fixed remote terms. |
| `TermMatchMode` | `exact` | `exact` or `regex` matching for all term categories. |
| `NVariations` | `3` | Number of output variations to produce. |
| `FairnessMetric` | `range` | `range`, `variance`, or `gini`. |
| `PreferenceWeight` | `1.0` | Weight on override minimization (secondary). |
| `FairnessWeight` | `1.0` | Weight on fairness (secondary). |
| `Seed` | `0` | RNG/local-search seed for reproducibility. |

Defaults are chosen so that running the operation on the shipped template with no extra
arguments produces sensible output.

## 7. Files, Registration, and Integration

New source files (matching existing one-op-per-file convention):
- `src/Operations/ScheduleCoverage.h`
- `src/Operations/ScheduleCoverage.cc`

Registration points (mirror `WidenTable`):
1. Add `ScheduleCoverage.cc` to the `add_library(Operations_objs OBJECT ...)` list in
   `src/Operations/CMakeLists.txt`.
2. Add `#include "Operations/ScheduleCoverage.h"` in `src/Operation_Dispatcher.cc`.
3. Add `out["ScheduleCoverage"] = std::make_pair(OpArgDocScheduleCoverage, ScheduleCoverage);`
   to `Known_Operations()` in `src/Operation_Dispatcher.cc`.

The operation should declare a `category: table processing` tag (and any relevant
medical-physics/planning category) for discoverability, consistent with other table ops.

## 8. Verification and Validation Strategy

Verification and validation are first-class deliverables, not an afterthought.

**8.1 Unit tests (compile-time/in-process, no external deps).**
Add self-contained test entry point(s) following the existing pattern in
`src/Operations/*_Tests.cc` (or a dedicated `ScheduleCoverage_Tests.cc`) covering:
- requirement-row and header-row detection (including quoted cells, extra blank rows);
- quota-expression parsing: `any 2`, `any`, `XC OR XD OR XE`, malformed input errors;
- cell classification: all categories, case-insensitivity, unknown-term warning path;
- holiday-day detection and skipping;
- candidate generation and lexicographic violation minimization on tiny synthetic cases
  with hand-computed optima;
- fairness/preference trade-off and Pareto-front pruning on a 2-staff/2-day toy problem;
- report rendering (FLAG/OVERRIDE/TALLY row presence and correctness).

**8.2 Integration test.**
Add `integration_tests/tests/ScheduleCoverage.sh` (following `ForEachDistinct.sh`
conventions: `set -eux`, `DCMA_BIN`, `TEST_FILES_ROOT`) that:
1. loads `artifacts/test_files/20260821_DCMA_schedule_template.tsv`;
2. runs the operation (default args and at least one non-default `NVariations`);
3. `grep`s the output stream for success markers and correct row counts (e.g. no `x`
   remains, expected number of output tables, report markers present).

**8.3 Invariant/property checks (asserted inside the operation, behind normal logging).**
- Immutable and holiday cells are byte-identical in every output.
- No `x`/undecided term remains in any output.
- Onsite count per day never exceeds the available (non-holiday) staff.
- The requirement objective of every emitted variation equals the lexicographic optimum
  (Phase B guarantee).
- Overrides only ever turn `Remote -> onsite`, never the reverse, and never touch fixed
  `remote`/`onsite` terms.

**8.4 Determinism and reproducibility.**
With fixed `Seed`, repeated runs must produce identical tables (asserted in tests).

**8.5 Performance.**
Benchmark the shipped template; assert wall-clock < 30 s on the CI runner (integration
test can include a soft timing guard). Report candidate-set sizes and search iterations
via `YLOGINFO` for manual review.

**8.6 Expert/clinical review checklist (documented in the PR).**
Provide: (a) the Pareto-front solutions for the shipped template, (b) an explainability
trace (which days/requirements drive which assignments), (c) the fairness tally, and
(d) a sensitivity note on the fairness metric and weights.

## 9. Edge Cases and Robustness Requirements

- Empty or malformed table -> clear `std::runtime_error` (or warning + no-op if empty).
- Requirements referencing staff not present in headers -> error naming the requirement.
- Staff columns with no header name -> error.
- Infeasible requirements -> solver still returns the best lexicographic solution and the
  report's `FLAG` rows identify the unmet days.
- Multiple week blocks / repeated headers -> parsed as a continuous day list (order
  preserved).
- A day with zero mutable cells -> treated as fully fixed; counted for fairness.
- Unknown terms in cells -> immutable/non-counting + warning (never silently dropped).

## 10. Constraints and Conventions

- **C++17 only** (no C++20 features); follow existing include ordering, `YLOG*` logging,
  `Compile_Regex`, `YgorString` helpers, and operation documentation style.
- **No new third-party dependencies.** Only `Ygor` and the C++ standard library. This
  means the optimizer must be a hand-written greedy/local-search (no external ILP/SAT
  solver). The decoupled per-day candidate + global selection design is chosen precisely
  to make this tractable and provably correct for the coverage objective.
- Operation is written so its core (parse/classify/optimize/report) is separable into
  free functions for unit testing without a full `Drover` fixture.
