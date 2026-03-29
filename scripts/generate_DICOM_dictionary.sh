#!/usr/bin/env bash

# generate_DICOM_dictionary.sh
#
# Generates a DICOMautomaton-compatible DICOM dictionary text file from a local
# copy of the DICOM standard Part 6 XML source (DocBook format).
#
# The input file can be obtained from:
#   https://dicom.nema.org/medical/dicom/current/source/docbook/part06/part06.xml
#
# Usage:
#   ./generate_DICOM_dictionary.sh /path/to/part06.xml [output.dict]
#
# If no output file is specified, the dictionary is written to stdout.
#
# Output format (one entry per line):
#   GGGG,EEEE VR VM Keyword [RETIRED]
#
# This output can be loaded directly by DCMA_DICOM::read_dictionary().

set -euo pipefail

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "Usage: $0 <part06.xml> [output.dict]" >&2
    echo "" >&2
    echo "Generates a DICOMautomaton-compatible DICOM dictionary from the" >&2
    echo "DICOM standard Part 6 XML source (DocBook format)." >&2
    echo "" >&2
    echo "The Part 6 XML can be obtained from:" >&2
    echo "  https://dicom.nema.org/medical/dicom/current/source/docbook/part06/part06.xml" >&2
    exit 1
fi

INPUT="${1}"
OUTPUT="${2:-/dev/stdout}"

if [ ! -f "${INPUT}" ]; then
    echo "Error: input file not found: ${INPUT}" >&2
    exit 1
fi

{
    echo "# DICOM Dictionary"
    echo "# Generated from the DICOM Part 6 XML source (DocBook format)."
    echo "# Format: GGGG,EEEE VR VM Keyword [RETIRED]"
    echo "#"

    # The Part 6 XML (DocBook format) contains tables with <row> elements.
    # Each data element row in the registry tables (Chapters 6, 7, 8) has
    # entries for: Tag, Name, Keyword, VR, VM, and an optional retirement
    # indicator (blank or "RET").
    #
    # Strategy:
    # 1. Collapse each <row>...</row> block onto a single line.
    # 2. Extract <entry> contents.
    # 3. Strip XML tags from each entry.
    # 4. Parse the resulting fields.

    # Use awk to process the XML. We track <row> blocks and <entry> elements
    # within them, accumulating stripped text for each entry. When we close a
    # row, we validate and emit a dictionary line.
    awk '
    BEGIN {
        in_row = 0
        entry_count = 0
    }

    # Detect the start of a new <row ...> element.
    /<row/ {
        in_row = 1
        entry_count = 0
        delete entry_text
        in_entry = 0
        next
    }

    # Detect the end of a </row> element.
    /<\/row>/ {
        if (!in_row) next

        # We expect at least 5 fields: Tag, Name, Keyword, VR, VM.
        # Field 6 (retirement indicator) is optional.
        if (entry_count < 5) {
            in_row = 0
            next
        }

        tag     = entry_text[1]
        # name  = entry_text[2]  # Not used in the output format.
        keyword = entry_text[3]
        vr      = entry_text[4]
        vm      = entry_text[5]
        retired = (entry_count >= 6) ? entry_text[6] : ""

        # Clean up whitespace.
        gsub(/^[[:space:]]+|[[:space:]]+$/, "", tag)
        gsub(/^[[:space:]]+|[[:space:]]+$/, "", keyword)
        gsub(/^[[:space:]]+|[[:space:]]+$/, "", vr)
        gsub(/^[[:space:]]+|[[:space:]]+$/, "", vm)
        gsub(/^[[:space:]]+|[[:space:]]+$/, "", retired)

        # Remove Unicode zero-width spaces and non-breaking spaces.
        gsub(/\xE2\x80\x8B/, "", tag)
        gsub(/\xC2\xA0/, " ", tag)
        gsub(/\xE2\x80\x8B/, "", keyword)
        gsub(/\xC2\xA0/, " ", keyword)
        gsub(/\xE2\x80\x8B/, "", vr)
        gsub(/\xE2\x80\x8B/, "", vm)

        # Extract group,element from tag like "(GGGG,EEEE)".
        gsub(/[() ]/, "", tag)

        # Skip rows where the tag contains "x" (repeating groups or ranges).
        if (tag ~ /[xX]/) { in_row = 0; next }

        # Validate tag format: exactly HHHH,HHHH.
        if (tag !~ /^[0-9A-Fa-f]{4},[0-9A-Fa-f]{4}$/) { in_row = 0; next }

        # Convert tag to uppercase.
        tag = toupper(tag)

        # Skip entries with empty VR or VM (e.g., header rows, separators).
        if (vr == "" || vm == "") { in_row = 0; next }

        # Remove internal whitespace from VR/VM.
        gsub(/ /, "", vm)

        # For multi-valued VR (e.g., "OB or OW", "US or SS"), take the first VR.
        # The DICOM dictionary maps each tag to a single 2-character VR.
        gsub(/ +or +.*/, "", vr)
        gsub(/\/.*/, "", vr)

        # Remove any remaining parentheses or brackets from keyword.
        gsub(/[()]/, "", keyword)

        # Build the output line.
        line = tag " " vr " " vm
        if (keyword != "") line = line " " keyword
        if (retired ~ /RET/) line = line " RETIRED"

        print line

        in_row = 0
        next
    }

    # Inside a <row>, track <entry> elements.
    in_row {
        # Detect start of a new <entry ...> element.
        if ($0 ~ /<entry/) {
            entry_count++
            in_entry = 1
            entry_text[entry_count] = ""
        }

        # Accumulate text for the current entry, stripping XML tags.
        if (in_entry) {
            line = $0
            gsub(/<[^>]*>/, "", line)
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", line)
            if (line != "") {
                if (entry_text[entry_count] != "") {
                    entry_text[entry_count] = entry_text[entry_count] " " line
                } else {
                    entry_text[entry_count] = line
                }
            }
        }

        # Detect end of an </entry> element.
        if ($0 ~ /<\/entry>/) {
            in_entry = 0
        }
    }
    ' "${INPUT}"

} > "${OUTPUT}"

# Report a summary to stderr.
if [ "${OUTPUT}" != "/dev/stdout" ]; then
    count=$(grep -v '^#' "${OUTPUT}" | grep -c -v '^$' || true)
    echo "Wrote ${count} dictionary entries to ${OUTPUT}" >&2
fi
