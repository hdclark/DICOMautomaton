# Schedule Optimizer Specification and Implementation Plan

## 1. Purpose and scope

Implement a DICOMautomaton Operation named `OptimizeSchedule` that consumes exactly one selected
`Sparse_Table`, validates and parses its constraints and schedule template, and produces one or more
new `Sparse_Table` schedule alternatives. The source table must not be modified.

The Operation must:

- preserve immutable input text exactly;
- replace each `x` with `Onsite` or `Remote*`;
- replace each `Pref` with `Remote`, or with `Onsite*` when the preference is overridden;
- evaluate all configured constraints case-insensitively;
- use simulated annealing without introducing a third-party dependency;
- support deterministic iteration-limited optimization and time-limited optimization;
- maintain a bounded Pareto archive and return diverse, high-quality schedules;
- append an auditable report to each output table;
- complete within the user limit, with a documented default below 30 seconds on a typical workstation;
- use only C++17 and existing DICOMautomaton/Ygor facilities.

This feature assigns onsite versus remote work only. It does not alter rota, leave, offsite, holiday,
or arbitrary site-specific statuses.

## 2. Terminology and policy decisions

The following definitions remove ambiguity from the implementation.

| Term | Definition |
|---|---|
| Staff | A unique, non-empty column label in a `Date` header row. |
| Schedule day | A parsed data row associated with a `Date` header. |
| Active day | A schedule day on which constraints are evaluated. See section 4.5. |
| Mutable cell | An input cell whose trimmed, case-folded value is `x` or `Pref`. |
| Immutable cell | Every other cell. Its exact original string is preserved in output. |
| Assignment | The binary `Onsite`/`Remote` decision corresponding to a mutable cell. |
| Status set | The statuses considered present or remote by one particular constraint row. |
| Component cost | A normalized, unweighted cost for one constraint row. |
| Objective | Sum of each component cost multiplied by that row's weight. |
| Direct violation | A violation attributable to a particular day or cells, suitable for the daily report. |
| Global penalty | A fairness penalty that cannot honestly be attributed to one day. |

`Onsite*` and `Remote*` are semantically identical to `Onsite` and `Remote` respectively during constraint
evaluation. If either occurs in input, it is immutable and preserved. `Remote` in input is also recognized
as remote for constraints. Recognizing output statuses makes an exported result safe to assess again
without making it mutable. Unrecognized statuses remain immutable and count as neither onsite nor remote
unless a constraint's explicit status set includes them.

All string keywords, constraint names, status names, and staff references are matched after trimming
ASCII whitespace and ASCII case-folding. Original text is retained for output and diagnostics.

## 3. Operation interface

Add `src/Operations/OptimizeSchedule.h` and `src/Operations/OptimizeSchedule.cc`, following the local
`OperationDoc`, `OperationArgPkg`, table selector, metadata, and exception conventions.

### 3.1 Arguments

| Argument | Default | Validation and behavior |
|---|---:|---|
| `TableSelection` | `last` | Use `STWhitelistOpArgDoc()`. Exactly one valid table must be selected. |
| `RandomSeed` | `0` | Unsigned 64-bit integer. A value of `0` is still a literal, reproducible seed. |
| `Iterations` | `250000` | Positive integer in iteration mode. One iteration is one proposed move. |
| `RuntimeSeconds` | `0` | Finite number in `[0, 30]`. If greater than zero, runtime mode is used and `Iterations` is ignored with an informational log message. |
| `OutputSchedules` | `3` | Integer in `[1, 20]`. Fewer unique schedules may be returned only if the decision space is smaller or runtime mode reaches its end-to-end deadline before enough alternatives are retained. |
| `ParetoArchiveSize` | `256` | Integer in `[OutputSchedules, 4096]`; bounds memory and archive maintenance work. |
| `TemperatureStart` | `auto` | `auto` calibrates from sampled uphill moves; otherwise a finite positive number. |
| `TemperatureEnd` | `0.001` | Finite positive fraction of the starting temperature, strictly less than 1. |
| `RestartCount` | `auto` | `auto` uses `min(OutputSchedules, 8)` chains. An explicit value must be in `[1, 64]`. Work is divided among chains. |
| `ExcludeUnanimousStatuses` | empty | Case-insensitive `|`-separated statuses. A row where every staff cell has one identical listed status is inactive. For example, set `Holiday` to exclude unanimous holiday rows. Empty disables this opt-in rule. |
| `TableLabel` | `Optimized Schedule` | Base label for emitted tables. Result number is appended. |

Iteration mode is reproducible for the same build, architecture, input, arguments, and seed. Runtime
mode is not promised to be bit-for-bit reproducible because the number of completed iterations depends
on machine load. Runtime mode must use `std::chrono::steady_clock`, check the deadline at least every
256 proposals, reserve time for final reporting, and not intentionally exceed the requested duration.

The default is iteration mode so tests and routine scripted use are deterministic. The documented
interactive recommendation is `RuntimeSeconds=20`, which leaves margin under the 30-second requirement.
The implementation and quality benchmark must keep the default iteration count below approximately 30
seconds on the documented representative workstation. An explicitly larger iteration count is a user
request for additional work and can exceed 30 seconds; use runtime mode when a hard elapsed-time limit is
required. Argument parsing uses the classic locale, consumes the whole token, permits decimal exponent
notation for floating-point values, and rejects signs on unsigned integers, overflow, and trailing text.

### 3.2 Operation integration

Register the Operation in:

- `src/Operations/CMakeLists.txt`;
- `src/Operation_Dispatcher.cc`;
- the corresponding Operation header include aggregation used by the dispatcher, if required by the
  current source layout.

Use `All_STs` and `Whitelist` as existing table Operations do. Create outputs with
`std::make_shared<Sparse_Table>()`, copy the input table, update only mutable schedule coordinates, append
the report, and add each result to `DICOM_data.table_data`. Set `TableLabel`, `NormalizedTableLabel`, and
`Description` using the same metadata helpers and `Explicator` conventions as other Operations. Also add
metadata keys `ScheduleOptimizerSeed`, `ScheduleOptimizerObjective`, and
`ScheduleOptimizerResultIndex`.

## 4. Input table contract

### 4.1 Row recognition

Parse from `Sparse_Table::table.data` or bounded `value()` access without assuming zero-based rows,
contiguous cells, fixed row numbers, or a fixed number of trailing columns.

Rows are classified by their first non-empty logical cell, after trimming and case-folding:

- `Constraint`: constraint row;
- `Date`: schedule header row;
- a valid date in the current header's first column: schedule data row;
- empty row: separator;
- anything else outside a schedule block: preserved non-schedule content.

A schedule block starts at a `Date` header and continues through data/separator rows until another
`Date` header, a `Constraint` row, or end of table. Within a schedule block, a non-empty row that is not a
valid date row is an error if any declared staff column is populated. This prevents a mistyped date from
silently preserving unresolved `x` or `Pref` cells. Arbitrary text rows outside schedule blocks remain
preserved.

Constraint rows may occur before or after schedule blocks, although putting them first remains the
recommended format. At least one constraint row is not required. At least one schedule block and one
mutable cell are required for optimization. A schedule with no mutable cells is rejected with a clear
message rather than emitting duplicate unchanged tables.

### 4.2 Constraint row grammar

Logical columns are:

| Column | Meaning |
|---:|---|
| 0 | Literal `Constraint`. |
| 1 | Constraint type, optionally with a name in parentheses. |
| 2 | Finite, non-negative decimal weight. |
| 3 | Constraint-specific expression, if required. |
| 4 and later | Optional `key=value` policy fields. Empty trailing fields are ignored. |

Weights may be zero. A zero-weight row is validated, evaluated, and reported, but does not participate
in annealing or Pareto dominance. Negative, NaN, and infinite weights are errors. Duplicate named or
unnamed constraints are allowed and evaluated independently; reports identify them by source row and
name. Reports label zero-weight rows `disabled/advisory`; their component is shown, but they do not emit
`DayViolation` records or count as unmet active constraints.

The common optional policy is `statuses=A|B|C`. It replaces the default status set for that individual
constraint row. `Onsite*` and `Remote*` are canonicalized to their unstarred forms when status sets are
parsed. Duplicate statuses are harmless. An empty status set is invalid. Unknown policy keys and policy
fields without `=` are errors so typographical mistakes do not silently alter clinical operations.
An explicit status policy can reclassify an otherwise unknown immutable status for that row only; without
such a policy, unknown values retain the required Vac-like behavior of counting as neither onsite nor
remote.

Supported type grammar is anchored and case-insensitive:

```text
minimum_onsite
maximum_onsite
group (<non-empty name>)
max_consecutive_remote
exclusivity (<non-empty name>)
max_weekly_remote
fairness_remote
fairness_overrides
```

Supported expressions are:

