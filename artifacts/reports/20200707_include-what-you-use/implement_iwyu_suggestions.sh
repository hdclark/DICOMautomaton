#!/usr/bin/env bash

set -eu

# Move to a standard location.
export REPO_ROOT=$(git rev-parse --show-toplevel || true)
if [ ! -d "${REPO_ROOT}" ] ; then
    printf 'Unable to find git repo root. Refusing to continue.\n' 1>&2
    exit 1
fi

set +e


# Trim any file prefixes and compiler warnings, and split the iwyu output into one-file-per-file.
grep -i -v '^warning:' iwyu_report.txt | 
  sed -e 's@/tmp/dcma_build/@@' |
  csplit --quiet --digits=6 --prefix=file_ - "/^---/+1" "{*}"

# Remove the '---' tokens.
sed -i -e 's/^---//' file_*

# Purge any files that contain compiler errors.
for f in file_* ; do
    [ ! -z "$( grep -i 'error' "${f}" )" ] && rm "${f}"
done

# Purge any files that refer to files *outside* the git repo.
#
# Note: Why would IWYU suggest these changes?!?!
for f in file_* ; do
    [ ! -z "$( grep -i '^/.*should remove.*' "${f}" )" ] && rm "${f}"
done

# For each file, attempt to implement the suggested removals.
for f in file_* ; do
   # Determine the source file name.
   src_f="$( cat "${f}" |
               grep 'should remove these lines' |
               sed -e 's@ should.*@@' )"

    # Only process files where there are no recommended additions.
    mapfile -t additions < <(
        cat "${f}" |
        sed -n -e '/should add these/,/should remove these/p' |
        tail -n +2 |
        head -n -2 )

    if [ "${#additions[@]}" != "0" ] ; then
        continue
    fi

    # Identify all subtractions in the file.
    mapfile -t removals < <(
        cat "${f}" |
          grep -- '^- ' |
          grep 'any\|optional\|limits\|cmath\|cstdlib\|string\|array\|stdexcept\|regex\|set\|list\|vector\|map\|functional\|algorithm\|utility\|tuple\|stream\|iomanip\|iosfwd\|memory\|exception\|iterator' |
          grep -i 'cgal\|boost\|eigen\|nlopt\|_functors' |
          grep -i 'ygor\|explicator' |
          `# The Ygor Boost serialization headers are falsely removed, so ignore them.` \
          grep -i -v 'BoostSerialization' |
          sed -e 's/^- //' )

    # Remove each to see if the compilation succeeds.
    for r in "${removals[@]}" ; do
        # Escape the removal.
        e="${r//\"/\\\"}"

        # Attempt to remove it.
        ( cd "${REPO_ROOT}" 
          #gawk -i inplace '{ if( index( $0, "'"${e}"'" ) == 0 ){ print $0 }else{ print "//" $0 } }' "${src_f}"
          gawk -i inplace '{ if( index( $0, "'"${e}"'" ) == 0 ){ print $0 }else{  } }' "${src_f}"
        )

    done

done

