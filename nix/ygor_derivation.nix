
{ stdenv
, fetchFromGitHub
, buildStatic ? false
, pkgs

, cmake
, pkg-config
, boost
, gsl
, eigen
#, gnuplot
, ... }:

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

  nativeBuildInputs = []
    ++ [ cmake ]
    ++ [ pkg-config ]
    ++ pkgs.lib.optionals (buildStatic && pkgs.glibc != null) [ pkgs.glibc.static ] ;

  buildInputs = []
    ++ pkgs.lib.optionals (eigen != null) [ eigen ]
    ++ pkgs.lib.optionals (boost != null) [ boost ]
#    ++ gnuplot  # Runtime dependency that makes exotic compilation significantly harder.
    ++ pkgs.lib.optionals (gsl != null) 
           [( if (buildStatic) then pkgs.pkgsStatic.gsl
                               else gsl )] ;

  cmakeFlags = []
    ++ [ "-DCMAKE_BUILD_TYPE=Release" ]
    ++ [( if (eigen == null) then "-DWITH_EIGEN=OFF"
                             else "-DWITH_EIGEN=ON" )]
    ++ [( if (boost == null) then "-DWITH_BOOST=OFF"
                             else "-DWITH_BOOST=ON" )]
    ++ [( if (gsl   == null) then "-DWITH_GNU_GSL=OFF"
                             else "-DWITH_GNU_GSL=ON" )]
    ++ [( if (buildStatic)   then "-DBUILD_SHARED_LIBS=OFF"
                             else "-DBUILD_SHARED_LIBS=ON" )]
    ++ [( if (buildStatic || !stdenv.hostPlatform.isLinux) then "-DWITH_LINUX_SYS=OFF"
                                                           else "-DWITH_LINUX_SYS=ON" )] ;

  enableParallelBuilding = true;

  meta = with stdenv.lib; {
    homepage        = "https://www.halclark.ca";
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

