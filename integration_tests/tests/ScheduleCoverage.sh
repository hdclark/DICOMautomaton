#!/usr/bin/env bash

set -eux
set -o pipefail

# Run the scheduler on the shipped template with the default argument set. The template carries a
# max_consecutive_remote=2 requirement, which MaxConsecutiveRemoteDays='table' consumes by default.

"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/20260821_DCMA_schedule_template.tsv \
  -o ScheduleCoverage:NVariations='3' \
  -o ExportTables:TableSelection='ScheduleVariation@^[0-9]+$':Filename='schedules.csv' \
  | tee -a fullstdout \
  | grep "Performing operation 'ScheduleCoverage' now" \
  | wc -l \
  | grep 1 \
  | `# Ensure the output stream is not empty. ` \
  grep .

# The default run emits exactly the requested number of variations, each with one report block and one OBJECTIVES row.
[ "$(grep -c '== Schedule Report ==' schedules.csv)" = "3" ]
[ "$(grep -c 'OBJECTIVES' schedules.csv)" = "3" ]
[ "$(grep -c 'consecutive_remote_penalty=' schedules.csv)" = "3" ]

# No undecided 'x' cell remains in any variation.
[ "$(grep -c '"x"' schedules.csv)" = "0" ]

# Every variation reports a TALLY row for each of the 11 staff members.
[ "$(grep -c 'TALLY' schedules.csv)" = "33" ]

# A runtime MaxConsecutiveRemoteDays value overrides the table requirement. Disable the run penalty and request two
# variations; both reports must show a zero consecutive-remote objective.
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/20260821_DCMA_schedule_template.tsv \
  -o ScheduleCoverage:NVariations='2':MaxConsecutiveRemoteDays='0' \
  -o ExportTables:TableSelection='ScheduleVariation@^[0-9]+$':Filename='schedules2.csv' \
  > /dev/null 2>&1

[ "$(grep -c '== Schedule Report ==' schedules2.csv)" = "2" ]
[ "$(grep -c 'OBJECTIVES' schedules2.csv)" = "2" ]
[ "$(grep -c 'consecutive_remote_penalty=0' schedules2.csv)" = "2" ]
