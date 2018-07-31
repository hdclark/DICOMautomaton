# Maintainer: Hal Clark <gmail.com[at]hdeanclark>
pkgname=dicomautomaton
pkgver=20151106_202000
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
   'ttf-computer-modern-fonts'
   'ttf-freefont'
   'zenity'
   'sfml'
   'jansson'
   'libpqxx'
   'postgresql'
   'freeglut'
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
   'zenity'
   'dialog'
   'gnuplot'
)
makedepends=(
   'cmake'
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
options=(strip staticlibs)

build() {
  cmake ../ -DCMAKE_INSTALL_PREFIX=/usr
  make -j 8
}

package() {
  make -j 8 DESTDIR="${pkgdir}" install
}

# vim:set ts=2 sw=2 et:
