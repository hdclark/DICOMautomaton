# Maintainer: Hal Clark <gmail.com[at]hdeanclark>
pkgname=dicomautomaton
pkgver=20151106_202000
pkgver() {
  date +%Y%m%d_%H%M%S
}
pkgrel=1

pkgdesc="Various tools for medical physics applications."
url="http://www.halclark.ca"
arch=('x86_64' 'i686')
license=('unknown')
depends=(
   'gcc-libs'
   'sfml'
   'jansson'
   'libpqxx'
   'postgresql'
   'freeglut'
   'nlopt' 
   'boost-libs'
   'protobuf3'
   'explicator'
   # -lygor -ldemarcator
)
# optdepends=('')
makedepends=('cmake')
# conflicts=()
# replaces=()
# backup=()
# install='foo.install'
#source=("http://www.server.tld/${pkgname}-${pkgver}.tar.gz"
#        "foo.desktop")
#md5sums=('a0afa52d60cea6c0363a2a8cb39a4095'
#         'a0afa52d60cea6c0363a2a8cb39a4095')

#options=(!strip staticlibs)
options=(strip staticlibs)

build() {

  #echo "Build dir: $(pwd)"
  #echo "Src dir: ${srcdir}"
  #exit
  
  # Make in the parent directory. This will spam Dropbox.
  #cd "${srcdir}"
  #mkdir -p ../build/
  #cd       ../build/
  
  # Make in /tmp/. Probably safer, and acts more like packer.
  #mkdir -p /tmp/dicomautomaton_build/build/
  #cd       /tmp/dicomautomaton_build/build/

  # Or, assume $BUILDDIR is set appropriately and do not change directories.

#  cd "${srcdir}"

  cmake "${srcdir}" -DCMAKE_INSTALL_PREFIX=/usr
  make
}

package() {
  #echo "Packacking dir: $(pwd)"
  #echo "pkg dir: ${pkgdir}"
  #echo "Src dir: ${srcdir}"
  #exit

  # Make in the parent directory. This will spam Dropbox.
  #cd "${srcdir}"
  #cd ../build/

  # Make in /tmp/. Probably safer, and acts more like packer.
  #mkdir -p /tmp/dicomautomaton_build/pkg/
  #cd       /tmp/dicomautomaton_build/pkg/

  # Or, assume $BUILDDIR is set appropriately and do not change directories.

  #echo "${pkgdir}"
  #exit

#  cd "${srcdir}"

  # Let CMake handle installation details.
  make -j 4 DESTDIR="${pkgdir}" install
}

# vim:set ts=2 sw=2 et:
