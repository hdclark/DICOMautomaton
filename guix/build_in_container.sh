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
daemon_socket="/var/guix/daemon-socket/socket"
disable_chroot="${DCMA_GUIX_DAEMON_DISABLE_CHROOT:-0}"
if ! pgrep -x guix-daemon >/dev/null 2>&1 ; then
    daemon_cmd=(guix-daemon --build-users-group=guixbuild)
    if [ "${disable_chroot}" = "1" ] ; then
        # Opt-in only: disabling chroot reduces Guix build isolation/reproducibility.
        daemon_cmd+=(--disable-chroot)
    fi
    "${daemon_cmd[@]}" &
    daemon_pid="$!"
    trap 'if [ -n "${daemon_pid}" ] ; then kill "${daemon_pid}" ; fi' EXIT

    daemon_ready=0
    for _ in $(seq 1 30) ; do
        if [ -S "${daemon_socket}" ] ; then
            daemon_ready=1
            break
        fi
        if ! kill -0 "${daemon_pid}" >/dev/null 2>&1 ; then
            printf 'guix-daemon exited before becoming ready\n' >&2
            wait "${daemon_pid}" || true
            exit 1
        fi
        sleep 1
    done

    if [ "${daemon_ready}" != "1" ] ; then
        printf 'Timed out waiting for guix-daemon socket: %s\n' "${daemon_socket}" >&2
        exit 1
    fi
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
