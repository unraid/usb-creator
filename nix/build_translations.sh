# Copyright (C) 2023 Lime Technology, Inc

# A bash script to build translation files

# NOTE: We're supposed to use qt_add_translations() in CMakeLists instead of running these commands manually
# but I can't figure out how to get nixcrpkgs to include qttools in the qt_host env
# so we have to build these translations before we compile.

# Pull in baseInputs
set -e
unset PATH
for p in $baseInputs; do
  export PATH=$p/bin${PATH:+:}$PATH
done

pwd

# Pull in source files
cp -r $src/ ./build
chmod u+w -R ./build/lang

cd ./build

# update .ts files
echo Updating ts files
lupdate lang/

# create .qm files out of .ts files
echo Creating qm files
lrelease lang/*.ts

# Emit output
cp -r ./ $out

