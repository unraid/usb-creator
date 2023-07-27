# Copyright (c) 2015-2016 Pololu Corporation - http://www.pololu.com/
# Copyright (C) 2023 Lime Technology, Inc

rec {

  nixcrpkgs = import <nixcrpkgs> {};

  src = nixcrpkgs.filter ./.;

  pkgs = import <nixpkgs> {};
  src_i18n_compiled = derivation {
    name = "src-i18n";
    builder = "${pkgs.bash}/bin/bash";
    args = [ ./nix/build_translations.sh ];
    baseInputs = with pkgs; [ coreutils qt6.qttools ];
    src = src;
    system = builtins.currentSystem;
  };

  build = (import ./nix/build.nix) src_i18n_compiled;

  win32 = build "win" nixcrpkgs.win32;
}
