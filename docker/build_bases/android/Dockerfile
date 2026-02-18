
# NOTE: This Dockerfile should be interpreted relative to the DICOMautomaton repo root.

FROM debian:bookworm

LABEL maintainer="https://www.halclark.ca" \
      description="DICOMautomaton Android build base (NDK + SDK)."

WORKDIR /scratch_base
COPY docker/build_bases/android /scratch_base

RUN /scratch_base/implementation_android_base.sh

# Expose environment variables to subsequent layers and interactive sessions.
ENV ANDROID_HOME=/opt/android-sdk
ENV ANDROID_NDK_ROOT=/opt/android-sdk/ndk/26.3.11579264
ENV PATH="${ANDROID_HOME}/cmdline-tools/latest/bin:${ANDROID_HOME}/platform-tools:${PATH}"

WORKDIR /
CMD ["/bin/bash"]
