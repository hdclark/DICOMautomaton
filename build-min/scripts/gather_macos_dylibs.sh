#!/usr/bin/env bash

# This script gathers a DICOMautomaton installation and required dependencies from a macos system to create a maximally
# self-contained, relocatable, and portable bundle for distributing the binary executables.
#
# This script requires a root directory from an existing installation, and a root directory for the outgoing bundle.
#
# The root dir contains the installed binaries, libraries, and other artifacts. This would nominally be '/usr' on Linux,
# but is likely something else to isolate the DICOMautomaton artifacts from the system artifacts. It should only contain
# the artifacts from a DICOMautomaton installation and any other binaries needed to be portable.
#
# Note that the binary executables (in .../bin/) and dynamic libraries (in .../lib/) will be modified. Some external
# libraries (provided by the building toolchain, but not from the OS) will be copied in to maximize relocatability.

set -eux

export rootdir="$1"   # The equivalent of /usr/local or wherever DCMA was installed.
export destdir="$2"   # The location where the bundle directory will be placed.

if [ ! -d "${rootdir}" ] ; then
    printf 'Root installation directory is not accessible.\n' 1>&2
    exit 1
fi
if [ ! -d "${rootdir}/bin/" ] || [ ! -d "${rootdir}/lib/" ] ; then
    printf 'Root installation directory is missing bin and/or lib directories.\n' 1>&2
    exit 1
fi
if [ -d "${destdir}" ] ; then
    printf 'Bundle destination directory already exists. Refusing to continue.\n' 1>&2
    exit 1
fi

mkdir -pv "${destdir}"/{bin,lib}/
rsync -rvP "${rootdir}/" "${destdir}/"


# This function copies all referenced dependencies to the destination directory.
function ingress_dependencies {
    local d="$1"
    local f="$2"

    while read adep ; do
        cp -L -n "${adep}" "${d}" || true

    done < <( otool -L "${f}" |
                grep -E '\t' |
                sed -e 's/[ :].*//g' |
                sed -e 's@^[^/]*@@g' |
                grep -E -v '^@' |
                grep -E -v '^/lib' |
                grep -E -v '^/usr/lib' |
                grep -E -v '^/System'    )

}
export -f ingress_dependencies

# This function prints a list of library 'install names' which are embedded into executables and libraries.
# It omits system libraries, which are considered immutable for the purposes of this script.
function list_lib_install_names {
    local f="$1"

    otool -l "${f}" | 
    sed -n '/cmd LC_LOAD_DYLIB/,/name /p' |
    grep 'name ' |
    sed -e 's#.*name \([^ ]*\).*#\1#g' |
    grep -E -v '^/lib' |
    grep -E -v '^/usr/lib' |
    grep -E -v '^/System'
}
export -f list_lib_install_names

# This function prints a list of rpaths embedded into executables and libraries.
function list_rpaths {
    local f="$1"

    otool -l "${f}" | 
    sed -n '/cmd LC_RPATH/,/path /p' |
    grep 'path ' |
    grep -E -v '@' |
    sed -e 's#.*path \([^ ]*\).*#\1#g' |
    grep -E -v '^/lib' |
    grep -E -v '^/usr/lib' |
    grep -E -v '^/System'
}
export -f list_rpaths


# Scan the binaries to gather explicitly named (first-order) dependencies.
for f in "${rootdir}"/bin/* ; do
    ingress_dependencies "${destdir}"/lib/ "${f}"
done

# Now scan the dependencies to find their dependencies recursively.
#
# Note the we could add feedback here so that the recursion stops when all dependencies are present.
# However, in practice the level of recursion is small. So it's easier to just recurse a fixed number
# of times and be idempotent when adding dependencies.
for i in `seq 1 5` ; do
    for f in "${destdir}"/lib/* ; do
        ingress_dependencies "${destdir}"/lib/ "${f}"
    done
done


# Now we scan for dependencies and modify hardcoded paths so the linker can find bundled dependencies.
for f in "${destdir}"/bin/* "${destdir}"/lib/* ; do

    # First, add an rpath entry.
    set +e
    z="$( otool -l "${f}" | grep '@executable_path/../lib')"
    [ -z "${z}" ] && install_name_tool -add_rpath '@executable_path/../lib' "$f"
    set -e

    # Second, modify the 'install name' of the relocatable libraries.
    for l in $(list_lib_install_names "${f}") ; do
        install_name_tool -change "${l}" "@executable_path/../lib/$(basename "${l}")"  "${f}"
    done

    # Third, remove any additional rpaths.
    for r in $(list_rpaths "${f}") ; do
        install_name_tool -delete_rpath "${r}" "${f}"
    done
    
done

# Now update the library ID since it still hardcodes the previous absolute path.
for f in "${destdir}"/lib/* ; do
    install_name_tool -id "$(basename "${f}")"  "${f}"
done

echo OK

