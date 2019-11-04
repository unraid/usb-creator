# Unraid USB Flash Creator
USB creator App for Unraid

An easy to use application to create bootable USB flash for Unraid. Runs on Windows and macOS operating systems.  Linux support coming soon.

### Building for Windows
1. Install ActivePerl, NASM, and Visual Studio 2017

2. Download and extract https://www.openssl.org/source/openssl-1.1.1d.tar.gz to C:\openssl-src-32

3. Download and extract http://download.qt.io/official_releases/qt/5.13/5.13.1/submodules/qtbase-everywhere-src-5.13.1.zip to %USERPROFILE%\Downloads

4. Download and extract http://download.qt.io/official_releases/qt/5.13/5.13.1/submodules/qttools-everywhere-src-5.13.1.zip to %USERPROFILE%\Downloads

5. Download and extract http://download.qt.io/official_releases/jom/jom_1_1_3.zip to %USERPROFILE%\Downloads\qtbase-everywhere-src-5.13.1

6. Open 'x86 Native Tools Command Prompt for VS 2017' app and type the following in the console:
```
%LOCALAPPDATA%\bin\NASM\nasmpath.bat
cd C:\openssl-src-32
perl Configure VC-WIN32 no-shared --prefix=C:\Build-OpenSSL-VC32-Release-Static
nmake
nmake test
nmake install

cd %USERPROFILE%\Downloads\qtbase-everywhere-src-5.13.1

SET OPENSSL_LIBS='-LC:\Build-OpenSSL-VC32-Release-Static\lib -lssl -lcrypto' 

configure.bat -static -static-runtime -no-shared -release -opensource -confirm-license -silent -platform win32-msvc -prefix C:\Qt\5.13.1-static -nomake examples -nomake tests -no-opengl -qt-zlib -qt-pcre -qt-libpng -qt-libjpeg -qt-harfbuzz -qt-freetype -openssl-linked -IC:\Build-OpenSSL-VC32-Release-Static\include -LC:\Build-OpenSSL-VC32-Release-Static\lib
```

7. Edit _mkspecs/common/msvc-desktop.conf_ and change _-MD_ to _-MT_

8. Back in the 'x86 Native Tools Command Prompt for VS 2017' app, type in:
```
jom -j 8
jom -j 8 install
```

### Building for MacOS

1. Install XCode with Command-line tools

2. Open a command prompt and type the following in the console:
```
mkdir -p ~/Downloads ~/Qt
cd ~/Downloads
wget http://download.qt.io/official_releases/qt/5.13/5.13.1/submodules/qtbase-everywhere-src-5.13.1.tar.xz
wget http://download.qt.io/official_releases/qt/5.13/5.13.1/submodules/qttools-everywhere-src-5.13.1.tar.xz
tar xf qtbase-everywhere-src-5.13.1.tar.xz
tar xf qttools-everywhere-src-5.13.1.tar.xz
cd qtbase-everywhere-src-5.13.1

./configure -static -no-shared -release -opensource -confirm-license -silent -prefix ~/Qt/5.13.1-static -nomake examples -nomake tests -no-strip -no-cups -qt-zlib -qt-pcre -qt-libpng -qt-libjpeg -qt-harfbuzz -qt-freetype

make -j$(sysctl -n hw.ncpu) && make -j1 install

cd ~/Downloads/qttools-everywhere-src-5.13.1

~/Qt/5.13.1-static/bin/qmake

make -j$(sysctl -n hw.ncpu) && make -j1 install
```
