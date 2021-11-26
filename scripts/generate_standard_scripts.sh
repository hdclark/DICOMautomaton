#!/usr/bin/env bash

# This script bundles the standard DCMA scripts into a C++ file so they are always available when distributing DCMA.
# This script should be called from the root directory.

# Handle spaces in the input and output file paths.
in_file="$1"
shift
while [ ! -f "$in_file" ] ; do
    in_file="$in_file $1"
    shift
done
out_file="$@"

inline_scripts=""
printf '==============================================\n'
printf '==============================================\n'
printf '==============================================\n'

pwd

for script in ./artifacts/dcma_scripts/*/*.dscr ; do
    script_path="${script}"
    # Note: the following may be more portable, but doesn't seem to work. Need to continue tweaking it...
    #script_contents="$( while read -n 1 c ; do printf '\\x%02x' "'${c}" ; done < "${script_path}" )"
    script_contents="$( hexdump -ve '1/1 "%02x"' "${script_path}" | sed -e 's@\([0-9a-fA-F][0-9a-fA-F]\)@\\x\1@g' )"
    script_category="$(printf '%s' "${script_path}" | sed -e 's@.*/\(.*\)/[^/]*@\1@g' | tr '_' ' ' )"
    script_filename="$(printf '%s' "${script_path}" | sed -e 's@.*/\(.*\)@\1@g' -e 's@[.]dscr$@@g' | tr '_' ' ' )"

    inline_scripts="${inline_scripts} {\"${script_category}\",\"${script_filename}\",\"${script_contents}\"},"
done

printf '' > "${out_file}"
while IFS= read -u 9 -r aline ; do
    if [ ! -z "$( printf '%s' "$aline" | grep '^[ ]*DCMA_SCRIPT_INLINES[ ]*' )" ] ; then
    #if [[ "$aline" =~ ^[[:space:]]*DCMA_SCRIPT_INLINES[[:space:]]*$ ]] ; then
        printf '%s\n' "${inline_scripts}" >> "${out_file}"
    else
        printf '%s\n' "${aline}" >> "${out_file}"
    fi
done 9< "${in_file}"
wc -l "${in_file}" "${out_file}"

#printf '%s' "${inline_scripts}" > /tmp/dcma_inline_scripts.cc
#cp "${out_file}" /tmp/dcma_out_file.cc
#
#cat "${in_file}" |
#    sed -e "s@[[:space:]]*DCMA_SCRIPT_INLINES@${inline_scripts}@g" > "${out_file}"


printf '==============================================\n'
printf '==============================================\n'
printf '==============================================\n'

exit


Script_Loader.cc Standard_Scripts

set(script_inlines "")
function(embed_script
         filename)
    # Read the file contents.
    file(READ "${filename}" script_contents HEX)
    #message(AUTHOR_WARNING "${script_contents}")

    # Derive a name for the script from the filename.
    set(script_path "${filename}")
    string(REGEX REPLACE ".*/(.*)/[^/]*" "\\1" script_category "${script_path}")
    string(REGEX REPLACE "_" " " script_category "${script_category}")

    string(REGEX REPLACE ".*/(.*)" "\\1" script_filename "${script_path}")
    string(REGEX REPLACE "_" " " script_filename "${script_filename}")
    string(REGEX REPLACE "[.]dscr$" "" script_filename "${script_filename}")

    # Convert the file contents to a C++ string with hex escapes.
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "\\\\x\\1" script_contents "${script_contents}")
    #message(AUTHOR_WARNING "${script_contents}")

    set(entry "{\"${script_category}\",\"${script_filename}\",\"${script_contents}\"},")

    # Append the string to a variable for later access.
    set(script_inlines "${script_inlines}" PARENT_SCOPE)
    set(script_inlines "${script_inlines}${entry}" PARENT_SCOPE)
endfunction()

file(GLOB script_files "../artifacts/dcma_scripts/*/*.dscr")
foreach(script_file ${script_files})
    embed_script("${script_file}")
endforeach()
#message(AUTHOR_WARNING "${script_inlines}")

