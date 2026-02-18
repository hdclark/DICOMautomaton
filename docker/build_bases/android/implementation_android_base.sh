#!/usr/bin/env bash
# implementation_android_base.sh - Sets up the Android NDK and SDK build environment.
#
# This script installs the Android SDK command-line tools, NDK, and all
# host-side build dependencies needed to cross-compile DICOMautomaton for
# Android using Gradle + CMake.
#
# It is intended to be run inside a Debian-based Docker container and produces
# an image ready to be used by docker/builders/android/implementation_android.sh.

set -eux

export DEBIAN_FRONTEND=noninteractive

# ---------------------------------------------------------------------------
# Host build tools
# ---------------------------------------------------------------------------
apt-get update --yes
apt-get install --yes --no-install-recommends \
    wget \
    unzip \
    curl \
    git \
    ca-certificates \
    make \
    cmake \
    ninja-build \
    g++ \
    pkg-config \
    openjdk-17-jdk-headless \
    python3 \
    rsync \
    file \
    coreutils \
    binutils \
    findutils \
    zip

# ---------------------------------------------------------------------------
# Android SDK command-line tools
# ---------------------------------------------------------------------------
export ANDROID_HOME=/opt/android-sdk
mkdir -p "${ANDROID_HOME}/cmdline-tools"

# Download the latest Android command-line tools.
# See: https://developer.android.com/studio#command-line-tools-only
CMDLINE_TOOLS_VERSION="11076708"
CMDLINE_TOOLS_URL="https://dl.google.com/android/repository/commandlinetools-linux-${CMDLINE_TOOLS_VERSION}_latest.zip"

wget --quiet -O /tmp/cmdline-tools.zip "${CMDLINE_TOOLS_URL}"
unzip -q /tmp/cmdline-tools.zip -d /tmp/cmdline-tools-extract
mv /tmp/cmdline-tools-extract/cmdline-tools "${ANDROID_HOME}/cmdline-tools/latest"
rm -rf /tmp/cmdline-tools.zip /tmp/cmdline-tools-extract

export PATH="${ANDROID_HOME}/cmdline-tools/latest/bin:${PATH}"

# Accept all SDK licenses non-interactively and warn if acceptance appears to fail.
yes | sdkmanager --licenses 2>&1 | grep -q "All SDK package licenses accepted" \
    || echo "Warning: Android SDK license acceptance may have failed"

# ---------------------------------------------------------------------------
# Android SDK build-tools and platform
# ---------------------------------------------------------------------------
sdkmanager "platform-tools"
sdkmanager "build-tools;34.0.0"
sdkmanager "platforms;android-34"

# ---------------------------------------------------------------------------
# Android NDK
# ---------------------------------------------------------------------------
NDK_VERSION="26.3.11579264"
sdkmanager "ndk;${NDK_VERSION}"

export ANDROID_NDK_ROOT="${ANDROID_HOME}/ndk/${NDK_VERSION}"

# ---------------------------------------------------------------------------
# Write environment profile
# ---------------------------------------------------------------------------
cat > /etc/profile.d/android_toolchain.sh <<'EOF'
export ANDROID_HOME=/opt/android-sdk
export ANDROID_NDK_ROOT="${ANDROID_HOME}/ndk/26.3.11579264"
export PATH="${ANDROID_HOME}/cmdline-tools/latest/bin:${ANDROID_HOME}/platform-tools:${PATH}"
EOF

chmod 644 /etc/profile.d/android_toolchain.sh

echo "Android build base setup complete."
echo "  ANDROID_HOME      = ${ANDROID_HOME}"
echo "  ANDROID_NDK_ROOT  = ${ANDROID_NDK_ROOT}"
