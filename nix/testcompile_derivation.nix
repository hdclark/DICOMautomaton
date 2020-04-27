
{ stdenv
, ygor
, ygorclustering
, explicator
, dicomautomaton
, ...
}:

#with import <nixpkgs> {};

stdenv.mkDerivation rec {
  pname = "testcompile";
  version = "2";

  #src = [ ];
  phases = [ "buildPhase"
             "installPhase"
             "fixupPhase" 
  ];

  buildInputs = [ 
    ygor
    ygorclustering
    explicator
    dicomautomaton
  ];

  buildPhase = ''
    echo '#include <YgorMisc.h>'                    >  in.cc
    echo 'int main(int argc, char **argv){'         >> in.cc
    echo '    FUNCINFO("Compilation successful!");' >> in.cc
    echo '    return 0;'                            >> in.cc
    echo '}'                                        >> in.cc

    $CXX -std=c++17 -Wall -Wextra -pedantic -o testcompile in.cc -lygor
  '';

  installPhase = ''
    mkdir -p $out/bin/
    cp testcompile $out/bin/
    echo " "
    $out/bin/testcompile
    echo " "
    dicomautomaton_dispatcher -h
    echo " "
  '';

}

