#!/usr/bin/env bash

# convert_dict_to_cpp.sh
#
# Converts a DICOMautomaton-compatible DICOM dictionary text file into C++
# source code that can be copy-pasted into the get_default_dictionary()
# function in src/DCMA_DICOM_Dictionaries.cc.
#
# Usage:
#   ./convert_dict_to_cpp.sh /path/to/input.dict [output.cpp]
#
# If no output file is specified, the C++ source is written to stdout.
#
# Input format (one entry per line, as produced by generate_DICOM_dictionary.sh):
#   GGGG,EEEE VR VM Keyword [RETIRED]
#
# Output format (C++ initializer list entries):
#   {{0xGGGG, 0xEEEE}, {"VR", "Keyword", "VM", false}},

set -euo pipefail

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "Usage: $0 <input.dict> [output.cpp]" >&2
    echo "" >&2
    echo "Converts a DICOM dictionary text file into C++ source code" >&2
    echo "suitable for embedding in src/DCMA_DICOM_Dictionaries.cc." >&2
    echo "" >&2
    echo "Input format (as produced by generate_DICOM_dictionary.sh):" >&2
    echo "  GGGG,EEEE VR VM Keyword [RETIRED]" >&2
    exit 1
fi

INPUT="${1}"
OUTPUT="${2:-/dev/stdout}"

if [ ! -f "${INPUT}" ]; then
    echo "Error: input file not found: ${INPUT}" >&2
    exit 1
fi

{
    echo "    static const DICOMDictionary dict = {"

    # Process non-comment, non-blank lines from the dictionary file.
    awk '
    /^[[:space:]]*#/ { next }
    /^[[:space:]]*$/ { next }
    {
        # Expected format: GGGG,EEEE VR VM Keyword [RETIRED]
        tag     = $1
        vr      = $2
        vm      = $3
        keyword = $4
        retired = ($5 == "RETIRED") ? "true" : "false"

        # Split group and element from tag (e.g., "0008,0016").
        n = split(tag, parts, ",")
        if (n != 2) next

        group   = "0x" toupper(parts[1])
        element = "0x" toupper(parts[2])

        # Emit a C++ initializer list entry.
        printf "        {{%s, %s}, {\"%s\", \"%s\", \"%s\", %s}},\n", \
            group, element, vr, keyword, vm, retired
    }
    ' "${INPUT}"

    echo "    };"

} > "${OUTPUT}"

# Report a summary to stderr.
if [ "${OUTPUT}" != "/dev/stdout" ]; then
    count=$(grep -c '{{0x' "${OUTPUT}" || true)
    echo "Wrote ${count} C++ dictionary entries to ${OUTPUT}" >&2
fi
