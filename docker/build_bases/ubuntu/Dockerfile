
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

FROM debian:oldstable

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton Ubuntu build base."

WORKDIR /scratch_base
COPY docker/build_bases/ubuntu /scratch_base

RUN /scratch_base/implementation_ubuntu.sh

