# Unraid USB Flash Creator
USB creator App for Unraid

An easy to use application to create bootable USB flash for Unraid. Runs on Windows and macOS operating systems.  Linux support coming soon.

### Building for Windows
1. Install Visual Studio 2022 Community Edition
  - during install, select only 'Desktop development with C++' under Workloads tab.  
  - default options raised the space required to 6.83 GB

2. Download and extract https://download.qt.io/official_releases/qt/6.2/6.2.4/submodules/qtbase-everywhere-src-6.2.4.zip to %USERPROFILE%\Downloads

3. Download and extract https://download.qt.io/official_releases/qt/6.2/6.2.4/submodules/qt5compat-everywhere-src-6.2.4.zip to %USERPROFILE%\Downloads

4. Download and extract https://download.qt.io/official_releases/qt/6.2/6.2.4/submodules/qttools-everywhere-src-6.2.4.zip to %USERPROFILE%\Downloads

5. Download and extract http://download.qt.io/official_releases/jom/jom_1_1_3.zip to %USERPROFILE%\Downloads\qtbase-everywhere-src-5.13.1

6. Open 'Developer Command Prompt for VS 2022' app and type the following in the console:
```
cd %USERPROFILE%\Downloads\qtbase-everywhere-src-6.2.4

configure.bat -static -static-runtime -no-shared -release -opensource -confirm-license -silent -platform win32-msvc -prefix C:\Qt\6.2.4-static -nomake examples -nomake tests -nomake tools -no-strip -no-cups -no-openvg -no-plugin-manifests -no-dbus -no-directwrite -qt-zlib -qt-pcre -qt-libpng -qt-libjpeg -qt-harfbuzz -qt-freetype -DFEATURE_openssl=OFF -DFEATURE_schannel=ON
```

7. Edit _mkspecs/common/msvc-desktop.conf_ and change _-MD_ to _-MT_

8. Back in the 'Developer Command Prompt for VS 2022' app, type in:
```
cmake --build . --parallel
cmake --install .

cd %USERPROFILE%\Downloads\qt5compat-everywhere-src-6.2.4
C:\Qt\6.2.4-static\bin\qt-configure-module.bat .
cmake --build . --parallel
cmake --install .

cd %USERPROFILE%\Downloads\qttools-everywhere-src-6.2.4
C:\Qt\6.2.4-static\bin\qt-configure-module.bat .
cmake --build . --parallel
cmake --install .
```

### Building for MacOS

1. Install XCode 13+ with Command-line tools and `brew install cmake`

2. Open a command prompt and type the following in the console:
```
mkdir -p ~/Downloads ~/Qt
cd ~/Downloads
wget https://download.qt.io/official_releases/qt/6.2/6.2.4/submodules/qtbase-everywhere-src-6.2.4.tar.xz
wget https://download.qt.io/official_releases/qt/6.2/6.2.4/submodules/qt5compat-everywhere-src-6.2.4.tar.xz
wget https://download.qt.io/official_releases/qt/6.2/6.2.4/submodules/qttools-everywhere-src-6.2.4.tar.xz
tar xf qtbase-everywhere-src-6.2.4.tar.xz
tar xf qt5compat-everywhere-src-6.2.4.tar.xz
tar xf qttools-everywhere-src-6.2.4.tar.xz
cd qtbase-everywhere-src-6.2.4

./configure -static -no-shared -release -opensource -confirm-license -silent -prefix ~/Qt/6.2.4-static -nomake examples -nomake tests -no-strip -no-cups -no-openvg -no-plugin-manifests -no-dbus -no-directwrite -qt-zlib -qt-pcre -qt-libpng -qt-libjpeg -qt-harfbuzz -qt-freetype -- -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"

cmake --build . --parallel
cmake --install .

cd ~/Downloads/qt5compat-everywhere-src-6.2.4
~/Qt/6.2.4-static/bin/qt-configure-module .
cmake --build . --parallel
cmake --install .

cd ~/Downloads/qttools-everywhere-src-6.2.4
~/Qt/6.2.4-static/bin/qt-configure-module .
cmake --build . --parallel
cmake --install .
```