```text
any <positive integer> of all
any <positive integer> of <staff> (or <staff>)+
any 1 of <staff> (xor <staff>)+
<non-negative integer>
<staff> = <non-negative integer> (, <staff> = <non-negative integer>)*
```

The standalone integer applies only to `max_consecutive_remote`. The assignment list applies only to
`max_weekly_remote`; the any-of and xor forms apply only to their corresponding coverage/exclusivity
types. The any-of integer may be zero only for `maximum_onsite`; it must be positive for minimum and group
coverage. Integers use ASCII digits only with full-token consumption and overflow checking. Weights use
a locale-independent decimal grammar with optional exponent and full-token consumption.

Whitespace around tokens is flexible. Staff identifiers are looked up case-insensitively but must be
unambiguous. Referencing an unknown staff identifier, repeating one in a list, requesting more staff
than the candidate set contains, supplying an expression to a no-expression constraint, or omitting a
required expression is an error. The exclusivity grammar deliberately requires `any 1`; accepting any
other number would disguise a malformed policy.

For the initial implementation, staff identifiers must match `[A-Za-z0-9_.-]+`. Validate header labels
against this grammar. This avoids ambiguity with the reserved expression delimiters `or`, `xor`, comma,
and equals. Quoted identifiers can be added later only with a fully specified escaping grammar.

Default status sets are:

| Constraint | Default `statuses` |
|---|---|
| `minimum_onsite` | `Onsite` |
| `maximum_onsite` | `Onsite` |
| `group (...)` | `Onsite` |
| `exclusivity (...)` | `Onsite|Prim|Sec` |
| `max_consecutive_remote` | `Remote` |
| `max_weekly_remote` | `Remote` |
| `fairness_remote` | `Remote` |
| `fairness_overrides` | Not configurable; it specifically detects `Pref` to `Onsite*`. |

Thus the sample's minimum and group constraints exclude `Prim` and `Sec`, while exclusivity includes
them. A user can override this per row, for example:

```text
Constraint  minimum_onsite       1000  any 2 of all                 statuses=Onsite|Prim
Constraint  group (srs)           200  any 1 of XC or XD or XE      statuses=Onsite|Prim|Sec
Constraint  exclusivity (office)  1.5  any 1 of XA xor XB           statuses=Onsite
```

### 4.3 Schedule headers and blocks

A header has `Date` in its first populated column and one or more staff labels in subsequent populated
columns. Staff labels must be non-empty and unique under case-insensitive comparison. The first header
defines the staff set and column coordinates. Every later header must repeat that exact set in the same
columns; differences are rejected rather than risking assignments to the wrong person.

Blank trailing columns are ignored. Interior blank staff columns are allowed and preserved, but cannot
be referenced. Schedule data rows use the column positions from the most recent header.

### 4.4 Dates

Implement a small locale-independent Gregorian parser rather than relying on platform locale. It must
accept the sample forms, including an optional weekday followed by a comma, full or abbreviated English
month names, `Sept`, and ordinal-free day/year values. At minimum these forms are accepted:

```text
Mon, Aug 31, 2026
Tues, Sept 1, 2026
2026-09-01
```

Validate leap years and month lengths. If a weekday is supplied, accept common variants (`Tue`/`Tues`,
`Thu`/`Thur`/`Thurs`) and reject a mismatch with the computed weekday. Duplicate dates are errors.
Input row order must be strictly increasing by date; do not silently sort because the output must retain
the source layout. Convert internally to an integer civil-day key. Use Monday-to-Sunday ISO calendar
weeks for weekly constraints; the implementation only needs a correct civil-date/ISO-week calculation,
not timezone handling.

### 4.5 Active and inactive days

By default, every parsed schedule day is active, including the sample's `Holiday` rows. This follows the
rule that unknown statuses are immutable and Vac-like; coverage constraints can therefore report an
unmet requirement on such a day. When `ExcludeUnanimousStatuses` is explicitly configured, a day is
inactive only if every declared staff cell is populated and all canonical values are the same listed
status. A missing staff cell is always an input error, never grounds for excluding a row.

This narrowly scoped opt-in rule does not hide an understaffed all-vacation day. A user who has confirmed
that unanimous holiday rows are true closures can set `ExcludeUnanimousStatuses=Holiday`. The report
lists every excluded date and reason. If explicit exclusions leave zero active days, reject the input;
there is no meaningful optimization objective or bias tally.

## 5. Internal model

Keep parsing, scoring, search, and rendering separate even if implementation remains in one `.cc` file.
Suggested internal types in an unnamed namespace are:

