
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

FROM voidlinux/voidlinux-musl

LABEL maintainer="https://www.halclark.ca" \
      description="DICOMautomaton Void Linux build base."

WORKDIR /scratch_base
COPY docker/build_bases/void /scratch_base
COPY scripts /dcma_scripts
COPY appimagekit_artifacts /appimagekit_artifacts
COPY linuxdeploy_artifacts /linuxdeploy_artifacts

# Ensure something in the image can interpret bash she-bangs.
RUN xbps-install -y -S xbps && \
    xbps-install -y -S bash

RUN /scratch_base/implementation_void.sh

