
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

FROM debian:oldoldstable

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton Debian (oldoldstable) build base."

WORKDIR /scratch_base
COPY docker/build_bases/debian_oldoldstable /scratch_base

RUN /scratch_base/implementation_debian_oldoldstable.sh

