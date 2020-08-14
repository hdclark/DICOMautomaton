
# Compilation using MXE ("M cross environment")

Attempted unsuccessfully to compile custom dependencies on 2020 July 14. The toolchain seems great, but the code will
need to be ported. So cygwin or WSL may be better options. Here are the step I used to assemble a toolchain with many
(all?) of the upstream DCMA dependencies.

See <https://mxe.cc/#tutorial> and <https://mxe.cc/#requirements-debian> for more information. Perform the following
instructions inside a recent `Debian` `Docker` container (or VM, instance, or bare metal installation). The following is
essentially a lightly customized version of the MXE tutorial.

    apt-get -y update
    apt-get -y install \
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
      ca-certificates \
      rsync \
      xz-utils

    git clone https://github.com/mxe/mxe.git /mxe
    cd /mxe

    # Builds a newer toolchain.
    #export TOOLCHAIN="x86_64-w64-mingw32.shared"
    export TOOLCHAIN="x86_64-w64-mingw32.static"

    # Build the MXE environment. Note this could take hours.
    # Builds a gcc5.5 compiler toolchain by default.
    #make -j4 --keep-going || true
    # Should build gcc9, but still builds gcc5.5 (default).
    #make -j4 --keep-going MXE_TARGETS="${TOOLCHAIN}.gcc9" || true
    # Works, but was purportedly deprecated in 2018.
    #echo 'override MXE_PLUGIN_DIRS += plugins/gcc9' >> settings.mk  # Update compiler version.
    make -j4 --keep-going MXE_TARGETS="${TOOLCHAIN}" MXE_PLUGIN_DIRS=plugins/gcc9 || true

    export PATH="/mxe/usr/bin:$PATH"

    # Confirm the compiler version is as-advertised.
    "${TOOLCHAIN}-g++" --version

    unset `env | \
      grep -vi '^EDITOR=\|^HOME=\|^LANG=\|MXE\|^PATH=' | \
      grep -vi 'PKG_CONFIG\|PROXY\|^PS1=\|^TERM=\|TOOLCHAIN=' | \
      cut -d '=' -f1 | tr '\n' ' '`

Cross-compiled libraries and upstream packages are installed with the `/mxe/usr/${TOOLCHAIN}/` prefix.
Installation to a custom prefix will simplify extraction of final build artifacts. Note that the `MXE` `CMake` wrapper
will properly source from the `MXE` prefix directory, so we only need to tell the toolchain where to look for custom
dependencies.

    git clone 'https://github.com/hdclark/Ygor' /ygor
    git clone 'https://github.com/hdclark/YgorClustering' /ygorclustering
    git clone 'https://github.com/hdclark/Explicator' /explicator
    git clone 'https://github.com/hdclark/DICOMautomaton' /dcma

Patch the sources to support cross-compilation and building static artifacts.

Comment out the following lines in CMakeLists.txt's:

    #set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    #set(BUILD_SHARED_LIBS TRUE)
    #set(POSITION_INDEPENDENT_CODE TRUE)
    #set(THREADS_PREFER_PTHREAD_FLAG ON)

Replace the following libraries with available counterparts:

    sed -i -e 's/boost_filesystem/boost_filesystem-mt/g' \
           -e 's/boost_serialization/boost_serialization-mt/g' \
           -e 's/boost_iostreams/boost_iostreams-mt/g' \
           -e 's/boost_thread/boost_thread_win32-mt wsock32/g' \
           -e 's/boost_system/boost_system-mt/g' \
           -e 's/sfml-graphics/sfml-graphics-s sfml-main/g' \       <---- needs to be fixed.
           -e 's/sfml-window/sfml-window-s/g' \
           -e 's/sfml-system/sfml-system-s/g' \
           /dcma/{,src/}CMakeLists.txt

Add 'wsock32' library to linking commands in src/CMakeLists.txt after boost_thread_win32-wt
(Should be part of FindBoost if I used that properly??)

