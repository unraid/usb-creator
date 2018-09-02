################################################################################
#      This file is part of Unraid USB Creator - https://github.com/limetech/usb-creator
#      Copyright (C) 2013-2015 RasPlex project
#      Copyright (C) 2016 Team LibreELEC
#      Copyright (C) 2018 Lime Technology, Inc
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

#-------------------------------------------------
#
# Project created by QtCreator 2013-03-14T18:13:26
#
#-------------------------------------------------

QT += core gui network widgets concurrent

TARGET = Unraid.USB.Creator
TEMPLATE = app

SOURCES += main.cpp \
           creator.cpp \
           diskwriter.cpp \
           jsonparser.cpp \
           movingaverage.cpp \
           downloadmanager.cpp \
           libs/quazip/*.c \
           libs/quazip/*.cpp
#libs/sha256crypt/*.c

HEADERS += creator.h \
           customipeditor.h \
           customlineedit.h \
           diskwriter.h \
           jsonparser.h \
           movingaverage.h \
           downloadmanager.h  \
           deviceenumerator.h \
           libs/quazip/*.h
#libs/sha256crypt/*.h

MOC_DIR     = .generated_files
OBJECTS_DIR = .generated_files
RCC_DIR     = .generated_files
UI_DIR      = .

# hide the full g++ command line and print only filename
CONFIG += silent c++11

#TRANSLATIONS += lang/lang-en_GB.ts \
#                lang/lang-bg_BG.ts \
#                lang/lang-cs_CZ.ts \
#                lang/lang-da_DK.ts \
#                lang/lang-de_DE.ts \
#                lang/lang-el_GR.ts \
#                lang/lang-es_ES.ts \
#                lang/lang-fi_FI.ts \
#                lang/lang-fr_FR.ts \
#                lang/lang-hu_HU.ts \
#                lang/lang-it_IT.ts \
#                lang/lang-nl_NL.ts \
#                lang/lang-no_NO.ts \
#                lang/lang-pl_PL.ts \
#                lang/lang-pt_PT.ts \
#                lang/lang-ru_RU.ts \
#                lang/lang-sl_SI.ts \
#                lang/lang-sr_RS.ts \
#                lang/lang-tr_TR.ts \
#                lang/lang-uk_UA.ts \
#                lang/lang-he_IL.ts \
#                lang/lang-hr_HR.ts \
#                lang/lang-sk_SK.ts \
#                lang/lang-sv_SE.ts \
#                lang/lang-ast_ES.ts \
#                lang/lang-fa_IR.ts \
#                lang/lang-lt_LT.ts

static { # everything below takes effect with CONFIG += static
    CONFIG += static
    #CONFIG += staticlib # this is needed if you create a static library, not a static executable
    DEFINES += STATIC
    message("~~~ static build ~~~") # this is for information, that the static build is done
}

win32 {
    # add suffix
    TARGET = Unraid.USB.Creator.Win32

    SOURCES += diskwriter_windows.cpp \
               deviceenumerator_windows.cpp \
               libs/zlib/*.c

    HEADERS += diskwriter_windows.h \
               deviceenumerator_windows.h \
               privileges.h \
               libs/zlib/*.h

    INCLUDEPATH += "libs\zlib"
    #INCLUDEPATH += "libs\sha256crypt"

    CONFIG += rtti
    CONFIG -= embed_manifest_exe

    #QMAKE_LFLAGS = -static -static-libgcc
    #QMAKE_LFLAGS = -static
    RC_FILE = winapp.rc

    LIBS += -lsetupapi
    LIBS += -LC:\Build-OpenSSL-VC32-Release-Static\lib -llibcrypto -llibssl

    # remove possible other optimization flags
    QMAKE_CXXFLAGS_RELEASE -= -O
    QMAKE_CXXFLAGS_RELEASE -= -O1
    QMAKE_CXXFLAGS_RELEASE -= -O2
    QMAKE_CXXFLAGS_RELEASE -= -O3
    # Optimize for size
    QMAKE_CXXFLAGS_RELEASE += -Os


    # write image to local file named 'dummy_image_file'
    # instead to real device (for testing)
    #DEFINES += WINDOWS_DUMMY_WRITE

    # Debugging:
    #CONFIG += console

    DEFINES += QUAZIP_STATIC
}

unix {
    # remove possible other optimization flags
    QMAKE_CFLAGS_RELEASE -= -O
    QMAKE_CFLAGS_RELEASE -= -O1
    QMAKE_CFLAGS_RELEASE -= -O2
    QMAKE_CFLAGS_RELEASE -= -O3
    # Optimize for size
    QMAKE_CFLAGS_RELEASE += -Os

    QMAKE_CXXFLAGS += -fPIC

    contains(QT_ARCH, i386) {
        # link with i386 library manualy unpacked
        QMAKE_LFLAGS += -L$(HOME)/Qt5.6.1/ubuntu-i386-lib/usr/lib/i386-linux-gnu
        QMAKE_RPATHLINKDIR += $$(HOME)/Qt5.6.1/ubuntu-i386-lib/lib/i386-linux-gnu
    }

    SOURCES += diskwriter_unix.cpp \
               privileges_unix.cpp

    HEADERS += diskwriter_unix.h \
               privileges_unix.h

    #INCLUDEPATH += "libs\sha256crypt"

    LIBS += -lz
}

macx {
    # use spaces on macOS
    TARGET = "Unraid USB Creator"

    SOURCES += deviceenumerator_macos.cpp
    HEADERS += deviceenumerator_macos.h

    # Specifies where to find the libraries at runtime
    QMAKE_RPATHDIR += @executable_path/../Frameworks
    QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/
    #QT_CONFIG -= no-pkg-config
    #CONFIG += link_pkgconfig
    # same thing
    #QMAKE_POST_LINK += install_name_tool -add_rpath @executable_path/../Frameworks \"Unraid USB Creator.app/Contents/MacOS/Unraid USB Creator\"
}

linux* {
    # manually add suffix 32/64
    TARGET = Unraid.USB.Creator.Linux-bit.bin

    SOURCES += deviceenumerator_linux.cpp
    HEADERS += deviceenumerator_linux.h

    #QMAKE_CXXFLAGS += -std=c++11

    LIBS += -lblkid
}

FORMS += creator.ui

OTHER_FILES +=

RESOURCES += resources.qrc
