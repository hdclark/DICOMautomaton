
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

FROM debian:stable

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton Debian build base."

WORKDIR /scratch_base
COPY docker/build_bases/debian_stable /scratch_base

RUN /scratch_base/implementation_debian_stable.sh