Address the following ASIO request:
    In file included from /mxe/usr/x86_64-w64-mingw32.static/include/asio/associated_allocator.hpp:18,
                     from /mxe/usr/x86_64-w64-mingw32.static/include/asio.hpp:18,
                     from /dcma/src/Operations/ConvertContoursToPoints.cc:3:
    /mxe/usr/x86_64-w64-mingw32.static/include/asio/detail/config.hpp:996:5: warning: #warning Please define _WIN32_WINNT or _WIN32_WINDOWS appropriately. [-Wcpp]
      996 | #   warning Please define _WIN32_WINNT or _WIN32_WINDOWS appropriately.
          |     ^~~~~~~
    /mxe/usr/x86_64-w64-mingw32.static/include/asio/detail/config.hpp:997:5: warning: #warning For example, add -D_WIN32_WINNT=0x0601 to the compiler command line. [-Wcpp]
      997 | #   warning For example, add -D_WIN32_WINNT=0x0601 to the compiler command line.
          |     ^~~~~~~
    /mxe/usr/x86_64-w64-mingw32.static/include/asio/detail/config.hpp:998:5: warning: #warning Assuming _WIN32_WINNT=0x0601 (i.e. Windows 7 target). [-Wcpp]
      998 | #   warning Assuming _WIN32_WINNT=0x0601 (i.e. Windows 7 target).
          |     ^~~~~~~



    mkdir -pv /out/usr/{bin,lib,include,share}

    if [ ! -f /mxe/usr/"${TOOLCHAIN}"/lib/libstdc++fs.a ] ; then
        cp /mxe/usr/"${TOOLCHAIN}"/lib/{libm.a,libstdc++fs.a}
    fi
    if [ ! -f /mxe/usr/"${TOOLCHAIN}"/lib/libstdc++fs.so ] ; then
        cp /mxe/usr/"${TOOLCHAIN}"/lib/{libm.so,libstdc++fs.so}
    fi

    # Confirm the search locations reflect the toolchain prefix.
    /mxe/usr/x86_64-pc-linux-gnu/bin/"${TOOLCHAIN}-g++" -print-search-dirs

    ( mkdir -pv /asio &&
      cd /asio &&
      wget 'https://sourceforge.net/projects/asio/files/latest/download' -O asio.tgz &&
      ( tar -axf asio.tgz || unzip asio.tgz ) &&
      cd asio-*/ &&
      cp -v -R include/asio/ include/asio.hpp /mxe/usr/"${TOOLCHAIN}"/include/  )

    # Note: insert *all* of the following into the target link commands involving SFML.
    #x86_64-w64-mingw32.static-pkg-config sfml --libs

    #for repo_dir in /ygor /ygorclustering /explicator /dcma ; do
    #for repo_dir in /ygor /ygorclustering /explicator ; do
    #for repo_dir in /explicator ; do
    #for repo_dir in /ygor ; do
    for repo_dir in /dcma ; do
        cd "$repo_dir"

        # Note: might have to remove the std::filesystem gcc8 linking (-lstdc++fs) workaround here before proceeding.
        # Note: add the following to the top-level CMakeLists.txt somewhere convenient (near the top seems best).
        # (This is required because something seems to be eating the CXXFLAGS I pass in via an environment variable.)
        # include_directories("/mxe/usr/x86_64-w64-mingw32.shared/usr/include")
        # link_directories("/mxe/usr/x86_64-w64-mingw32.shared/usr/libs" "/mxe/usr/x86_64-w64-mingw32.shared/usr/bin")

        # Adjust CMake settings to build static binaries.
        # set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
        # set(BUILD_SHARED_LIBS OFF)
        # set(CMAKE_EXE_LINKER_FLAGS "-static")

        rm -rf build/
        mkdir build
        cd build 
        #"${TOOLCHAIN}-cmake" -E env CXXFLAGS='-I/out/include' \
        #"${TOOLCHAIN}-cmake" -E env LDFLAGS="-L/out/lib" \
        #"${TOOLCHAIN}-cmake" \
        #  -DCMAKE_CXX_FLAGS='-I/out/include' \

        #export SFML_ROOT="/mxe/usr/${TOOLCHAIN}/"
        #export SFML_ROOT="/mxe/usr/x86_64-w64-mingw32.static/lib/cmake/SFML/"
        "${TOOLCHAIN}-cmake" \
          -DCMAKE_INSTALL_PREFIX=/usr/ \
          -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_ALL_STATIC=ON \
          -DWITH_LINUX_SYS=OFF \
          -DWITH_EIGEN=ON \
          -DWITH_CGAL=OFF \
          -DWITH_NLOPT=OFF \
          -DWITH_SFML=ON \
          -DSFML_STATIC_LIBRARIES=TRUE \
          -DWITH_WT=OFF \
          -DWITH_GNU_GSL=OFF \
          -DWITH_POSTGRES=OFF \
          -DWITH_JANSSON=OFF \
          -DBUILD_SHARED_LIBS=OFF \
          -DCMAKE_EXE_LINKER_FLAGS='-static' \
          ../
        make --ignore-errors VERBOSE=1 2>&1 | tee ../build_log 
        make install DESTDIR="/mxe/usr/${TOOLCHAIN}/"
        make install DESTDIR="/out" # Also install here, for easier access later.
        rsync -avP /out/usr/ "/mxe/usr/${TOOLCHAIN}/"
    done





    #-DCMAKE_FIND_LIBRARY_PREFIXES='lib;' \
    #-DCMAKE_FIND_LIBRARY_SUFFIXES='.dll;.a;.dll.a;.lib' \

    # For older gccs the following might work (unlikely, especially std::filesystem, but maybe...)
    #find ./ -type f -iname '*.h' -exec sed -i -e 's/[<]optional/\<experimental\/optional/g' -e 's/std::optional/std::experimental::optional/g' '{}' \+
    #find ./ -type f -iname '*.cc' -exec sed -i -e 's/[<]optional/\<experimental\/optional/g' -e 's/std::optional/std::experimental::optional/g' '{}' \+
    #find ./ -type f -iname '*.h' -exec sed -i -e 's/[<]any/\<experimental\/any/g' -e 's/std::any/std::experimental::any/g' '{}' \+
    #find ./ -type f -iname '*.cc' -exec sed -i -e 's/[<]any/\<experimental\/any/g' -e 's/std::any/std::experimental::any/g' '{}' \+
    #find ./ -type f -iname '*.h' -exec sed -i -e 's/[<]filesystem/\<experimental\/filesystem/g' -e 's/std::filesystem/std::experimental::filesystem/g' '{}' \+
    #find ./ -type f -iname '*.cc' -exec sed -i -e 's/[<]filesystem/\<experimental\/filesystem/g' -e 's/std::filesystem/std::experimental::filesystem/g' '{}' \+




Here is where the attempt stopped. There were lots of `POSIX` calls that caused compilation issues which will need to be
ported before I can continue. See the error logs. Note that they are likely incomplete!

