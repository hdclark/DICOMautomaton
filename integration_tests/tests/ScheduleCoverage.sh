#!/usr/bin/env bash

set -eux
set -o pipefail

# The shipped template now declares Hard Constraint and weighted Soft Constraint rows inline.
# Keep the integration test reasonably quick while exercising the user-controlled iteration parameter.
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/20260821_DCMA_schedule_template.tsv \
  -o ScheduleCoverage:NVariations='3':AnnealingIterations='5000' \
  -o ExportTables:TableSelection='ScheduleVariation@^[0-9]+$':Filename='schedules.csv' \
  | tee -a fullstdout \
  | grep "Performing operation 'ScheduleCoverage' now" \
  | wc -l \
  | grep 1 \
  | grep .

[ "$(grep -c '== Schedule Report ==' schedules.csv)" = "3" ]
[ "$(grep -c 'OBJECTIVES' schedules.csv)" = "3" ]
[ "$(grep -c 'annealing_cost=' schedules.csv)" = "3" ]
[ "$(grep -c 'pareto_nondominated=' schedules.csv)" = "3" ]

# Six soft constraints are declared in the template; every variation reports each one's raw penalty and inline weight.
[ "$(grep -c 'SOFT_OBJECTIVE' schedules.csv)" = "18" ]

# No undecided x cell remains, and every variation has a TALLY row for all eleven staff.
[ "$(grep -c '"x"' schedules.csv)" = "0" ]
[ "$(grep -c 'TALLY' schedules.csv)" = "33" ]

# Zero iterations is explicitly supported as a deterministic baseline-only mode.
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/20260821_DCMA_schedule_template.tsv \
  -o ScheduleCoverage:NVariations='1':AnnealingIterations='0':RequirementViolationWeight='2500' \
  -o ExportTables:TableSelection='ScheduleVariation@^[0-9]+$':Filename='schedules2.csv' \
  > /dev/null 2>&1

[ "$(grep -c '== Schedule Report ==' schedules2.csv)" = "1" ]
[ "$(grep -c 'OBJECTIVES' schedules2.csv)" = "1" ]
[ "$(grep -c 'annealing_cost=' schedules2.csv)" = "1" ]
