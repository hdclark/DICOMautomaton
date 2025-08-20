
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

# NOTE: The following can be overridden using `docker build --build-arg BASE_CONTAINER_NAME=xyz`.
ARG BASE_CONTAINER_NAME="dcma_build_base_debian_buster"
FROM "$BASE_CONTAINER_NAME"

LABEL maintainer="https://www.halclark.ca" \
      description="DICOMautomaton Web Server built from Debian buster."

EXPOSE 80
EXPOSE 443

WORKDIR /scratch
COPY ./docker/builders/debian_buster /scratch
COPY . /dcma

RUN /scratch/implementation_debian_buster.sh

# Default to launching the webserver.
WORKDIR /scratch
CMD ["/bin/bash", "/scratch/Run_WebServer.sh"]

