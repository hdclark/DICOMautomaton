# Maintainer: Hal Clark <gmail.com[at]hdeanclark>
pkgname=dicomautomaton
pkgver=20190713_202000
pkgver() {
  date +%Y%m%d_%H%M%S
}
pkgrel=1

pkgdesc="Various tools for medical physics applications."
url="http://www.halclark.ca"
arch=('x86_64' 'i686' 'armv7h')
license=('unknown')
depends=(
   'gcc-libs'
   'gnu-free-fonts'
   'zenity'
   'sfml'
   'sdl2'
   'glew'
   'glu'
   'jansson'
   'libpqxx'
   'postgresql'
   'nlopt' 
   'gsl'
   'boost-libs'
   'zlib'
   'cgal>=4.8'
   'wt'
   'explicator'
   'ygor'
)
optdepends=(
   'libnotify'
   'dunst' # Or any other notification server compatible with libnotify.
   'zenity'
   'dialog'
   'gnuplot'
   'patchelf'
   'bash-completion'
   'ttf-computer-modern-fonts'
)
makedepends=(
   'cmake'
   'git'
   'asio'
   'ygorclustering'
)
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
options=(strip staticlibs)

build() {
  # ---------------- Configure -------------------
  # Try use environment variable, but fallback to standard. 
  install_prefix=${INSTALL_PREFIX:-/usr}

  # If the version is not supplied as an environment variable, attempt to provide it.
  # (If this is being built it an intact git repo, it should pick up the git hash.)
  if [ -z "${DCMA_VERSION}" ] ; then
      DCMA_VERSION="$(../scripts/extract_dcma_version.sh)"
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
    -DWITH_SFML=ON \
    -DWITH_WT=ON \
    -DWITH_GNU_GSL=ON \
    -DWITH_POSTGRES=ON \
    -DWITH_JANSSON=ON \
    ../

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
