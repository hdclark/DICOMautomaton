# Schedule Optimizer Implementation Tracker

Status convention: `[ ]` not started, `[~]` in progress, `[x]` complete, `[!]` blocked. Each completed
item should reference the implementing commit or validation evidence.

Validation evidence (2026-08-25): dependency-free standalone release and debug harnesses compiled the
Operation as strict C++17. All 18 focused doctest cases passed (312 assertions). The provided fixture
completed 250,000 proposals and emitted three schedules in 15.93 seconds in the release harness. Full
repository CMake configuration was blocked in this environment because its Ygor package configuration
was not discoverable, although the installed Ygor library and headers supported direct compilation.

## A. Contract and design approval

- [ ] Review `broad_plan.md` with scheduling stakeholders and record approval of mutable/immutable status semantics.
- [ ] Confirm default per-constraint status sets, especially treatment of `Prim` and `Sec`.
- [ ] Confirm that closure exclusion is opt-in and whether production invocations should explicitly use `ExcludeUnanimousStatuses=Holiday`.
- [ ] Confirm ISO Monday-to-Sunday week semantics.
- [ ] Confirm normalized cost formulas and initial production weights.
- [ ] Confirm the report schema and advisory-use disclaimer.

## B. Operation skeleton and registration

- [x] Add `src/Operations/OptimizeSchedule.h` with `OpArgDocOptimizeSchedule` and Operation declaration.
- [x] Add `src/Operations/OptimizeSchedule.cc` using C++17 and local style.
- [x] Document all arguments, defaults, validation limits, status policy syntax, and reproducibility limits in `OperationDoc`.
- [x] Add `OptimizeSchedule.cc` to `src/Operations/CMakeLists.txt`.
- [x] Include and register `OptimizeSchedule` in `src/Operation_Dispatcher.cc` and relevant header aggregation.
- [ ] Verify dispatcher help lists the Operation and every argument.

## C. Input model and parser

- [x] Implement ASCII trim/case-fold helpers while preserving original cell text.
- [x] Implement sparse row classification independent of absolute coordinates and contiguous storage.
- [x] Implement repeated `Date` header parsing and strict staff-column consistency checks.
- [x] Implement unique case-insensitive staff lookup with source coordinate diagnostics.
- [x] Enforce the initial unquoted staff identifier grammar `[A-Za-z0-9_.-]+`.
- [x] Implement locale-independent Gregorian date parsing for sample and ISO forms.
- [x] Validate weekday text, leap years, duplicate dates, and strictly increasing row order.
- [x] Implement ISO year/week derivation without timezone dependence.
- [x] Implement `ExcludeUnanimousStatuses` and report inactive dates.
- [x] Reject missing staff cells, malformed date-like rows in a schedule block, and zero active days.
- [x] Classify `x`, `Pref`, `Onsite`, `Onsite*`, `Remote`, known immutable statuses, and arbitrary immutable statuses.
- [x] Reject pre-existing `Schedule Optimizer Report` rows.
- [x] Reject no schedule, inconsistent shape, populated unknown columns, and no mutable cells.

## D. Constraint parser and validation

- [x] Parse finite non-negative weights and preserve source row identifiers.
- [x] Parse `minimum_onsite` and `group (name)` any-of expressions.
- [x] Parse `exclusivity (name)` xor expressions with required `any 1` semantics.
- [x] Parse `max_consecutive_remote` non-negative integer limits.
- [x] Parse one or more `max_weekly_remote` staff-limit assignments.
- [x] Parse no-expression `fairness_remote` and `fairness_overrides` rows.
- [x] Parse per-row `statuses=A|B|C` policies and apply documented defaults.
- [x] Reject unknown types, policy keys, malformed expressions, duplicate list members, and unknown staff.
- [x] Permit duplicate constraint types/names as independent source-row components.
- [x] Verify zero-weight constraints are labeled disabled/advisory, evaluated without active violation flags, and excluded from optimization and Pareto dominance.

## E. Exact evaluator

