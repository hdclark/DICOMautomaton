#!/usr/bin/env bash

# This script bundles the standard DCMA scripts into a C++ file so they are always available when distributing DCMA.
# This script should be called from the root directory.

set -eux

# Handle spaces in the input and output file paths.
in_file="$1"
shift
while [ ! -f "$in_file" ] ; do
    in_file="$in_file $1"
    shift
done
out_file="$@"

function command_exists () {
    type "$1" &> /dev/null ;
}
export -f command_exists

if ! command_exists grep ; then
    printf 'This script requires grep.' 1>&2
    exit 1
fi
if ! command_exists tr ; then
    printf 'This script requires tr.' 1>&2
    exit 1
fi
if ! command_exists sed ; then
    printf 'This script requires sed.' 1>&2
    exit 1
fi

# Convert the scripts to entries in a C++-style initializer list.
# To simplify inclusion, we convert the contents to hex data and embed in a string.
inline_scripts=""
for script in ./artifacts/dcma_scripts/*/*.dscr ; do
    script_path="${script}"

    # Note: the following may be more portable, but doesn't seem to work. Need to continue tweaking it...
    #script_contents="$( while read -n 1 c ; do printf '\\x%02x' "'${c}" ; done < "${script_path}" )"
    if command_exists hexdump ; then
        raw_script_contents="$( cat "${script_path}" | hexdump -ve '1/1 "%02x"' )" 
    elif command_exists od ; then
        raw_script_contents="$( cat "${script_path}" | od -v -t x1 -An | tr -d "\n "~ )" 
    elif command_exists xxd ; then
        raw_script_contents="$( cat "${script_path}" | xxd -p | tr -d '\n' )" 
    else
        printf 'Unable to find suitable hex dump utility.' 1>&2
        exit 1
    fi

    # Add C/C++-style '\x' prefix so it can be included into a C++ source file.
    script_contents="$( printf '%s' "${raw_script_contents}" | sed -e 's@\([0-9a-fA-F][0-9a-fA-F]\)@\\x\1@g' )"

    script_category="$( printf '%s' "${script_path}" | sed -e 's@.*/\(.*\)/[^/]*@\1@g' | tr '_' ' ' )"
    script_filename="$( printf '%s' "${script_path}" | sed -e 's@.*/\(.*\)@\1@g' -e 's@[.]dscr$@@g' | tr '_' ' ' )"

    inline_scripts="${inline_scripts} {\"${script_category}\",\"${script_filename}\",\"${script_contents}\"},"
done

# Filter the input file, emitting the stringified script data into the output file when a bare C++ macro is observed.
printf '' > "${out_file}"
while IFS= read -u 9 -r aline ; do
    if [ ! -z "$( printf '%s' "$aline" | grep '^[ ]*DCMA_SCRIPT_INLINES[ ]*' )" ] ; then
    #if [[ "$aline" =~ ^[[:space:]]*DCMA_SCRIPT_INLINES[[:space:]]*$ ]] ; then
        printf '%s\n' "${inline_scripts}" >> "${out_file}"
    else
        printf '%s\n' "${aline}" >> "${out_file}"
    fi
done 9< "${in_file}"

