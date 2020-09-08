
{ nixpkgs ? (import <nixpkgs> {})

# Note: see nixpkgs/lib/systems/examples for standard cross-compiler definitions.
, toolchainStatic  ? false
, toolchainMusl    ? false
, toolchainMingw64 ? false
, toolchainWasi    ? false
#, toolchainClang   ? false
}:

#let
#  #nixpkgs = import <nixpkgs>;
#  nixpkgs = import ./nixpkgs.nix;
#  pkgs = import nixpkgs {
#    config = {};
#    overlays = [
#      (import ./overlay.nix)
#    ];
#  };
#
#in pkgs.ygor

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

# Compile using musl.
#let pkgs = if toolchainMusl then nixpkgs.pkgsMusl else nixpkgs;
#let pkgs = if toolchainMusl then nixpkgs.pkgsCross.musl64 else nixpkgs;

# Compile static libraries and binaries.
#let pkgs = if toolchainStatic then nixpkgs.pkgsStatic else nixpkgs;

# Cross-compile.
#let pkgs = import <nixpkgs> {
#    #crossSystem = (import <nixpkgs/lib>).systems.examples.mingwW64;
#    #crossSystem = (import <nixpkgs/lib>).systems.examples.wasi32;
#    #crossSystem = (import <nixpkgs/lib>).systems.examples.musl64;
#};

#with import <nixpkgs> { };
#let 
#  static = makeStaticBinaries stdenv;
#  dicomautomaton = pkgs.dicomautomaton.override { stdenv = static; };
#  ygor = pkgs.ygor.override { stdenv = static; };

#let pkgs = import <nixpkgs> {
#  config = {
#    packageOverrides = pkgs: {
#      firebird = pkgs.firebirdSuper.override{ firebird = null; };
#    };
#  };
#}; 

#in pkgs.callPackage( (import ./testcompile_derivation.nix) ){ 
in pkgs.callPackage( (import ./dicomautomaton_derivation.nix) ){ 
  explicator     = pkgs.callPackage ./explicator_derivation.nix { };
  ygorclustering = pkgs.callPackage ./ygorclustering_derivation.nix { };
  ygor           = pkgs.callPackage ./ygor_derivation.nix { };
  #dicomautomaton = pkgs.callPackage ./dicomautomaton_derivation.nix { };
}


