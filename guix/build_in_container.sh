#!/usr/bin/env bash
set -euo pipefail

reporoot="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
channels_file="${DCMA_GUIX_CHANNELS_FILE:-${reporoot}/guix/channels.scm}"
output_dir="${DCMA_GUIX_OUTPUT_DIR:-/out}"
package_name="${DCMA_GUIX_PACKAGE:-dicomautomaton}"
linkage="${DCMA_GUIX_LINKAGE:-shared}"
toolchain="${DCMA_GUIX_C_TOOLCHAIN:-gcc-toolchain@8}"

cd "${reporoot}"
export DCMA_GUIX_REPO_ROOT="${reporoot}"

case "${linkage}" in
    shared) ;;
    static)
        package_name="${package_name}-static"
        ;;
    *)
        printf 'Unsupported DCMA_GUIX_LINKAGE value: %s\n' "${linkage}" >&2
        exit 1
        ;;
esac

target_expression="(@ (dicomautomaton packages) ${package_name})"

mkdir -p "${output_dir}"

daemon_pid=""
if ! pgrep -x guix-daemon >/dev/null 2>&1 ; then
    guix-daemon --build-users-group=guixbuild --disable-chroot &
    daemon_pid="$!"
    trap 'if [ -n "${daemon_pid}" ] ; then kill "${daemon_pid}" ; fi' EXIT
    sleep 5
fi

guix pull -C "${channels_file}"

cmd=(
    guix time-machine -C "${channels_file}" --
    build
    -L "${reporoot}/.guix/modules"
    -e "${target_expression}"
)

if [ -n "${toolchain}" ] ; then
    cmd+=(--with-c-toolchain="${package_name}=${toolchain}")
fi

cmd+=("$@")

store_path="$("${cmd[@]}")"
ln -snf "${store_path}" "${output_dir}/${package_name}"
printf '%s\n' "${store_path}" > "${output_dir}/${package_name}.store-path"
printf 'Built %s at %s\n' "${package_name}" "${store_path}"
