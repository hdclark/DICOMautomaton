
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

FROM debian:buster

LABEL maintainer="https://www.halclark.ca" \
      description="DICOMautomaton Debian build base."

WORKDIR /scratch_base
COPY docker/build_bases/debian_buster /scratch_base
COPY scripts /dcma_scripts

RUN /scratch_base/implementation_debian_buster.sh

