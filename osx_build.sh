#!/bin/sh
################################################################################
#      This file is part of Unraid USB Creator - https://github.com/limetech/usb-creator
#      Copyright (C) 2016 Team LibreELEC
#      Copyright (C) 2019 Lime Technology, Inc
#
#  Unraid USB Creator is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 2 of the License, or
#  (at your option) any later version.
#
#  Unraid USB Creator is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with Unraid USB Creator.  If not, see <http://www.gnu.org/licenses/>.
################################################################################

set -e

echo ""
echo "Start building..."

USER=$(whoami)

chmod -R 755 dmg_osx

echo ""
echo "Creating .ts files"
for f in lang/*.po; do 
  if [ -f "$f" ]
  then
    echo "Processing $f file..";
    lconvert -verbose -locations none "$f" -o "lang/$(basename $f .po).ts"
  fi
done

echo ""
echo "Creating .qm files"
/Users/$USER/Qt/6.2.4-static/bin/lrelease creator.pro

echo ""
echo "Running qmake..."
/Users/$USER/Qt/6.2.4-static/bin/qmake

echo ""
echo "Building..."
make -j$(sysctl -n hw.ncpu)

# to decompile
#    osadecompile main.scpt >main.scpt.txt
echo ""
echo "Running osacompile..."
mkdir -p dmg_osx/template.app/Contents/Resources/Scripts
osacompile -t osas -o dmg_osx/template.app/Contents/Resources/Scripts/main.scpt dmg_osx/main.scpt.txt

echo ""
echo "Copying template files over..."
cp -r dmg_osx/template.app/* "Unraid USB Creator.app"

echo ""
echo "  To run application directly type"
echo "    sudo \"Unraid USB Creator.app/Contents/MacOS/Unraid USB Creator\""
echo ""
echo "  Next step would be to codesign the application by running osx_sign.sh"
echo "  and then run osx_make_dmg.sh script to create final .dmg file..."
echo ""
echo "Finished..."
echo ""

exit 0
