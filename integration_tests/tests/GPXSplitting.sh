#!/usr/bin/env bash

set -eux
set -o pipefail

# Move to a temporary directory.
cd "$(mktemp -d)"

# Load the GPX file and verify it creates multiple contours.
"${DCMA_BIN}" \
    "${TEST_FILES_ROOT}/gpx_multi_activity.gpx" \
    -o DroverDebug \
| tee output.txt

# Check that the output contains information about multiple contours.
# The original contour should be present, plus additional segmented contours.
grep -i "contour" output.txt || true
grep -i "ActivitySegment" output.txt || true

# Verify that we have multiple contours loaded.
# The original implementation would create 1 contour, 
# the new implementation should create the original + split segments (at least 4 total).
contour_count=$(grep -c "contour_of_points" output.txt || echo "0")
if [ "$contour_count" -lt 4 ]; then
    printf "Error: Expected at least 4 contours (1 original + 3 segments), got %s\n" "$contour_count"
    exit 1
fi

printf "Test passed: GPX file loaded with contours (count: %s)\n" "$contour_count"
exit 0
