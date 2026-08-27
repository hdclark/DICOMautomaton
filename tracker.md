# Implementation Tracker: Automated On-Site Coverage Scheduler

Checklist of items to implement, in recommended order. Each item is independent enough
to be tracked/PR'd separately where noted. Acceptance criteria (AC) are testable.

## Phase 0 — Setup & scaffolding

- [ ] **T0.1** Create `src/Operations/ScheduleCoverage.h` declaring
  `OpArgDocScheduleCoverage()` and the operation function with the standard 4-arg
  signature (mirror `src/Operations/WidenTable.h`).
- [ ] **T0.2** Create `src/Operations/ScheduleCoverage.cc` with the `OpArgDoc` (full
  argument list + `category: table processing` tag) and a stub implementation that
  returns `true` after logging.
- [ ] **T0.3** Register the operation: add to `src/Operations/CMakeLists.txt`, add the
  `#include` and `Known_Operations()` entry in `src/Operation_Dispatcher.cc`.
  - AC: project compiles; `dicomautomaton_dispatcher` lists `ScheduleCoverage`.

## Phase 1 — Parsing & classification

- [ ] **T1.1** Implement requirement-region scanning: detect `Requirement <N>` labels in
  column 0 (via `RequirementRegex`), read type (col 1) and quota (col 2) into an ordered
  `requirements` vector.
  - AC: unit test parses the 3 requirements from the shipped template.
- [ ] **T1.2** Implement schedule-region scanning: detect header rows (`HeaderRegex`),
  read staff names, then read date rows into `days` (order preserved across blocks/blank
  rows).
  - AC: unit test yields 11 staff and the correct number of date rows from the template.
- [ ] **T1.3** Implement term classification with per-category term lists and
  `TermMatchMode` (`exact`/`regex`), case-insensitive, plus the unknown-term warning path.
  - AC: unit test covers every category, case variants, regex mode, unknown-term warning.
- [ ] **T1.4** Implement holiday-day detection (all/normalized cells match `HolidayTerms`
  -> skip).
  - AC: template's `Sept 7` and `Sept 30` days are marked holiday.

## Phase 2 — Quota / requirement model

- [ ] **T2.1** Implement quota-expression parser -> `(subset_of_staff, min_onsite)`
  supporting `any <N>`, `<N>`, `any`, and `A OR B OR C`.
  - AC: unit tests for each form plus malformed-input errors.
- [ ] **T2.2** Implement the per-day violation-vector evaluator
  `v_i = max(0, quota_i - onsite_count(subset_i))`.
  - AC: unit test on fixed synthetic days with hand-computed vectors.

## Phase 3 — Optimizer

- [ ] **T3.1** Phase A: per-day candidate enumeration + Pareto-minimal pruning over
  `(violation_vector, override_count)`.
  - AC: toy-day unit test; candidate set size within bounds.
- [ ] **T3.2** Phase B: lexicographic multi-choice selection producing the provably
  optimal requirement solution.
  - AC: unit test asserts optimal violation vector on a constructed infeasible case.
- [ ] **T3.3** Phase C: deterministic local search (tabu/SA, fixed `Seed`) optimizing
  `w_fair*Fairness + w_pref*Overrides` without worsening requirements.
  - AC: improves baseline fairness/overrides on synthetic multi-day cases.
- [ ] **T3.4** Phase D: weight/seed sweep, dedup, Pareto-front projection, cap at
  `NVariations`.
  - AC: unit test on a small problem with known Pareto points; returns <= N distinct
  solutions.

## Phase 4 — Output & report

- [ ] **T4.1** Render each variation as a full table copy: `x -> onsite|remote`,
  `Remote -> remote|onsite`; preserve all immutable/holiday cells verbatim.
  - AC: invariant checks (no `x` remains; immutable cells byte-identical).
- [ ] **T4.2** Append report block (`FLAG`, `OVERRIDE`, `TALLY`, `OBJECTIVES` rows) per
  the format in `broad_plan.md` §5.
  - AC: unit test verifies presence/count of each row type.
- [ ] **T4.3** Append output tables to `DICOM_data.table_data` with variation metadata
  (`TableLabel`, `ScheduleVariation`, objective values).
  - AC: `NVariations` tables added; metadata correct.

## Phase 5 — Verification & validation

- [ ] **T5.1** Add `ScheduleCoverage_Tests.cc` (or `*_Tests.cc`) exercising Phases 1-4;
  wire into the existing unit-test build.
- [ ] **T5.2** Add `integration_tests/tests/ScheduleCoverage.sh` running the operation
  on the shipped template via the dispatcher with success-marker `grep`s.
- [ ] **T5.3** Add determinism test (fixed `Seed` -> identical outputs across two runs).
- [ ] **T5.4** Add performance guard: template runs < 30 s; log candidate/sizes and
  iteration counts via `YLOGINFO`.
- [ ] **T5.5** Run full local validation: build, unit tests, integration tests,
  `check_syntax.sh` (if applicable), and lint/style adherence.
- [ ] **T5.6** Produce the expert-review artifacts (Pareto solutions, explainability
  trace, fairness tally, sensitivity note) for the shipped template.

## Phase 6 — Documentation

- [ ] **T6.1** Ensure the `OpArgDoc` fully documents every argument, default, and the
  report format (so `compile_documentation.sh` generates a usable man page).
- [ ] **T6.2** Add an entry to `development_log.md` under the current month summarizing
  the new operation.
