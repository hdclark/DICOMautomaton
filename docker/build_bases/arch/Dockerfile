
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

FROM archlinux:latest

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton build base for Arch Linux."

WORKDIR /scratch_base
COPY docker/build_bases/arch /scratch_base

RUN /scratch_base/implementation_arch.sh

