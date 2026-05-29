
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

FROM archlinux:latest

LABEL maintainer="https://www.halclark.ca" \
      description="DICOMautomaton build base for Arch Linux with SYCL."

WORKDIR /scratch_base
COPY docker/build_bases/arch_sycl /scratch_base
COPY scripts /dcma_scripts
COPY appimagekit_artifacts /appimagekit_artifacts
COPY linuxdeploy_artifacts /linuxdeploy_artifacts

RUN /scratch_base/implementation_arch_sycl.sh

