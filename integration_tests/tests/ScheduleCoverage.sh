#!/usr/bin/env bash

set -eux
set -o pipefail

# 1) NVariations is an output contract even when annealing is disabled. The shipped
# template has many distinct hard-optimal schedules, so four must be emitted.
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/20260821_DCMA_schedule_template.tsv \
  -o ScheduleCoverage:NVariations='4':AnnealingIterations='0' \
  -o ExportTables:TableSelection='ScheduleVariation@^[0-9]+$':Filename='schedules_zero.csv' \
  > schedule_zero.stdout 2>&1

[ "$(grep -c '== Schedule Report ==' schedules_zero.csv)" = "4" ]
[ "$(grep -c 'OBJECTIVES' schedules_zero.csv)" = "4" ]
[ "$(grep -c 'SEARCH' schedules_zero.csv)" = "4" ]
[ "$(grep -c 'hard_optimal=true' schedules_zero.csv)" = "4" ]
[ "$(grep -c 'hard_violations=0,0,0' schedules_zero.csv)" = "4" ]
[ "$(grep -c 'requested_variations=4' schedules_zero.csv)" = "4" ]
[ "$(grep -c 'returned_variations=4' schedules_zero.csv)" = "4" ]
[ "$(grep -c 'annealing_proposals=0' schedules_zero.csv)" = "4" ]
[ "$(grep -c 'SOFT_OBJECTIVE' schedules_zero.csv)" = "24" ]
[ "$(grep -c 'TALLY' schedules_zero.csv)" = "44" ]

# Every mutable input cell must be resolved to a final state. In particular, Remote is
# an input preference marker; a final honoured preference is rendered as fixed "remote".
[ "$(grep -c '"x"' schedules_zero.csv)" = "0" ]
[ "$(grep -c '"Remote"' schedules_zero.csv)" = "0" ]

# 2) AnnealingIterations must control actual executed search work. NVariations=2 causes
# five bounded search runs, so 25 proposals/run gives exactly 125 proposals.
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/20260821_DCMA_schedule_template.tsv \
  -o ScheduleCoverage:NVariations='2':AnnealingIterations='25':Seed='17' \
  -o ExportTables:TableSelection='ScheduleVariation@^[0-9]+$':Filename='schedules_annealed.csv' \
  > schedule_annealed.stdout 2>&1

[ "$(grep -c '== Schedule Report ==' schedules_annealed.csv)" = "2" ]
[ "$(grep -c 'hard_optimal=true' schedules_annealed.csv)" = "2" ]
[ "$(grep -c 'hard_violations=0,0,0' schedules_annealed.csv)" = "2" ]
[ "$(grep -c 'iterations_per_run=25' schedules_annealed.csv)" = "2" ]
[ "$(grep -c 'annealing_runs=5' schedules_annealed.csv)" = "2" ]
[ "$(grep -c 'annealing_proposals=125' schedules_annealed.csv)" = "2" ]

# 3) A stale RequirementRegex must not silently discard the new Hard/Soft Constraint
# rows. This was the failure mode that could generate an unconstrained all-remote table.
if "${DCMA_BIN}" \
     "${TEST_FILES_ROOT}"/20260821_DCMA_schedule_template.tsv \
     -o ScheduleCoverage:RequirementRegex='^Requirement':AnnealingIterations='0' \
     > stale_regex.stdout 2>&1
then
    echo "ScheduleCoverage unexpectedly accepted a regex that excludes declared constraints" >&2
    exit 1
fi
grep -q 'excluded by RequirementRegex' stale_regex.stdout

# 4) If a hard constraint is genuinely infeasible, the returned schedule must still be
# the lexicographic hard optimum and must explicitly report the deficit.
cat > infeasible_schedule.csv <<'EOF'
"Hard Constraint 1","onsite","any 2"
"Date","XA"
"Mon","remote"
EOF

"${DCMA_BIN}" \
  infeasible_schedule.csv \
  -o ScheduleCoverage:NVariations='1':AnnealingIterations='0' \
  -o ExportTables:TableSelection='ScheduleVariation@^[0-9]+$':Filename='infeasible_out.csv' \
  > infeasible.stdout 2>&1

[ "$(grep -c '== Schedule Report ==' infeasible_out.csv)" = "1" ]
[ "$(grep -c '"FLAG"' infeasible_out.csv)" = "1" ]
[ "$(grep -c 'hard_optimal=true' infeasible_out.csv)" = "1" ]
[ "$(grep -c 'hard_violations=2' infeasible_out.csv)" = "1" ]
grep -q 'onsite_count=0, required>=2, deficit=2' infeasible_out.csv
