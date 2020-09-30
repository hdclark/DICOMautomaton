
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

FROM archlinux:latest

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton build base for Arch Linux with SYCL."

WORKDIR /scratch_base
COPY docker/devel/arch_sycl /scratch_base

RUN /scratch_base/implementation_arch_sycl.sh

