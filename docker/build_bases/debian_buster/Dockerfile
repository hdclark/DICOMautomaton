
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

FROM debian:buster

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton Debian build base."

WORKDIR /scratch_base
COPY docker/build_bases/debian_buster /scratch_base

RUN /scratch_base/implementation_debian_buster.sh

