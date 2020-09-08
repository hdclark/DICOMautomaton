
{ nixpkgs ? (import <nixpkgs> {})

# Note: see nixpkgs/lib/systems/examples for standard cross-compiler definitions.
, toolchainStatic  ? false
, toolchainMusl    ? false
, toolchainMingw64 ? false
, toolchainWasi    ? false
#, toolchainClang   ? false
}:

let pkgs = # Compile with musl-based toolchain.
           if toolchainMusl then 
               nixpkgs.pkgsMusl
               #nixpkgs.pkgsCross.musl64

           # Generate and use static libraries and binaries.
           else if toolchainStatic then
               nixpkgs.pkgsStatic

           # Use Mingw64 toolchain to cross-compile.
           else if toolchainMingw64 then
               nixpkgs.pkgsCross.mingwW64

           # Use WASI toolchain to cross-compile.
           else if toolchainWasi then
               nixpkgs.pkgsCross.wasi32

           # Use the default toolchain.
           else 
               nixpkgs ;

#  stdenv = if toolchainClang then
#               pkgs.clangStdenv
#           else
#               pkgs.stdenv ;

#in pkgs.callPackage( (import ./testcompile_derivation.nix) ){ 
in pkgs.callPackage( (import ./dicomautomaton_derivation.nix) ){ 
  explicator     = pkgs.callPackage ./explicator_derivation.nix { };
  ygorclustering = pkgs.callPackage ./ygorclustering_derivation.nix { };
  ygor           = pkgs.callPackage ./ygor_derivation.nix { };
  #dicomautomaton = pkgs.callPackage ./dicomautomaton_derivation.nix { };
}