- `CivilDate`, including day key and ISO week key;
- `StaffMember`, including original label and source column;
- `ScheduleDay`, including date, source row, active flag, and cells;
- `Cell`, including original text, canonical status, mutability, preference flag, and variable index;
- a `std::variant` of typed constraint records;
- `Problem`, the immutable parsed representation;
- `Candidate`, a compact assignment vector plus cached score;
- `Score`, containing per-row normalized components, weighted total, direct violations, fairness details,
  and per-staff tallies;
- `ArchiveEntry`, containing assignment, component vector, weighted score, and insertion sequence.

Use `std::vector<uint8_t>` or an equivalent compact container for binary decisions. Do not store output
strings in the search state. Build lookup tables from each variable to affected day, staff, week, and
constraint records. Keep all arithmetic in `double`; reject non-finite intermediate values defensively.

## 6. Constraint semantics and scoring

All component costs are normalized independently of schedule length where practical. This keeps a
weight meaningful when optimizing five days or several months. The objective is:

```text
objective(candidate) = sum(weight[c] * component_cost[c])
```

Each parsed constraint row contributes one component. The report must include both its component and
weighted contribution. Status membership always uses the row's status set and the candidate's rendered
semantic status (`Pref` and `x` decisions both become semantic `Remote` or `Onsite`).

### 6.1 Minimum onsite and group

For each active day, count candidate staff whose status is in the constraint status set. For `all`, the
candidate set is all staff; otherwise it is the explicitly listed set. Given requirement `k` and count
`n`, daily deficit is:

```text
deficit = max(0, k - n) / k
```

The component cost is the mean daily deficit across active days. Every day with non-zero deficit is a
direct violation. Its report includes the observed count, required count, status set, and missing staff
count. `minimum_onsite` and each `group` row are scored independently, including overlapping groups.

### 6.2 Maximum onsite

For each active day, count candidate staff whose status is in the constraint status set. Given maximum
`k`, candidate count `m`, and observed count `n`, daily excess is:

```text
excess = max(0, n - k) / max(1, m - k)
```

The component cost is the mean daily excess across active days. A maximum of zero is valid. Every day
with non-zero excess is a direct violation reporting the observed count and configured maximum.

### 6.3 Exclusivity

For each active day, count listed staff whose status is in the status set. The normalized excess is:

```text
excess = max(0, count - 1) / max(1, listed_staff_count - 1)
```

The component is mean excess over active days. A non-zero excess is a direct violation listing all
simultaneously present staff. The `xor` syntax means "at most one present," not exactly one; zero or one
is valid. In particular, two absent staff must never be penalized.

### 6.4 Maximum consecutive remote

For each staff member, inspect active schedule rows in chronological order. An inactive day or any active
cell not in the row's remote status set terminates a run. Missing weekend dates do not themselves break a
run, so Friday and the following Monday are consecutive schedule workdays. For limit `L`, a run of length
`r` contributes `max(0, r-L)` excess days.

The component is total excess days divided by total active staff-days. This denominator depends only on
the input problem, so adding an unrelated remote assignment cannot reduce an existing penalty. A zero
denominator is impossible after active-day validation. The `(L+1)`th and subsequent days in each run are
direct violations and identify the staff member and current run length. `L=0` is valid and penalizes
every remote-status day.

### 6.5 Maximum weekly remote

The expression maps one or more staff members to individual limits. Group active days by ISO year/week.
For each configured staff/week pair, let `r` be days in the remote status set and `L` the configured
limit. Sum `max(0, r-L)` and divide by the number of active staff-days for those configured staff across
the represented weeks. A zero denominator gives zero cost.

Every remote-status day after the first `L` such days in chronological order that week is marked as a
direct violation. The report also includes one weekly summary with the actual and allowed totals. This
attribution is deterministic and does not imply that the last day is uniquely responsible.

### 6.6 Remote fairness

Fairness is based only on opportunities controlled by this optimizer. For staff `i`:

```text
eligible_i = active cells initially equal to x or Pref
remote_i   = eligible cells assigned Remote and included by the constraint's statuses
ratio_i    = remote_i / eligible_i
```

Staff with no eligible cells are excluded. Let `m` be the mean ratio over included staff. The component
is mean absolute deviation from `m`:

```text
component = sum(abs(ratio_i - m)) / included_staff_count
```

This is in `[0, 1]`, measures opportunity-adjusted disparity, and is global rather than proportional to
the number of days. It therefore remains stable as schedule length changes. It does not flag individual
days. The report shows each numerator, denominator, ratio, mean, and component.

