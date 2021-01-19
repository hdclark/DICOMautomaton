
self: nixpkgs: {
      explicator      = nixpkgs.callPackage ./explicator_derivation.nix { };
      ygorclustering  = nixpkgs.callPackage ./ygorclustering_derivation.nix { };
      ygor            = nixpkgs.callPackage ./ygor_derivation.nix { };
      dicomautomaton  = nixpkgs.callPackage ./dicomautomaton_derivation.nix { };
}

