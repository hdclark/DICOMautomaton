
let
  #nixpkgs = ???.fetchFromGithub {
  #  owner  = "hdclark";
  #  repo   = "ygor";                                                                      

  nixpkgs = builtins.fetchTarball {
    #rev    = "96174462da92efff84e8077264fbc5102c8df0eb";
    #url    = "https://github.com/hdclark/nixpkgs/archive/${rev}.tar.gz";
    url    = "https://github.com/hdclark/nixpkgs/archive/96174462da92efff84e8077264fbc5102c8df0eb.tar.gz";
    sha256 = "1yaaz6rnmzly7qv37jac3mpb1bwxwxsknj6ppsm3h6d8dnsjv90j";
  };
in nixpkgs

