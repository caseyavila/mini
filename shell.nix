let pkgs = import <nixpkgs> {
  crossSystem = { config = "aarch64-linux"; };
};
in
(pkgs.mkShell.override { stdenv = pkgs.clangStdenv; }) { }
