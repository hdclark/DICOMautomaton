
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

FROM archlinux:latest

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton build base for Arch Linux."

WORKDIR /scratch_base
COPY docker/build_bases/arch /scratch_base

RUN /scratch_base/implementation_arch.sh

# Installs ITK library in /itk needed for nSIFT
# set ITK_IDR=/itk when building using ITK
#
# Build and run (from repo root):
#   docker build -t hdclark/dcma_build_base_arch:latest -f docker/arch/Dockerfile .
RUN git clone --branch v5.2.1 https://github.com/InsightSoftwareConsortium/ITK /tmp/itk \
    && mkdir /itk \
    && mkdir /tmp/itk-build && cd /tmp/itk-build \
    && cmake -DCMAKE_INSTALL_PREFIX=/itk -DBUILD_EXAMPLES:BOOL=OFF -DBUILD_TESTING:BOOL=OFF /tmp/itk \
    && make install \
    && rm -rf /tmp/itk*
