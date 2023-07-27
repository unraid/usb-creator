# Copyright (c) 2015-2016 Pololu Corporation - http://www.pololu.com/
# Copyright (C) 2023 Lime Technology, Inc

source $setup

cmake-cross $src \
  -DCMAKE_INSTALL_PREFIX=$out

cat CMakeFiles/Unraid.USB.Creator.dir/link.txt
cat CMakeFiles/Unraid.USB.Creator.dir/linkLibs.rsp
make
make install

$host-strip $out/bin/*
