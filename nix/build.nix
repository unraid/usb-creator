# Copyright (c) 2015-2016 Pololu Corporation - http://www.pololu.com/
# Copyright (C) 2023 Lime Technology, Inc

src: config_name: env:

let
  payload = env.make_derivation {
    builder = ./builder.sh;
    inherit src;
    cross_inputs = [ env.zlib env.qt6 env.qt6.qt5compat];
  };

in
  payload // { inherit env; }
