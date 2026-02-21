# Maintainer: Hal Clark <gmail.com[at]hdeanclark>
pkgname=dicomautomaton
pkgver=20260215_083412
pkgver() {
  date +%Y%m%d_%H%M%S
}
pkgrel=1

pkgdesc="Various tools for medical physics applications."
url="https://www.halclark.ca"
arch=('x86_64' 'i686' 'armv7h')
license=('unknown')

# Build the dependencies dynamically from the get_packages() script.
if [ ! -n "$REPOROOT" ] ; then
    printf 'REPOROOT environment variable not set. It is needed to dynamically generate the package dependency list.\n' 2>&1
    exit 1
fi
if [ ! -d "${REPOROOT}" ] ; then
    printf 'REPOROOT ("%s") is not accessible. It is needed to dynamically generate the package dependency list.\n' "${REPOROOT}" 2>&1
    exit 1
fi
printf 'Note: REPOROOT set to "%s"\n' "${REPOROOT}"

depends=()
while IFS= read -r line; do
    depends+=( "$line" )
done < <( "${REPOROOT}"/scripts/get_packages.sh --os arch --tier ygor_deps --tier dcma_deps --required-only --newline )
depends+=( 'ygor' )
depends+=( 'explicator' )

optdepends=()
while IFS= read -r line; do
    optdepends+=( "$line" )
done < <( "${REPOROOT}"/scripts/get_packages.sh --os arch --tier ygor_deps --tier dcma_deps --optional-only --newline )

makedepends=()
while IFS= read -r line; do
    makedepends+=( "$line" )
done < <( "${REPOROOT}"/scripts/get_packages.sh --os arch --tier build_tools --newline )
makedepends+=( 'ygorclustering' )

# conflicts=()
# replaces=()
# backup=()
# install='foo.install'
# source=("http://www.server.tld/${pkgname}-${pkgver}.tar.gz"
#         "foo.desktop")
# md5sums=(''
#          '')

#options=(!strip staticlibs)
#PKGEXT='.pkg.tar' # Disable compression.
options=(strip staticlibs !debug !lto)

build() {
  # ---------------- Configure -------------------
  # Try use environment variable, but fallback to standard. 
  install_prefix=${INSTALL_PREFIX:-/usr}

  # If the version is not supplied as an environment variable, attempt to provide it.
  # (If this is being built it an intact git repo, it should pick up the git hash.)
  if [ -z "${DCMA_VERSION}" ] ; then
      DCMA_VERSION="$(../scripts/extract_dcma_version.sh)"
  fi

  # Work-around for SFML 2.6.0-1.
  if [ -d '/usr/pkgconfig/' ] ; then
      export PKG_CONFIG_PATH='/usr/pkgconfig/'
  fi

  # Default build with default compiler flags.
  cmake \
    -DDCMA_VERSION="${DCMA_VERSION}" \
    -DCMAKE_INSTALL_PREFIX="${install_prefix}" \
    -DCMAKE_INSTALL_SYSCONFDIR=/etc \
    -DCMAKE_BUILD_TYPE=Release \
    -DMEMORY_CONSTRAINED_BUILD=OFF \
    -DWITH_ASAN=OFF \
    -DWITH_TSAN=OFF \
    -DWITH_MSAN=OFF \
    -DWITH_EIGEN=ON \
    -DWITH_CGAL=ON \
    -DWITH_NLOPT=ON \
    -DWITH_SFML=OFF \
    -DWITH_WT=ON \
    -DWITH_GNU_GSL=ON \
    -DWITH_POSTGRES=OFF \
    -DWITH_JANSSON=ON \
    -DAdaptiveCpp_DIR="/usr/include/AdaptiveCPP/" \
    \
    ../

  ## Use custom toolchain.
  #cmake \
  #  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/clang.cmake \
  #  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/gnu-gcc.cmake \
  #  -DCMAKE_INSTALL_PREFIX=/usr \
  #  ../

  ## Debug build with default compiler flags.
  #cmake \
  #  -DMEMORY_CONSTRAINED_BUILD=OFF \
  #  -DCMAKE_INSTALL_PREFIX=/usr \
  #  -DCMAKE_BUILD_TYPE=Debug \
  #  ../

  ## Custom build which can make use of custom compiler flags.
  ## (Note that the CMake build type must NOT be specified, or these flags will be overridden.)
  #cmake \
  #  -DMEMORY_CONSTRAINED_BUILD=OFF \
  #  -DCMAKE_INSTALL_PREFIX=/usr \
  #  -DCMAKE_CXX_FLAGS='-O2' \
  #  ../

  # ------------------ Build ---------------------
  # Scale compilation, but limit to 8 concurrent jobs to temper memory usage.
  JOBS=$(nproc)
  JOBS=$(( $JOBS < 8 ? $JOBS : 8 ))
  make -j "$JOBS" VERBOSE=1

  ## Build with a single job to keep memory usage low.
  #make VERBOSE=1

  ## Debug compiler flags without actually compiling ("dry-run").
  #make -n

}

package() {
  make DESTDIR="${pkgdir}" install
}

# vim:set ts=2 sw=2 et:
