#!/usr/bin/env bash

# Build dev environment on MacOSX 10.15.4 (Catalina).
set -eux

export WORKROOT="${1:-${HOME}/working/}"             # <---- optional user input #1.
export INSTALLROOT="${2:-${HOME}/build_artifacts/}"  # <---- optional user input #2.
mkdir -pv "${INSTALLROOT}"/{include,lib,bin,etc}

export PATH="/usr/local/bin:${PATH}"   # Assumed to contain the homebrew packages.
export CC='/usr/local/bin/gcc-12'
export CXX='/usr/local/bin/g++-12' 
export CXXFLAGS="'-I${INSTALLROOT}/include/' -Wno-deprecated -Wno-array-bounds -Wno-unused-parameter -Wno-unused-variable -Wno-deprecated-copy -Wno-maybe-uninitialized -undefined dynamic_lookup"
export LDFLAGS="'-L${INSTALLROOT}/lib/' -L/usr/local/lib/ -undefined dynamic_lookup"
export JOBS=2

#for repo in Explicator ; do
#for repo in DICOMautomaton ; do
for repo in Explicator Ygor YgorClustering DICOMautomaton ; do
    cd
    rm -rf "${WORKROOT}"/"${repo}"
    mkdir -pv "${WORKROOT}"/"${repo}"
    git clone "https://github.com/hdclark/${repo}" "${WORKROOT}"/"${repo}"

    ( cd "${WORKROOT}"/"${repo}" &&
#      rm -rf build &&
      mkdir -pv build && 
      cd build &&
#        -DCMAKE_INSTALL_PREFIX=/usr/local/ \
#        -DCMAKE_INSTALL_SYSCONFDIR=/usr/local/etc/ \
#        -DCMAKE_MACOSX_RPATH='@executable_path/../lib' \
      cmake \
        -DCMAKE_INSTALL_PREFIX="${INSTALLROOT}/" \
        -DCMAKE_INSTALL_SYSCONFDIR="${INSTALLROOT}/etc/" \
        -DCMAKE_INSTALL_RPATH='@executable_path/../lib' \
        -DCMAKE_SKIP_BUILD_RPATH=True \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DWITH_LINUX_SYS=OFF \
        -DWITH_EIGEN=ON \
        -DWITH_NLOPT=OFF \
        -DWITH_JANSSON=OFF \
        -DWITH_GNU_GSL=OFF \
        -DWITH_WT=OFF \
        -DWITH_SFML=OFF \
        -DWITH_CGAL=OFF \
        -DWITH_POSTGRES=OFF \
        ../ &&
        make -j"${JOBS}" VERBOSE=1 &&
        make install VERBOSE=1
    )

    ( cd "${WORKROOT}"/"${repo}" &&
      git clean -fxd || true &&
      git reset --hard || true
    )
done

