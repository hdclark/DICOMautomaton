
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

FROM debian:bullseye

LABEL maintainer="https://www.halclark.ca" \
      description="DICOMautomaton Debian build base."

WORKDIR /scratch_base
COPY docker/build_bases/debian_bullseye /scratch_base
COPY scripts /dcma_scripts
COPY appimagekit_artifacts /appimagekit_artifacts
COPY linuxdeploy_artifacts /linuxdeploy_artifacts

RUN /scratch_base/implementation_debian_bullseye.sh

