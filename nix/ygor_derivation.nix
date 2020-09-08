
{ stdenv
, fetchFromGitHub
, cmake
, pkg-config
, boost
, gsl
, eigen
, gnuplot
, ... }:

#{ stdenv, lib, fetchFromGitHub, cmake, pkgconfig
#, boost, eigen, expat, glew, 
#, nlopt, xorg,
#, cgal_5, gmp, mpfr
#}:

#with import <nixpkgs> {};

stdenv.mkDerivation rec {
  pname   = "ygor";
  version = "20200415.1";

  src = fetchGit {
    url = "https://github.com/hdclark/ygor";
  };
#  src = fetchFromGitHub {
#    # Reminder: the sha256 hash can be computed via:
#    #  nix-prefetch-url --unpack "https://github.com/hdclark/ygor/archive/${rev}.tar.gz"
#    #
#    owner  = "hdclark";
#    repo   = "ygor";
#    rev    = "7ff09b7ea7c193b29b1e24542babe3c6b40e369d";
#    sha256 = "14fljbzgh7zkviz16xdjfkm8jm0qf60sy03d91pfhwil3p1l60lp";
#  };

  nativeBuildInputs = [ 
    cmake 
    pkg-config 
  ];

  buildInputs = [ 
    boost
    gsl
    eigen
    gnuplot
  ];

  cmakeFlags = [ "-DCMAKE_BUILD_TYPE=Release"
                 "-DWITH_LINUX_SYS=ON"
                 "-DWITH_EIGEN=ON"
                 "-DWITH_GNU_GSL=ON"
                 "-DWITH_BOOST=ON" ];

  enableParallelBuilding = true;

  meta = with stdenv.lib; {
    homepage        = "http://halclark.ca";
    license         = stdenv.lib.licenses.gpl3Plus;
    platforms       = stdenv.lib.platforms.linux;
    maintainers     = with stdenv.lib.maintainers; [ halclark ];
    description     = "Support library with scientific emphasis";
    longDescription = ''
      Ygor was written to factor common code amongst a handful of disparate projects.

      Most, but not all of Ygor's routines are focused on scientific or mathematic
      applications. These routines will grow, be replaced, be updated, and may even
      disappear when their functionality is superceded by new features in the
      language/better libraries/etc. However, many of these routines are not broadly
      useful enough for a project like Boost to include, and many are not
      comprehensive enough to be submitted to more mature projects. The routines in
      this library were all developed for specific projects with specific needs, but
      which may (have) become useful for other projects.
    '';
  };
}

