
{ nixpkgs ? (import ./nixpkgs.nix), ... } :
let
  pkgs = import nixpkgs {config={};};
  pkgs-arm = import nixpkgs {system="armv7l-linux"; config={}; };

  nixBuildWrapper = drv : extraAttrs: drv.overrideAttrs (old:{
    initPhase = ''
      #mkdir -p $out/nix-support
      #echo "$system" > $out/nix-support/system
      echo "$system"
    '';
  }//extraAttrs);

  jobs = rec {
    nix-build = { system ? builtins.currentSystem }:
                  nixBuildWrapper (pkgs.callPackage ./derivation.nix {}) {};

    nix-build-clang = { system ? builtins.currentSystem }:
                        nixBuildWrapper (pkgs.callPackage ./derivation.nix {
                          stdenv = pkgs.clangStdenv;
                        }) {};

    nix-build-arm = { system ? builtins.currentSystem }:
                    with pkgs-arm;
                    (nixBuildWrapper (pkgs-arm.callPackage ./derivation.nix {
                      stdenv = pkgs-arm.stdenv;
                      system = "armv7l-linux";
                    }) {});
  };
in
  jobs
