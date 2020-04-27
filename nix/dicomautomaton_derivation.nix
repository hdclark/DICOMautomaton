
{ stdenv
, fetchFromGitHub
# Build inputs.
, cmake
, pkg-config
, asio
, eigen
, ygorclustering
# Runtime inputs.
, sfml
, xorg
, jansson
, libpqxx
, postgresql
, nlopt
, gsl
, boost
, zlib
#, cgal
#, wt
, mpfr
, gmp
, explicator
, ygor
# Optional runtime components.
, freefont_ttf
, dialog
, gnome3
, gnuplot
, patchelf
, bash-completion
, ... }:

stdenv.mkDerivation rec {
  pname   = "dicomautomaton";
  version = "20200415.1";

  src = fetchFromGitHub {
    # Reminder: the sha256 hash can be computed via:
    #  nix-prefetch-url --unpack "https://github.com/hdclark/dicomautomaton/archive/${rev}.tar.gz"
    #
    owner  = "hdclark";
    repo   = "dicomautomaton";
    rev    = "6b5fc3445a99c79f83842228f81e0b9a680bfe96";
    sha256 = "1ac8n8hqby2pkl5a7j2gxxlpn57xcvp3288lk3kh9hngyxrgpz1j";
  };

  nativeBuildInputs = [ 
    cmake 
    pkg-config 
    asio
    eigen
    ygorclustering
  ];

  buildInputs = [
    # Runtime inputs.
    sfml
#    xorg
#    xorg.libX11
#    xorg.xcbutilkeysyms
#    xorg.libxcb
#    xorg.libXcomposite
#    xorg.libXext
#    xorg.libXrender
#    xorg.libXi
#    xorg.libXcursor
#    xorg.libXtst
#    xorg.libXrandr
#    xorg.xcbutilimage
    jansson
    libpqxx
    postgresql
    nlopt
    gsl
    boost
    zlib
#    cgal
#    wt
    mpfr
    gmp
    explicator
    ygor
    # Optional runtime components.
    freefont_ttf
    dialog
    gnome3.zenity
    gnuplot
    patchelf
    bash-completion
  ];

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
    "-DMEMORY_CONSTRAINED_BUILD=OFF"
    "-DWITH_EIGEN=ON"
  "-DWITH_CGAL=OFF"
    "-DWITH_NLOPT=ON"
    "-DWITH_SFML=ON"
  "-DWITH_WT=OFF"
    "-DWITH_GNU_GSL=ON"
    "-DWITH_POSTGRES=ON"
    "-DWITH_JANSSON=ON" 
    #"-DCGAL_CONFIG_LOADED TRUE:bool=true"
    #"-DCGAL_ROOT=${cgal}/cmake/"
    #"-DCGAL_INSTALL_PREFIX=''"
    #"-DCGAL_INSTALL_PREFIX:FILEPATH='/'"
    #"-DCGAL_HEADER_ONLY=ON"
    #"-DCGAL_DIR=${cgal}/lib/cmake/CGAL"
    #"-DCGAL_ROOT=${cgal}/lib/cmake/CGAL"
  ];

  enableParallelBuilding = true;

  meta = with stdenv.lib; {
    homepage        = "http://halclark.ca";
    license         = stdenv.lib.licenses.gpl3Plus;
    platforms       = stdenv.lib.platforms.linux;
    maintainers     = with stdenv.lib.maintainers; [ halclark ];
    description     = "A collection of tools for analyzing medical physics data";
    longDescription = ''
      DICOMautomaton is a collection of tools for analyzing medical physics data,
      specifically dosimetric and medical imaging data in the DICOM format. It has
      become something of a platform that provides a variety of functionality.
      DICOMautomaton is designed for easily developing customized workflows.
    '';
  };
}

