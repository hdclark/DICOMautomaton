
let
    # Use the user's nixpkgs.
    #nixpkgs = import <nixpkgs> {
    #nixpkgs = import <nixpkgs-unstable> {

    # Pin nixpkgs version. See https://status.nixos.org for current build statuses.
    nixpkgs = import (builtins.fetchTarball "https://github.com/NixOS/nixpkgs/archive/dd1b7e377f6d77ddee4ab84be11173d3566d6a18.tar.gz") {

    ## Pin nixokgs version, and verify the integrity.
    #nixpkgs = builtins.fetchTarball {
    #  #rev    = "96174462da92efff84e8077264fbc5102c8df0eb";
    #  #url    = "https://github.com/hdclark/nixpkgs/archive/${rev}.tar.gz";
    #  url    = "https://github.com/hdclark/nixpkgs/archive/96174462da92efff84e8077264fbc5102c8df0eb.tar.gz";
    #  sha256 = "1yaaz6rnmzly7qv37jac3mpb1bwxwxsknj6ppsm3h6d8dnsjv90j";
    #} {

        config = import ./config.nix;
        overlays = [(import ./overlays.nix)];
    };
in nixpkgs

