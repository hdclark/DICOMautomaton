#!/usr/bin/env bash

set -eux
set -o pipefail

source_before="schedule-source-before.csv"
source_after="schedule-source-after.csv"
outputs="schedule-outputs.csv"

"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/20260821_DCMA_schedule_template.tsv \
  -o ExportTables \
    -p TableSelection='#0' \
    -p Filename="${source_before}" \
  -o OptimizeSchedule \
    -p TableSelection='#0' \
    -p RandomSeed=20260821 \
    -p Iterations=300 \
    -p RuntimeSeconds=0 \
    -p OutputSchedules=3 \
    -p ParetoArchiveSize=32 \
    -p RestartCount=2 \
  -o TestConditions \
    -p Conditions='table_count(4)' \
  -o ExportTables \
    -p TableSelection='#0' \
    -p Filename="${source_after}" \
  -o ExportTables \
    -p TableSelection='#1;#2;#3' \
    -p Filename="${outputs}"

cmp "${source_before}" "${source_after}"
test "$(grep -c '"Schedule Optimizer Report","Summary","result"' "${outputs}")" -eq 3
test "$(grep -c '"Schedule Optimizer Report","Component"' "${outputs}")" -eq 48
test "$(grep -c '"Schedule Optimizer Report","Component",[^,]*,"align_with_preferences"' "${outputs}")" -eq 3
! grep -E '^"(Mon|Tues|Wed|Thurs|Fri)[^"]*".*,"(x|Pref)"(,|$)' "${outputs}"