`remote_i` is technically the number of eligible decisions whose resulting status belongs to the
configured set. The default `Remote` gives the intended remote-fairness meaning. A custom set deliberately
changes which decision outcome is equalized and must be described as such in `OperationDoc`; immutable
statuses can never occur in eligible mutable cells. This configurability supports the requirement that
the end user can choose statuses counted by an individual constraint.

### 6.7 Preference override fairness and cost

For staff `i` with at least one input `Pref` on an active day:

```text
pref_i     = number of active Pref cells
override_i = Pref cells assigned Onsite
ratio_i    = override_i / pref_i
```

Let `m` be the mean ratio and `d` the mean absolute deviation from `m`. Define:

```text
component = 0.5 * m + 0.5 * d
```

This simultaneously ensures that every override contributes positive cost and discourages concentrating
necessary overrides on a subset of staff. It is normalized, in `[0, 1]`, and global. Complete override
for every staff has component 0.5 by design: half of this row's weight prices override frequency and half
prices disparity. If there are no active `Pref` cells, the cost is zero. Each override is listed in the
report as an indication, not a direct unmet constraint; the global report includes per-staff rates and
the component decomposition.

This definition is intentional: using dispersion alone could make overriding every preference appear
perfectly fair and cost-free.

Because every constraint is optional, override cost is active only when `fairness_overrides` is present
with positive weight. Omitting it or assigning zero weight explicitly opts out of preference-override
cost, though overrides remain visibly rendered and reported. `OperationDoc` must make this consequence
prominent; there is no hidden, unweighted objective.

### 6.8 No constraints and impossible constraints

With no positive-weight rows, all assignments have objective zero. The initializer should honor `Pref`
as `Remote` and choose `Remote` for `x`; additional requested outputs may vary any mutable cells,
including `Pref`, deterministically from seeded random candidates. Reports still provide tallies and
make explicit that override cost was disabled.

Constraints remain soft except for input validity and cell immutability. An impossible coverage policy
must produce the least-cost schedule and explicit violations, not fail optimization. Before search,
perform a feasibility bound for each daily minimum/group row by choosing, independently for each mutable
cell, whichever of its two outcomes maximizes membership in that row's status set. Log and report dates
whose requirements are provably impossible. Do not mistake this bound for a full cross-constraint
feasibility proof.

## 7. Simulated annealing

### 7.1 Randomness

Use a fully specified local pseudo-random generator implemented with the standard library or a small
in-tree C++17 routine. Prefer `std::mt19937_64`, seeded from `RandomSeed`. Derive each chain seed from the
base seed and chain index with a documented fixed mixing function. Do not use `std::random_device`, global
state, wall-clock time, or implementation-dependent `std::hash`.

Generate uniform doubles from raw generator bits with a documented conversion if cross-standard-library
reproducibility is required by tests. Record the base seed in metadata and report.

### 7.2 Initial candidates

Always create a preference-respecting baseline: all `Pref` and `x` cells assigned `Remote`. Improve a
copy with a greedy coverage pass, processing highest weighted minimum/group deficits first and selecting
the decision toggle that reduces the deficit with the smallest exact objective increase. Stop when no
deficit can be reduced. This remains correct for explicitly customized status sets.

Other chains start from seeded perturbations of this baseline and greedy candidate. This gives immediate
reasonable schedules while retaining diversity. Score and archive all initial candidates.

### 7.3 Moves

At each proposal choose among:

- single flip: toggle one mutable cell;
- same-day swap: toggle one onsite and one remote mutable cell on a day, preserving daily count;
- same-staff swap: toggle two opposite assignments for one staff member, helping weekly and fairness
  trade-offs;
- targeted repair: choose a current direct violation and flip an eligible variable that can reduce it.

Use fixed documented probabilities, with unavailable move types falling back to a single flip. Every
variable must remain reachable by single flips, preserving ergodicity. A proposal may touch `Pref`; its
override penalty governs acceptance. Never modify immutable cells.

### 7.4 Acceptance and cooling

For objective change `delta`:

```text
accept when delta <= 0
otherwise accept with probability exp(-delta / temperature)
```

Clamp the exponent to avoid overflow/underflow. Use geometric cooling from `T_start` to
`T_start * TemperatureEnd` over each chain's budget. In runtime mode estimate progress as elapsed search
time divided by budget. In iteration mode use completed proposals divided by allocated proposals.

For `TemperatureStart=auto`, sample a bounded number of valid random uphill proposals from initial
candidates without advancing the actual chains. Choose a temperature that would accept the median
positive delta with probability 0.8. If no positive delta exists, use `max(1e-9, abs(objective)*1e-3)`.
Document and unit-test the exact calibration.

Track the best weighted candidate independently from the current state. Divide the work approximately
equally among chains, assigning remainder iterations deterministically. Chains may run sequentially for
strict deterministic behavior; parallel search should only be introduced after profiling and must not
compromise deadline handling or shared archive correctness.

### 7.5 Efficient exact scoring

First implement one clear full scorer as the correctness oracle. Then add incremental scoring if profiling
shows it is needed. A variable flip only affects its day, staff run, staff/week, and global fairness
aggregates. Same-day and same-staff moves can be evaluated as a small transaction and rolled back on
rejection.

In debug builds and targeted tests, periodically compare cached incremental scores to a full recomputation
within a tight floating-point tolerance. Never allow a speed optimization to become a second definition
of a constraint.

## 8. Pareto archive and output selection

The Pareto vector contains one normalized component for each positive-weight constraint row, in source
row order. Candidate `A` dominates `B` when every component of `A` is no greater and at least one is
strictly smaller, using a fixed comparison tolerance of `1e-12`. Identical assignment vectors are
deduplicated before Pareto comparison.

Insert every accepted candidate, each chain best, and all initial candidates into a bounded archive.
When an insertion is non-dominated, remove entries it dominates. If the archive exceeds
`ParetoArchiveSize`, preserve the globally lowest weighted objective and prune one entry from the most
crowded region. For each component, normalize values to `[0,1]` over the current archive; a zero-span
component contributes zero distance. Use standard adjacent-neighbor crowding distance after sorting each
component, assign boundary entries infinite distance, sum dimensions, and break pruning ties by removing
the newest entry first.

Final selection proceeds as follows:

1. Include the globally lowest weighted-objective candidate.
2. Add archive entries by max-min Euclidean distance in the same normalized component space: choose the
   candidate whose distance to its nearest selected vector is greatest, with lower weighted objective and
   older insertion sequence as tie-breakers.
3. If the Pareto archive cannot supply enough unique assignments, add lowest-objective unique dominated
   candidates retained in a separate bounded elite pool.
4. If still short, generate at most `10 * OutputSchedules` seeded assignment vectors by bounded random
   mutation, stopping at the end-to-end deadline, and choose the lowest-objective unique candidates. Do
   not exhaustively enumerate. Score these through the same archive/elite insertion path before final
   Pareto labels are assigned. Never emit duplicate schedules merely to reach the requested count.

Sort emitted results by weighted objective, then lexicographic assignment vector. A report field states
`Pareto=yes/no` and explains when a dominated fallback was used. The first result is always the scalar
best found. `Pareto=yes` means non-dominated relative to the final retained archive, not every scored
proposal and not the true global front; the user-facing documentation must say so.

Runtime mode's deadline is end-to-end from immediately before initialization through selection and
rendering. Reserve the larger of 250 ms or 5% of the requested duration for final selection/reporting.
If that reserve proves insufficient, stop generating fallback alternatives and return the unique results
already available rather than exceed the deadline intentionally.

## 9. Output table and report

### 9.1 Schedule rendering

Start from a complete copy of the input `Sparse_Table`, including metadata. For each mutable coordinate:

| Input | Decision | Output |
|---|---|---|
| `x` | onsite | `Onsite` |
| `x` | remote | `Remote*` |
| `Pref` | onsite | `Onsite*` |
| `Pref` | remote | `Remote` |

The mapping is based on canonical input, but generated spelling is exactly as shown. All immutable cells,
unknown rows, headers, constraints, blank structure, and original date strings remain unchanged. Reports
are appended below `next_empty_row()` after at least one blank row. If old report rows identified by the
exact marker `Schedule Optimizer Report` are present, reject the input and instruct the caller to select
the original template; do not append reports recursively.

### 9.2 Report schema

Use tabular rows so reports remain machine-readable after CSV/TSV export. Every report row begins with
`Schedule Optimizer Report` in column 0. Column 1 is a record type. Remaining columns are record-specific:

