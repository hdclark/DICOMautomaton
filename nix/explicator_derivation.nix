
{ stdenv
, fetchFromGitHub
, buildStatic ? false
, pkgs

, cmake
, ... }:


stdenv.mkDerivation rec {
  pname   = "explicator";
  version = "20200415.1";

  src = fetchGit {
    url = "https://github.com/hdclark/explicator";
  };
#  src = fetchFromGitHub {
#    # Reminder: the sha256 hash can be computed via:
#    #  nix-prefetch-url --unpack "https://github.com/hdclark/explicator/archive/${rev}.tar.gz"
#    #
#    owner  = "hdclark";
#    repo   = "explicator";
#    rev    = "01f1dc2182cbe9285d5198112ed38258c00c25d4";
#    sha256 = "0a8p0hc07bw1ihk6bd4ymaj2m377982nrydxk40ynl3kx3zdnkmm";
#  };

  nativeBuildInputs = []
    ++ [ cmake ]
    ++ pkgs.lib.optionals (buildStatic && pkgs.glibc != null) [ pkgs.glibc.static ] ;

  cmakeFlags = []
    ++ [ "-DCMAKE_BUILD_TYPE=Release" ]
    ++ [( if (buildStatic)   then "-DBUILD_SHARED_LIBS=OFF"
                             else "-DBUILD_SHARED_LIBS=ON" )] ;

  enableParallelBuilding = true;

  meta = with stdenv.lib; {
    homepage    = "http://halclark.ca";
    license     = stdenv.lib.licenses.gpl3Plus;
    platforms   = stdenv.lib.platforms.linux;
    maintainers = with stdenv.lib.maintainers; [ halclark ];
    description = "String translation library using a combination of string similarity metrics.";
  };
}

