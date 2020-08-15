
FROM debian:stable

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton MXE build."

WORKDIR /scratch
COPY docker/builders/mxe /scratch
COPY . /dcma

RUN /scratch/implementation_mxe.sh

# Default to a no-op.
WORKDIR /scratch
CMD ["/bin/false"]

