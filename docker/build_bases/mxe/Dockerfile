
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

FROM debian:bullseye

LABEL maintainer="https://www.halclark.ca" \
      description="DICOMautomaton MXE build base."

WORKDIR /scratch_base
COPY docker/build_bases/mxe /scratch_base
COPY scripts /dcma_scripts

RUN /scratch_base/implementation_mxe.sh

