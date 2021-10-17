
# NOTE: This Dockerfile should be interepretted relative to the DICOMautomaton repo root.

FROM archlinux:latest

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton build base for Arch Linux."

WORKDIR /scratch_base
COPY docker/build_bases/arch /scratch_base

RUN /scratch_base/implementation_arch.sh

# Added for remote development
# CLion remote docker environment (How to build docker container, run and stop it)
#
# Build and run (from repo root):
#   docker build -t hdclark/dcma_build_base_arch:latest -f docker/arch/Dockerfile .
#   docker run -d --cap-add sys_ptrace -p 127.0.0.1:2222:22 --name capstone_env hdclark/dcma_build_base_arch:latest
#
# stop:
#   docker stop capstone_env
#
# ssh credentials (test user):
#   user@password

RUN pacman -Sy --noconfirm --needed openssh
RUN rm -f /var/cache/pacman/pkg/*

RUN ( \
    echo 'LogLevel DEBUG2'; \
    echo 'PermitRootLogin yes'; \
    echo 'PasswordAuthentication yes'; \
    echo 'Subsystem sftp /usr/lib/ssh/sftp-server'; \
  ) > /etc/ssh/sshd_config_clion \
  && mkdir /run/sshd

RUN useradd -m user \
  && yes password | passwd user

RUN usermod -s /bin/bash user

CMD ["/usr/sbin/sshd", "-D", "-e", "-f", "/etc/ssh/sshd_config_clion"]

RUN ssh-keygen -A