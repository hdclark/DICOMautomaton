
{ stdenv
, fetchFromGitHub
, cmake
, pkg-config
, boost
, ... }:

stdenv.mkDerivation rec {
  pname   = "ygorclustering";
  version = "20200415.1";

  src = fetchGit {
      url = "https://github.com/hdclark/ygorclustering";
  };
#  src = fetchFromGitHub {
#    # Reminder: the sha256 hash can be computed via:
#    #  nix-prefetch-url --unpack "https://github.com/hdclark/ygorclustering/archive/${rev}.tar.gz"
#    #
#    owner  = "hdclark";
#    repo   = "ygorclustering";
#    rev    = "0af477560917aff71a3965d87816a3883db12a8a";
#    sha256 = "0fqjmjvwv9r0d876ahfrgfd5q2dx217azgwiph4i89kwzr7zm2pi";
#  };

  nativeBuildInputs = [ 
    cmake 
    pkg-config 
  ];

  buildInputs = [ 
    boost
  ];

  cmakeFlags = [ "-DCMAKE_BUILD_TYPE=Release" ];

  enableParallelBuilding = true;

  meta = with stdenv.lib; {
    homepage        = "http://halclark.ca";
    license         = stdenv.lib.licenses.gpl3Plus;
    platforms       = stdenv.lib.platforms.linux;
    maintainers     = with stdenv.lib.maintainers; [ halclark ];
    description     = "C++ library for clustering data";
    longDescription = ''
      YgorClustering is a C++ library for clustering data. It uses Boost.Geometry
      R*-trees for fast indexing. The DBSCAN implementation can cluster 20 million 2D
      datum in around an hour, and 20 thousand in seconds.

      At the moment, only (vanilla) DBSCAN is implemented. It is based on the article
      "A Density-Based Algorithm for Discovering Clusters" by Ester, Kriegel, Sander,
      and Xu in 1996. DBSCAN is generally regarded as a solid, reliable clustering
      technique compared with techniques such as k-means (which is, for example,
      unable to cluster concave clusters).

      Other clustering techniques are planned.
    '';
  };
}

