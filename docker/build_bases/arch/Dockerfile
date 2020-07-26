
FROM archlinux/base

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton build base for Arch Linux."

WORKDIR /scratch_base
COPY docker/build_bases/arch /scratch_base

RUN /scratch_base/implementation_arch.sh

