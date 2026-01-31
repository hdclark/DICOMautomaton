#!/usr/bin/env bash

set -eux
set -o pipefail


# Create a test GPX file with a time gap that should trigger activity splitting.
# The time gap of 105 seconds (between 08:00:15 and 08:02:00) exceeds the 60-second threshold.
cat > test_gpx_with_activity_split.gpx << 'GPXEOF'
<?xml version="1.0"?>
<gpx>
  <metadata>
    <name>Test Activity</name>
  </metadata>
  <trk>
    <name>Morning Run</name>
    <trkseg>
      <trkpt lat="49.2827" lon="-123.1207">
        <ele>10</ele>
        <time>2024-01-15T08:00:00Z</time>
      </trkpt>
      <trkpt lat="49.2828" lon="-123.1206">
        <ele>11</ele>
        <time>2024-01-15T08:00:05Z</time>
      </trkpt>
      <trkpt lat="49.2829" lon="-123.1205">
        <ele>12</ele>
        <time>2024-01-15T08:00:10Z</time>
      </trkpt>
      <trkpt lat="49.2830" lon="-123.1204">
        <ele>13</ele>
        <time>2024-01-15T08:00:15Z</time>
      </trkpt>
      <trkpt lat="49.2831" lon="-123.1203">
        <ele>13</ele>
        <time>2024-01-15T08:02:00Z</time>
      </trkpt>
      <trkpt lat="49.2832" lon="-123.1202">
        <ele>14</ele>
        <time>2024-01-15T08:02:30Z</time>
      </trkpt>
      <trkpt lat="49.2833" lon="-123.1201">
        <ele>15</ele>
        <time>2024-01-15T08:03:00Z</time>
      </trkpt>
      <trkpt lat="49.2834" lon="-123.1200">
        <ele>16</ele>
        <time>2024-01-15T08:03:30Z</time>
      </trkpt>
    </trkseg>
  </trk>
</gpx>
GPXEOF

# Test 1: Load a GPX file with time data that triggers activity splitting.
# The original contour (8 vertices) plus 2 activity segments should result in 3 contour_collections.
printf 'Test 1: GPX activity splitting based on time gap\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  test_gpx_with_activity_split.gpx \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "3 contour_collections loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Create a simple GPX file without time gaps (no splitting expected).
cat > test_gpx_no_split.gpx << 'GPXEOF'
<?xml version="1.0"?>
<gpx>
  <metadata>
    <name>Simple Track</name>
  </metadata>
  <trk>
    <trkseg>
      <trkpt lat="49.2827" lon="-123.1207">
        <ele>10</ele>
        <time>2024-01-15T08:00:00Z</time>
      </trkpt>
      <trkpt lat="49.2828" lon="-123.1206">
        <ele>11</ele>
        <time>2024-01-15T08:00:05Z</time>
      </trkpt>
      <trkpt lat="49.2829" lon="-123.1205">
        <ele>12</ele>
        <time>2024-01-15T08:00:10Z</time>
      </trkpt>
    </trkseg>
  </trk>
</gpx>
GPXEOF

# Test 2: Load a GPX file without significant time gaps (no splitting expected).
# Only the original contour should be loaded (1 contour_collection).
printf 'Test 2: GPX without time gaps (no splitting expected)\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  test_gpx_no_split.gpx \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "1 contour_collections loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Test 3: Verify that Line_Sample data (elevation over time) is also loaded.
printf 'Test 3: Verify elevation data is loaded as Line_Sample\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  test_gpx_with_activity_split.gpx \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "1 Line_Samples loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 

