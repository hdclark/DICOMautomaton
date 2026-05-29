
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

FROM debian:bullseye

LABEL maintainer="https://www.halclark.ca" \
      description="DICOMautomaton MXE build base."

WORKDIR /scratch_base
COPY docker/build_bases/mxe /scratch_base
COPY scripts /dcma_scripts
COPY appimagekit_artifacts /appimagekit_artifacts
COPY linuxdeploy_artifacts /linuxdeploy_artifacts
COPY thrift-0.18.1.tar.gz /
COPY thrift.mk /

RUN /scratch_base/implementation_mxe.sh

