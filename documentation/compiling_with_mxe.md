
# Compilation using MXE ("M cross environment")

Attempted unsuccessfully to compile custom dependencies on 2020 July 14. The toolchain seems great, but the code will
need to be ported. So cygwin or WSL may be better options. Here are the step I used to assemble a toolchain with many
(all?) of the upstream DCMA dependencies.

See <https://mxe.cc/#tutorial> and <https://mxe.cc/#requirements-debian> for more information. The following is
essentially a lightly customized version of the MXE tutorial.

    mkdir -pv /mxe
    git clone https://github.com/mxe/mxe.git .

    apt-get install \
      autoconf \
      automake \
      autopoint \
      bash \
      bison \
      bzip2 \
      flex \
      g++ \
      g++-multilib \
      gettext \
      git \
      gperf \
      intltool \
      libc6-dev-i386 \
      libgdk-pixbuf2.0-dev \
      libltdl-dev \
      libssl-dev \
      libtool-bin \
      libxml-parser-perl \
      lzip \
      make \
      openssl \
      p7zip-full \
      patch \
      perl \
      python \
      ruby \
      sed \
      unzip \
      wget \
      xz-utils

    cd mxe
    #echo 'override MXE_PLUGIN_DIRS += plugins/gcc9' >> settings.mk  # Update compiler version.
    rm src/ocaml*  # Currently failing, easiest to just delete it.

    # Builds a gcc5.5 compiler toolchain by default.
    #make # note this could take hours

    # Builds a newer toolchain.
    make MXE_TARGETS=x86_64-w64-mingw32.static.gcc9

    export PATH="/mxe/usr/bin:$PATH"

    unset `env | \
      grep -vi '^EDITOR=\|^HOME=\|^LANG=\|MXE\|^PATH=' | \
      grep -vi 'PKG_CONFIG\|PROXY\|^PS1=\|^TERM=' | \
      cut -d '=' -f1 | tr '\n' ' '`

Cross-compiled libraries and upstream packages are installed with the `/mxe/usr/x86_64-w64-mingw32.static.gcc9/` prefix.
Installation to a custom prefix will simplify extraction of final build artifacts. Note that the `MXE` `CMake` wrapper
will properly source from the `MXE` prefix directory, so we only need to tell the toolchain where to look for custom
dependencies.

    mkdir -pv /out/{bin,lib,include,share}

    cd /ygor
    rm -rf build/
    mkdir build
    cd build 
    CXXFLAGS="-I/out/include" LDFLAGS="-L/out/lib" \
      x86_64-w64-mingw32.static.gcc9-cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -DWITH_LINUX_SYS=OFF ../
    make --ignore-errors  # --keep-going
    make install DESTDIR="/out"   # Confirm this prefix is legit!

Here is where the attempt stopped. There were lots of `POSIX` calls that caused compilation issues which will need to be
ported before I can continue. See the error logs. Note that they are likely incomplete!

