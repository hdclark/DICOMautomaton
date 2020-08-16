
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

# NOTE: The following can be overridden using `docker build --build-arg BASE_CONTAINER_NAME=xyz`.
ARG BASE_CONTAINER_NAME="dcma_build_base_mxe"
FROM "$BASE_CONTAINER_NAME"

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton MXE cross-compiler."

WORKDIR /scratch
COPY docker/builders/mxe /scratch
COPY . /dcma

RUN /scratch/implementation_mxe.sh

# Default to a no-op.
WORKDIR /scratch
CMD ["/bin/false"]

