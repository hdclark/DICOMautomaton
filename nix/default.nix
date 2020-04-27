
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


let pkgs = import <nixpkgs> { }; 

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


