
#FROM archlinux:latest
FROM debian:oldstable

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton minimal CI build."

WORKDIR /scratch
COPY docker/builders/ci /scratch
COPY . /dcma

#RUN /scratch/implementation_ci_arch.sh
RUN /scratch/implementation_ci_debian_oldstable.sh

# Default to a no-op.
WORKDIR /scratch
CMD ["/bin/false"]