```text
Schedule Optimizer Report  Summary      key  value
Schedule Optimizer Report  Component    source-row  constraint  weight  normalized-cost  weighted-cost
Schedule Optimizer Report  DayViolation date  source-row  constraint  observed  required  description
Schedule Optimizer Report  Override     date  staff  Pref  Onsite*  description
Schedule Optimizer Report  StaffTally   staff  assigned-onsite  assigned-remote  pref-count  overrides  eligible-remote  remote-ratio  total-onsite-status  total-remote-status
Schedule Optimizer Report  Weekly       iso-week  staff  constraint  remote-count  limit  excess
Schedule Optimizer Report  ExcludedDay  date  status  description
Schedule Optimizer Report  Feasibility  date  source-row  constraint  maximum-possible  required  description
```

Summary records must include result index/count, seed, search mode and actual iterations/time, weighted
objective, Pareto status, archive size, mutable cell count, active day count, and count of direct
violations and overrides. Human-readable descriptions must include original constraint names and staff
labels, not only internal indexes.

`assigned-onsite` and `assigned-remote` tally optimizer-controlled cells on active days only, which
directly exposes assignment bias. `total-onsite-status` counts active-day cells canonically equal to
`Onsite` (including `Onsite*`) and `total-remote-status` counts active-day cells canonically equal to
`Remote`; these fixed semantic totals do not include `Prim`/`Sec` or vary with constraint policies. Keep
the controlled and total concepts separate and define both in `OperationDoc`.

Order report records deterministically: summaries, components in source order, day violations by date and
source row, overrides by date/staff order, weekly records by week/staff, staff tallies in header order,
excluded days, then feasibility warnings. Numeric output should use sufficient round-trip precision and
the classic locale.

Also emit concise `YLOGINFO` progress/final summaries and `YLOGWARN` feasibility warnings. Do not log on
every iteration or violation.

## 10. Error handling and safety

Throw `std::invalid_argument` before changing `Drover` for all input and argument errors. Diagnostics must
include source row/column, offending text when safe, and expected grammar. Parse and optimize entirely
into temporary objects; append output tables only after every selected result has rendered successfully,
providing operation-level strong exception safety.

Reject at least:

- zero or multiple selected tables;
- null selected table;
- malformed or non-finite arguments and weights;
- unknown constraints or policy keys;
- malformed expressions or unknown/ambiguous staff;
- inconsistent headers, duplicate staff, dates, or constraint references;
- invalid, duplicate, or non-monotonic dates;
- schedule-like rows before a header and malformed non-empty rows inside a schedule block;
- empty staff cells on a schedule data row;
- a data row with populated cells beyond known staff columns, except empty trailing cells;
- no schedule days or no mutable cells;
- arithmetic overflow or non-finite score;
- a pre-existing optimizer report marker.

Do not reject infeasible soft constraints. Do not silently reinterpret malformed input. Do not write files,
invoke external programs, or use network services.

## 11. Proposed source organization

Prefer the smallest maintainable arrangement:

```text
src/Operations/OptimizeSchedule.h
src/Operations/OptimizeSchedule.cc
```

Keep private parser/model/scorer/search helpers in the `.cc` unnamed namespace initially. If the file
becomes unwieldy or tests require direct access, extract only cohesive implementation details to:

```text
src/Operations/OptimizeSchedule_Utils.h
src/Operations/OptimizeSchedule_Utils.cc
```

Do not expose optimizer internals as public DICOMautomaton API without a demonstrated reuse case. Reuse
`Sparse_Table`, `tables::table2`, `Whitelist`, metadata helpers, `Explicator`, Ygor logging, and existing
string utilities where their behavior matches this specification. Implement specialized constraint/date
parsing locally when generic utilities are locale-sensitive or too permissive.

## 12. Verification and validation strategy

### 12.1 Unit tests

Add deterministic C++ tests for pure parsing and scoring behavior. If the current unit test harness is
minimal, create a focused executable linked against the same object libraries rather than adding a test
framework dependency.

Required parser cases include:

- the provided TSV through `table2::read_csv`;
- sparse/non-zero coordinate bounds and blank separators;
- repeated matching and mismatching headers;
- all accepted date variants, leap day, bad weekday, duplicate/non-monotonic dates;
- every valid constraint grammar and policy override;
- case and whitespace variation;
- malformed numbers, NaN/infinity, unknown staff/constraint/policy, duplicate list members;
- unknown immutable statuses and exact preservation;
- unanimous holiday exclusion and disabled exclusion;
- input `Onsite*` and `Remote` round-trip semantics.

Required scorer cases use tiny hand-computable schedules and assert exact or tightly bounded values for:

- minimum/group deficits and overlapping groups;
- exclusivity with zero, one, and multiple present staff, including immutable `Prim`/`Sec`;
- consecutive runs across weekends, breaks on leave/holiday, and limit zero;
- ISO week transitions and different per-staff weekly limits;
- remote fairness with unequal eligibility and no eligible staff;
- override mean and dispersion, including the all-overridden case;
- zero active days for a component;
- weighted total equals the sum of reported contributions;
- direct violation attribution and tally values.

Required search tests include:

- immutable cells never change;
- all mutable cells are resolved;
- fixed seed plus iterations gives identical assignments, scores, and report text;
- every returned score matches a fresh full evaluation;
- archive entries are mutually non-dominated and unique;
- best scalar candidate is output first;
- impossible constraints produce warnings/results rather than exceptions;
- incremental scoring, if implemented, matches full scoring over thousands of seeded random moves;
- one-variable and no-positive-weight edge cases terminate cleanly.

### 12.2 Integration tests

Add `integration_tests/tests/OptimizeSchedule.sh` in local shell-test style. Load
`artifacts/test_files/20260821_DCMA_schedule_template.tsv`, run the Operation with a fixed seed and modest
iteration count, export outputs, and verify using existing Operations where possible plus a small
dependency-free checker where necessary.

The integration test must verify:

- requested number of unique tables is added;
- source table is unchanged;
- output metadata labels are distinct;
- no `x` or `Pref` remains in schedule cells;
- immutable statuses and date/header positions match input exactly;
- every `Onsite*` corresponds to an input `Pref`;
- reports contain summary, components, violations (if any), overrides, and every staff tally;
- reported component and objective values independently recompute correctly;
- exclusivity never penalizes two absent staff;
- repeated invocation with seed/iterations is byte-identical after excluding metadata fields that existing
  infrastructure necessarily timestamps.

Add malformed fixture tables for focused negative integration cases rather than mutating the main sample.

### 12.3 Performance and soak validation

Create a deterministic synthetic generator in test code for schedules of approximately 25, 100, and 250
staff over 5, 30, 90, and 365 active days with realistic constraint density. Record parse time, proposals
per second, archive time, peak memory, objective improvement, and full-versus-incremental score agreement.

Acceptance targets on a documented representative workstation are:

- default sample completes comfortably below 30 seconds;
- `RuntimeSeconds=N` returns near N seconds and never intentionally exceeds N plus 250 ms reporting
  overhead;
- memory remains bounded by problem size plus `ParetoArchiveSize * assignment_size`;
- a fixed quality benchmark reaches a pre-recorded objective threshold for several seeds;
- sanitizer runs (ASan/UBSan where supported) show no failures;
- a multi-seed soak run shows no malformed output or score mismatch.

Runtime is not the only quality metric. Maintain benchmark fixtures with known global optima for small
problems (verified by exhaustive enumeration) and compare annealing against those optima. For the sample,
record best/median/worst objective over at least 30 fixed seeds and set a regression threshold only after
reviewing the resulting schedules with operational stakeholders.

### 12.4 Clinical/operational validation

Before treating the Operation as production-ready:

- have schedule owners review the exact semantics and default status sets;
- compare outputs against manually created historical schedules without using the manual schedule as a
  presumed optimum;
- review all violations and preference overrides for explainability;
- test site-specific statuses and closure-day settings;
- confirm weights through sensitivity analysis, changing one weight at a time;
- confirm Pareto alternatives expose meaningful trade-offs rather than cosmetic differences;
- run in advisory mode for several scheduling cycles and require human approval of every schedule.

The Operation is decision support. It must not claim that a returned result proves staffing safety or
global optimality.

## 13. Delivery sequence

1. Freeze this input grammar and scoring contract with stakeholders.
2. Implement model, parser, validation, and exact full scorer with unit tests.
3. Implement report rendering and a deterministic evaluator-only path used by tests.
4. Implement seeded annealing, greedy initialization, moves, and cooling.
5. Implement Pareto archive, elite fallback, and multiple output rendering.
6. Profile; add incremental scoring only where measured.
7. Register/document the Operation and add sample-based integration tests.
8. Run correctness, sanitizer, performance, quality-regression, and operational validation.
9. Release initially as an advisory optimizer with explicit limitations and reproducibility information.

## 14. Definition of done

The work is complete when all items in `tracker.md` are closed, all documented tests pass, C++17 builds
succeed on supported toolchains, no new dependency is present, the provided sample produces auditable and
reproducible results, the runtime target is demonstrated, and operational stakeholders sign off on the
constraint semantics and report readability.
