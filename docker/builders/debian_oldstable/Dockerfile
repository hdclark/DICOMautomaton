
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

# NOTE: The following can be overridden using `docker build --build-arg BASE_CONTAINER_NAME=xyz`.
ARG BASE_CONTAINER_NAME="dcma_build_base_debian_oldstable"
FROM "$BASE_CONTAINER_NAME"

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton Web Server built from Debian OldStable."

EXPOSE 80
EXPOSE 443

WORKDIR /scratch
COPY ./docker/builders/debian_oldstable /scratch
COPY . /dcma

RUN /scratch/implementation_debian_oldstable.sh

# Default to launching the webserver.
WORKDIR /scratch
CMD ["/bin/bash", "/scratch/Run_WebServer.sh"]