- [x] Implement candidate semantic-status lookup without rendering strings.
- [x] Implement normalized minimum onsite cost and daily deficit records.
- [x] Implement independent normalized group costs, including overlap cases.
- [x] Implement exclusivity excess cost and present-staff violation records.
- [x] Implement consecutive remote run cost and excess-day attribution.
- [x] Implement ISO weekly remote cost, weekly summaries, and deterministic excess-day attribution.
- [x] Implement opportunity-adjusted remote fairness and per-staff details.
- [x] Implement preference override mean/dispersion cost and override records.
- [x] Implement weighted objective summation with finite-value checks.
- [x] Implement optimizer-controlled and total semantic-status staff tallies.
- [x] Implement per-day upper-bound feasibility warnings for minimum/group constraints.
- [x] Compute custom-status feasibility bounds over both possible mutable outcomes.
- [ ] Add a debug/test assertion path that independently recomputes all scores.

## F. Simulated annealing

- [x] Implement fixed-seed `std::mt19937_64` ownership and documented per-chain seed mixing.
- [x] Ensure no random device, clock seed, global RNG, or `std::hash` affects search.
- [x] Implement preference-respecting all-remote baseline.
- [x] Implement exact-objective greedy coverage initialization.
- [x] Implement seeded perturbed chain initializers.
- [x] Implement single-flip proposals with complete variable reachability.
- [x] Implement same-day and same-staff swap proposals.
- [x] Implement violation-targeted repair proposals.
- [x] Implement Metropolis acceptance with safe exponent handling.
- [x] Implement automatic starting-temperature calibration and geometric cooling.
- [x] Implement deterministic iteration budgeting across chains.
- [x] Implement steady-clock runtime budgeting, periodic deadline checks, and report-time reserve.
- [~] Track current, chain-best, and global scalar-best candidates independently. Current and global best are explicit; accepted states and the final state are archived, but chain-best is not separately named.
- [x] Verify no proposal can alter an immutable cell.

## G. Pareto and elite archives

- [x] Define positive-weight component vectors in source-row order.
- [x] Implement tolerance-aware dominance and assignment deduplication.
- [x] Implement bounded non-dominated archive insertion/removal.
- [x] Implement deterministic crowding-distance pruning while preserving scalar best.
- [x] Implement bounded dominated elite pool for output fallback.
- [x] Implement diverse final selection with scalar best first.
- [x] Avoid duplicate outputs and handle decision spaces smaller than `OutputSchedules`.
- [x] Label each result as explored-front Pareto or dominated fallback.
- [x] Bound fallback generation and include selection/rendering in the runtime-mode deadline.

## H. Rendering and metadata

- [x] Deep-copy the selected input table for each result without modifying the source.
- [x] Render `x` decisions as `Onsite`/`Remote`.
- [x] Render `Pref` decisions as `Onsite*`/`Remote`.
- [x] Prove all immutable cell values and coordinates are byte-for-byte preserved.
- [x] Append a blank separator and deterministic machine-readable report records.
- [x] Render summary, component, daily violation, override, weekly, tally, excluded-day, and feasibility records.
- [x] Include human-readable direct violation descriptions with original names and dates.
- [x] Use classic locale and round-trip-safe numeric precision.
- [x] Set distinct table labels, normalized labels, descriptions, seed, objective, and result index metadata.
- [x] Hold all outputs temporarily and append to `Drover` only after every result renders successfully.
- [x] Add concise final logs and feasibility warnings without per-iteration noise.

## I. Unit verification

