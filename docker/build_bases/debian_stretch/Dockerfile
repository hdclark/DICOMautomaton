
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

FROM debian:stretch

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton Debian (stretch) build base."

WORKDIR /scratch_base
COPY docker/build_bases/debian_stretch /scratch_base

RUN /scratch_base/implementation_debian_stretch.sh

