
#FROM archlinux/base
FROM debian:stable

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton minimal CI build."

WORKDIR /scratch
COPY docker/builders/ci /scratch
COPY . /dcma

#RUN /scratch/implementation_ci_arch.sh
RUN /scratch/implementation_ci_debian_stable.sh

# Default to a no-op.
WORKDIR /scratch
CMD ["/bin/false"]