- [~] Add a dependency-free C++ test target suitable for parser/evaluator/search internals. Tests are embedded in the dispatcher; a standalone harness was used for validation but is not a repository CMake target.
- [x] Test loading the provided TSV through `table2::read_csv`.
- [~] Test sparse coordinates, separators, repeated headers, and unknown preserved rows. Sparse coordinates, separators, and repeated headers are covered; unknown preserved rows need a focused assertion.
- [ ] Test constraints after schedule blocks and malformed non-empty rows within schedule blocks.
- [ ] Test valid/invalid date forms, weekday variants, leap dates, ordering, and ISO weeks.
- [ ] Test every valid constraint form, policy override, case variant, and malformed form.
- [ ] Test exact score formulas with hand-computable schedules.
- [ ] Test overlapping groups and configurable `Prim`/`Sec` treatment.
- [ ] Test exclusivity for zero, one, and multiple present staff.
- [ ] Test remote runs across weekends and breaks on inactive/non-remote days.
- [ ] Test weekly limits spanning year and week boundaries.
- [ ] Test fairness with unequal/zero eligibility and all preferences overridden.
- [ ] Test direct violation attribution, feasibility warnings, and both tally definitions.
- [x] Test fixed-seed iteration-mode byte reproducibility.
- [ ] Test returned scores against full independent recomputation.
- [ ] Test archive uniqueness, non-dominance, pruning, and deterministic ordering.
- [x] Test impossible, zero-positive-weight, one-variable, and undersized decision-space cases.
- [ ] Test zero active days, partial/missing staff cells, custom-status feasibility bounds, and pre-existing report rejection.
- [ ] If incremental scoring is added, compare it to full scoring over seeded random move sequences.

## J. Integration verification

- [x] Add `integration_tests/tests/OptimizeSchedule.sh` in repository style.
- [x] Add the provided schedule template as the primary end-to-end fixture.
- [x] Assert the comma-delimited `.tsv` fixture parses as 11 constraints, 11 staff, 25 dated rows, and 2 unanimous Holiday rows.
- [ ] Add minimal malformed and impossible fixtures for negative/soft-failure cases.
- [ ] Verify requested unique output table count and unchanged source table.
- [ ] Verify all mutable cells resolve and immutable cells remain exact.
- [ ] Verify every `Onsite*` maps to an input `Pref`.
- [ ] Independently verify report components, objective, violations, overrides, and staff tallies.
- [ ] Verify metadata labels and result ordering.
- [ ] Verify deterministic repeated iteration-mode output.
- [ ] Verify runtime mode stops within the documented tolerance.
- [ ] Verify dispatcher help and table export interoperability.
- [ ] Verify zero/multiple table selection errors and strong exception safety on parse/render failure.

## K. Performance and robustness

- [ ] Build deterministic synthetic schedules for small through year-scale benchmarks.
- [ ] Measure parse, scoring, search, archive, render, and peak-memory costs separately.
- [ ] Profile before deciding whether incremental scoring is necessary.
- [ ] If needed, add lookup-based incremental scoring while retaining the full scorer as oracle.
- [x] Demonstrate the sample and default invocation complete below 30 seconds on a documented workstation. Release standalone harness: 250,000 proposals, three outputs, 15.93 seconds on the 2026-08-25 CI/container host.
- [ ] Demonstrate requested runtime plus reporting overhead remains within tolerance.
- [ ] Verify archive memory is bounded by configured size and assignment dimensions.
- [ ] Run ASan and UBSan where supported.
- [ ] Run multi-seed/multi-size soak tests with full-score consistency checks.
- [ ] Exhaustively enumerate small fixtures and measure optimizer optimality rate.
- [ ] Establish sample objective best/median/worst baselines over at least 30 fixed seeds.
- [ ] Add a quality regression threshold based on reviewed benchmark evidence.

## L. Operational validation and release

- [ ] Review generated sample schedules and reports with scheduling owners.
- [ ] Compare against historical manual schedules and investigate material differences.
- [ ] Perform one-weight-at-a-time sensitivity analysis.
- [ ] Confirm Pareto outputs represent useful operational trade-offs.
- [ ] Validate site-specific statuses and closure policies.
- [ ] Document that Pareto membership covers the final retained archive and is not a global-optimality proof.
- [ ] Document advisory use, human approval requirement, seed, and runtime reproducibility limits.
- [ ] Run advisory-only parallel scheduling cycles and record stakeholder feedback.
- [ ] Obtain engineering and operational sign-off before production use.
- [x] Confirm all added code compiles strictly as C++17 and no dependency was added beyond existing Ygor/DICOMautomaton facilities.
