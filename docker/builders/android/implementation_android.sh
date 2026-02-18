#!/usr/bin/env bash
# implementation_android.sh - Cross-compiles DICOMautomaton dependencies and
# builds the Android APK using Gradle.
#
# This script is designed to run inside the Android build base Docker container
# (docker/build_bases/android/Dockerfile).  It:
#   1. Clones or uses the DICOMautomaton repository at /dcma.
#   2. Cross-compiles Boost, Ygor, Explicator, and SDL2 for each target Android ABI.
#   3. Invokes Gradle to assemble the Android APK.
#
# Output: /out/*.apk

set -eux

# ---------------------------------------------------------------------------
# Environment
# ---------------------------------------------------------------------------
source /etc/profile.d/android_toolchain.sh || true

export ANDROID_HOME="${ANDROID_HOME:-/opt/android-sdk}"
export ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-${ANDROID_HOME}/ndk/26.3.11579264}"
export JAVA_HOME="${JAVA_HOME:-$(dirname $(dirname $(readlink -f $(which javac))))}"

TARGET_ABIS="${TARGET_ABIS:-arm64-v8a x86_64}"
ANDROID_API="${ANDROID_API:-26}"

# Validate that ANDROID_API is within the range supported by the installed NDK.
validate_android_api() {
    local platforms_json="${ANDROID_NDK_ROOT}/meta/platforms.json"
    if [ ! -f "${platforms_json}" ]; then
        printf 'WARNING: %s not found; skipping API level compatibility check.\n' "${platforms_json}" >&2
        return 0
    fi
    local min_api max_api
    min_api=$(grep -o '"apiLevel":[[:space:]]*[0-9]\+' "${platforms_json}" | sed 's/[^0-9]//g' | sort -n | head -1)
    max_api=$(grep -o '"apiLevel":[[:space:]]*[0-9]\+' "${platforms_json}" | sed 's/[^0-9]//g' | sort -n | tail -1)
    if [ -n "${min_api}" ] && [ -n "${max_api}" ]; then
        if [ "${ANDROID_API}" -lt "${min_api}" ] || [ "${ANDROID_API}" -gt "${max_api}" ]; then
            printf 'ERROR: ANDROID_API=%s is outside NDK-supported range [%s, %s].\n' \
                "${ANDROID_API}" "${min_api}" "${max_api}" >&2
            exit 1
        fi
    fi
}
validate_android_api

JOBS=$(nproc)
JOBS=$(( JOBS < 8 ? JOBS : 8 ))

DEPS_PREFIX=/android-deps
mkdir -p "${DEPS_PREFIX}"
mkdir -p /out

# ---------------------------------------------------------------------------
# Ensure DICOMautomaton source is available
# ---------------------------------------------------------------------------
if [ ! -d /dcma ]; then
    printf 'Source not provided at /dcma, pulling public repository.\n'
    git clone 'https://github.com/hdclark/DICOMautomaton' /dcma
fi

# ---------------------------------------------------------------------------
# Helper: write NDK toolchain cmake file for a given ABI
# ---------------------------------------------------------------------------
make_toolchain_args() {
    local abi="$1"
    printf -- '-DCMAKE_TOOLCHAIN_FILE=%s/build/cmake/android.toolchain.cmake ' \
        "${ANDROID_NDK_ROOT}"
    printf -- '-DANDROID_ABI=%s ' "${abi}"
    printf -- '-DANDROID_PLATFORM=android-%s ' "${ANDROID_API}"
}

# ---------------------------------------------------------------------------
# Build SDL2 for Android
# ---------------------------------------------------------------------------
build_sdl2() {
    local abi="$1"
    local prefix="${DEPS_PREFIX}/${abi}"
    mkdir -p "${prefix}"

    if [ -f "${prefix}/lib/libSDL2.so" ]; then
        echo "SDL2 already built for ${abi}, skipping."; return 0
    fi

    if [ ! -d /sdl2 ]; then
        git clone --depth=1 --branch release-2.28.5 \
            'https://github.com/libsdl-org/SDL' /sdl2
    fi

    rm -rf "/tmp/sdl2_build_${abi}"
    mkdir -p "/tmp/sdl2_build_${abi}"
    cmake -S /sdl2 -B "/tmp/sdl2_build_${abi}" \
        $(make_toolchain_args "${abi}") \
        -DCMAKE_INSTALL_PREFIX="${prefix}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DSDL_SHARED=ON \
        -DSDL_STATIC=OFF \
        -GNinja

    cmake --build "/tmp/sdl2_build_${abi}" --parallel "${JOBS}"
    cmake --install "/tmp/sdl2_build_${abi}"
}

# ---------------------------------------------------------------------------
# Build Boost for Android
# ---------------------------------------------------------------------------
build_boost() {
    local abi="$1"
    local prefix="${DEPS_PREFIX}/${abi}"
    mkdir -p "${prefix}"

    if [ -f "${prefix}/lib/libboost_filesystem.a" ]; then
        echo "Boost already built for ${abi}, skipping."; return 0
    fi

    BOOST_VERSION="1.83.0"
    BOOST_DIR="/tmp/boost_$(echo ${BOOST_VERSION} | tr . _)"
    # SHA256 of the official boost_1_83_0.tar.bz2 tarball.
    BOOST_SHA256="6478edfe2f3305127cffe8caf73ea0176c53769f4bf1585be237eb30798c3b8e"

    if [ ! -d "${BOOST_DIR}" ]; then
        BOOST_URL="https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VERSION}/source/boost_$(echo ${BOOST_VERSION} | tr . _).tar.bz2"
        wget --quiet -O /tmp/boost.tar.bz2 "${BOOST_URL}"
        # Verify checksum before extraction to guard against corrupted/tampered archives.
        echo "${BOOST_SHA256}  /tmp/boost.tar.bz2" | sha256sum -c - || {
            printf 'ERROR: Boost tarball SHA256 mismatch. Refusing to continue.\n' >&2
            exit 1
        }
        tar -xjf /tmp/boost.tar.bz2 -C /tmp/
    fi

    if [ ! -f "${BOOST_DIR}/b2" ]; then
        cd "${BOOST_DIR}"
        ./bootstrap.sh --without-libraries=python
    fi

    case "${abi}" in
        arm64-v8a)   ndk_arch="arm64"  ; ndk_triple="aarch64-linux-android${ANDROID_API}"  ;;
        armeabi-v7a) ndk_arch="arm"    ; ndk_triple="armv7a-linux-androideabi${ANDROID_API}" ;;
        x86_64)      ndk_arch="x86_64" ; ndk_triple="x86_64-linux-android${ANDROID_API}"    ;;
        x86)         ndk_arch="x86"    ; ndk_triple="i686-linux-android${ANDROID_API}"      ;;
        *) echo "Unknown ABI ${abi}"; return 1 ;;
    esac

    TOOLCHAIN="${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/linux-x86_64"
    CXX="${TOOLCHAIN}/bin/${ndk_triple}-clang++"
    CC="${TOOLCHAIN}/bin/${ndk_triple}-clang"

    cat > "/tmp/boost_user_config_${abi}.jam" <<JAM_EOF
using clang : android_${ndk_arch}
    : ${CXX}
    : <cxxflags>"-fPIC"
      <compileflags>"-fPIC"
    ;
JAM_EOF

    cd "${BOOST_DIR}"
    ./b2 \
        --user-config="/tmp/boost_user_config_${abi}.jam" \
        toolset="clang-android_${ndk_arch}" \
        target-os=android \
        link=static \
        threading=multi \
        --prefix="${prefix}" \
        --with-filesystem \
        --with-iostreams \
        --with-serialization \
        --with-thread \
        --with-system \
        -j "${JOBS}" \
        install
}

# ---------------------------------------------------------------------------
# Build Ygor for Android
# ---------------------------------------------------------------------------
build_ygor() {
    local abi="$1"
    local prefix="${DEPS_PREFIX}/${abi}"

    if [ -f "${prefix}/lib/libygor.a" ]; then
        echo "Ygor already built for ${abi}, skipping."; return 0
    fi

    if [ ! -d /ygor ]; then
        git clone --depth=1 'https://github.com/hdclark/Ygor' /ygor
    fi

    rm -rf "/tmp/ygor_build_${abi}"
    mkdir -p "/tmp/ygor_build_${abi}"
    cmake -S /ygor -B "/tmp/ygor_build_${abi}" \
        $(make_toolchain_args "${abi}") \
        -DCMAKE_INSTALL_PREFIX="${prefix}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DWITH_LINUX_SYS=OFF \
        -DBOOST_ROOT="${prefix}" \
        -GNinja

    cmake --build "/tmp/ygor_build_${abi}" --parallel "${JOBS}"
    cmake --install "/tmp/ygor_build_${abi}"
}

# ---------------------------------------------------------------------------
# Build Explicator for Android
# ---------------------------------------------------------------------------
build_explicator() {
    local abi="$1"
    local prefix="${DEPS_PREFIX}/${abi}"

    if [ -f "${prefix}/lib/libexplicator.a" ]; then
        echo "Explicator already built for ${abi}, skipping."; return 0
    fi

    if [ ! -d /explicator ]; then
        git clone --depth=1 'https://github.com/hdclark/Explicator' /explicator
    fi

    rm -rf "/tmp/explicator_build_${abi}"
    mkdir -p "/tmp/explicator_build_${abi}"
    cmake -S /explicator -B "/tmp/explicator_build_${abi}" \
        $(make_toolchain_args "${abi}") \
        -DCMAKE_INSTALL_PREFIX="${prefix}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -GNinja

    cmake --build "/tmp/explicator_build_${abi}" --parallel "${JOBS}"
    cmake --install "/tmp/explicator_build_${abi}"
}

# ---------------------------------------------------------------------------
# Build dependencies for each target ABI
# ---------------------------------------------------------------------------
for abi in ${TARGET_ABIS}; do
    echo "=== Building dependencies for ABI: ${abi} ==="
    build_sdl2 "${abi}"
    build_boost "${abi}"
    build_ygor "${abi}"
    build_explicator "${abi}"
done

# ---------------------------------------------------------------------------
# Configure Gradle
# ---------------------------------------------------------------------------
cat > /dcma/android/local.properties <<EOF
sdk.dir=${ANDROID_HOME}
ndk.dir=${ANDROID_NDK_ROOT}
EOF

# ---------------------------------------------------------------------------
# Run Gradle to build the APK
# ---------------------------------------------------------------------------
cd /dcma/android

chmod +x gradlew

./gradlew assembleDebug \
    --no-daemon \
    --stacktrace \
    2>&1 | tee /tmp/gradle_build.log

# Collect output APK.
find /dcma/android -name "*.apk" -exec cp {} /out/ \;

echo "Build complete. APK(s) in /out/:"
ls -lh /out/*.apk 2>/dev/null || echo "(no APKs found)"
