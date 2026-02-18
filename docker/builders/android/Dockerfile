
# NOTE: This Dockerfile should be interpreted relative to the DICOMautomaton repo root.

# NOTE: The following can be overridden using `docker build --build-arg BASE_CONTAINER_NAME=xyz`.
ARG BASE_CONTAINER_NAME="dcma_build_base_android"
FROM "$BASE_CONTAINER_NAME"

LABEL maintainer="https://www.halclark.ca" \
      description="DICOMautomaton Android APK builder."

WORKDIR /scratch
COPY docker/builders/android /scratch
COPY . /dcma

RUN /scratch/implementation_android.sh

# Default to a no-op.
WORKDIR /scratch
CMD ["/bin/false"]
