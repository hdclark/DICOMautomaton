
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

FROM debian:oldstable

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton MXE build base."

WORKDIR /scratch_base
COPY docker/build_bases/mxe /scratch_base

RUN /scratch_base/implementation_mxe.sh

