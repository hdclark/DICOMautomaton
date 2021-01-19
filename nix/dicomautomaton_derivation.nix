
{ stdenv
, fetchFromGitHub
, buildStatic ? false
, pkgs

# Build inputs.
, cmake
, pkg-config
, asio
, eigen
, ygorclustering

# Runtime inputs.
, sfml
, SDL2
, glew
, xorg
, jansson
, libpqxx
, postgresql
, nlopt
, gsl
, boost
, zlib
, cgal_5
, wt4
, mpfr
, gmp
, explicator
, ygor

# Optional runtime components.
, freefont_ttf
, dialog
#, gnome3
#, gnuplot
, patchelf
, bash-completion
, ... }:

stdenv.mkDerivation rec {
  pname   = "dicomautomaton";
  version = "20200415.1";

  src = fetchGit {
      url = "https://github.com/hdclark/dicomautomaton";
  };
  #src = fetchFromGitHub {
  #  # Reminder: the sha256 hash can be computed via:
  #  #  nix-prefetch-url --unpack "https://github.com/hdclark/dicomautomaton/archive/${rev}.tar.gz"
  #  #
  #  owner  = "hdclark";
  #  repo   = "dicomautomaton";
  #  rev    = "6b5fc3445a99c79f83842228f81e0b9a680bfe96";
  #  sha256 = "1ac8n8hqby2pkl5a7j2gxxlpn57xcvp3288lk3kh9hngyxrgpz1j";
  #};

  # Selectively disable specific functionality.
  #eigen      = null;
  #cgal_5     = null;
  #nlopt      = null;
  #sfml       = null;
  #SDL2       = null;
  #glew       = null;
  #wt4        = null;
  #gsl        = null;
  #libpqxx    = null;
  #postgresql = null;
  #jansson    = null;

  nativeBuildInputs = []
    ++ [ cmake ]
    ++ [ pkg-config ]
    ++ pkgs.lib.optionals (buildStatic && pkgs.glibc != null) [ pkgs.glibc.static ] ;

  buildInputs = []
    ++ [ ygorclustering ]
    ++ [ explicator ]
    ++ [ ygor ]
    ++ pkgs.lib.optionals (asio != null) [ asio ]
    ++ pkgs.lib.optionals (eigen != null) [ eigen ]
    ++ pkgs.lib.optionals (sfml != null) [ sfml ]
    ++ pkgs.lib.optionals (SDL2 != null) [ SDL2 ]
    ++ pkgs.lib.optionals (glew != null) [ glew ]
    ++ pkgs.lib.optionals (jansson != null) [ jansson ]
    ++ pkgs.lib.optionals (postgresql != null) [ postgresql ]  # Needed for libpq.
    ++ pkgs.lib.optionals (nlopt != null) [ nlopt ]
    ++ pkgs.lib.optionals (zlib != null) [ zlib ]
    ++ pkgs.lib.optionals (cgal_5 != null) [ cgal_5 ]
    ++ pkgs.lib.optionals (wt4 != null) [ wt4 ]
    ++ pkgs.lib.optionals (mpfr != null) [ mpfr ]
    ++ pkgs.lib.optionals (gmp != null) [ gmp ]
    ++ pkgs.lib.optionals (gsl != null) [ gsl ]
    ++ pkgs.lib.optionals (libpqxx != null) [ libpqxx ]
    ++ pkgs.lib.optionals (boost != null) [ boost ]

    #++ pkgs.lib.optionals (xorg != null) [ xorg ]
    #++ pkgs.lib.optionals (xorg.libX11 != null) [ xorg.libX11 ]
    #++ pkgs.lib.optionals (xorg.xcbutilkeysyms != null) [ xorg.xcbutilkeysyms ]
    #++ pkgs.lib.optionals (xorg.libxcb != null) [ xorg.libxcb ]
    #++ pkgs.lib.optionals (xorg.libXcomposite != null) [ xorg.libXcomposite ]
    #++ pkgs.lib.optionals (xorg.libXext != null) [ xorg.libXext ]
    #++ pkgs.lib.optionals (xorg.libXrender != null) [ xorg.libXrender ]
    #++ pkgs.lib.optionals (xorg.libXi != null) [ xorg.libXi ]
    #++ pkgs.lib.optionals (xorg.libXcursor != null) [ xorg.libXcursor ]
    #++ pkgs.lib.optionals (xorg.libXtst != null) [ xorg.libXtst ]
    #++ pkgs.lib.optionals (xorg.libXrandr != null) [ xorg.libXrandr ]
    #++ pkgs.lib.optionals (xorg.xcbutilimage != null) [ xorg.xcbutilimage ]

    # Optional runtime components.
    ++ pkgs.lib.optionals (freefont_ttf != null) [ freefont_ttf ]
    ++ pkgs.lib.optionals (dialog != null) [ dialog ]
    #++ pkgs.lib.optionals (gnome3.zenity != null) [ gnome3.zenity ]
    #++ pkgs.lib.optionals (gnuplot != null) [ gnuplot ]
    ++ pkgs.lib.optionals (patchelf != null) [ patchelf ]
    ++ pkgs.lib.optionals (bash-completion != null) [ bash-completion ]
  ;

  cmakeFlags = []
    ++ [ "-DCMAKE_BUILD_TYPE=Release" ]
    ++ [ "-DMEMORY_CONSTRAINED_BUILD=OFF" ]
    ++ [ "-DWITH_ASAN=OFF" ]
    ++ [ "-DWITH_TSAN=OFF" ]
    ++ [ "-DWITH_MSAN=OFF" ]
    ++ [( if (eigen == null)   then "-DWITH_EIGEN=OFF"
                               else "-DWITH_EIGEN=ON" )]
    ++ [( if (cgal_5 == null)  then "-DWITH_CGAL=OFF"
                               else "-DWITH_CGAL=ON" )]
    ++ [( if (nlopt == null)   then "-DWITH_NLOPT=OFF"
                               else "-DWITH_NLOPT=ON" )]
    ++ [( if (sfml == null)    then "-DWITH_SFML=OFF"
                               else "-DWITH_SFML=ON" )]
    ++ [( if (SDL2 == null)    then "-DWITH_SDL=OFF"
                               else "-DWITH_SDL=ON" )]
    ++ [( if (jansson == null) then "-DWITH_JANSSON=OFF"
                               else "-DWITH_JANSSON=ON" )]
    ++ [( if (wt4 == null)     then "-DWITH_WT=OFF"
                               else "-DWITH_WT=ON" )]
    ++ [( if (gsl == null)     then "-DWITH_GNU_GSL=OFF"
                               else "-DWITH_GNU_GSL=ON" )]
    ++ [( if (buildStatic)     then "-DBUILD_SHARED_LIBS=OFF"
                               else "-DBUILD_SHARED_LIBS=ON" )]
    ++ [( if (postgresql == null || libpqxx == null) then "-DWITH_POSTGRES=OFF"
                                                     else "-DWITH_POSTGRES=ON" )] ;

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

