TEMPLATE = lib
CONFIG += qt warn_on
QT -= gui
DEPENDPATH += .
INCLUDEPATH += .

DEFINES += QUAZIP_BUILD
CONFIG(staticlib): DEFINES += QUAZIP_STATIC

win32 {
    ZLIBDIR = $$[QT_INSTALL_PREFIX]/../Src/qtbase/src/3rdparty/zlib
}
INCLUDEPATH += $${ZLIBDIR}

# Input
HEADERS += \
    crypt.h\
    ioapi.h\
    JlCompress.h\
    quaadler32.h\
    quachecksum32.h\
    quacrc32.h\
    quazip.h\
    quazipfile.h\
    quazipfileinfo.h\
    quazipnewinfo.h\
    quazip_global.h\
    unzip.h\
    zip.h\

SOURCES += *.c *.cpp

unix:!symbian {
    headers.path=$$PREFIX/include/quazip
    headers.files=$$HEADERS
    target.path=$$PREFIX/lib
    INSTALLS += headers target

	OBJECTS_DIR=.obj
	MOC_DIR=.moc
	
}

win32 {
    headers.path=$$PREFIX/include/quazip
    headers.files=$$HEADERS
    target.path=$$PREFIX/lib
    INSTALLS += headers target

}

win32 {
    !contains(QMAKE_TARGET.arch, x86_64) {
        CONFIG(release, debug|release) {
            DESTDIR = $${_PRO_FILE_PWD_}/../build/win32
        }
        else:CONFIG(debug, debug|release){
            DESTDIR = $${_PRO_FILE_PWD_}/../build/win32d
        }
    } else {
        CONFIG(release, debug|release) {
            DESTDIR = $${_PRO_FILE_PWD_}/../build/win64
        }
        else:CONFIG(debug, debug|release){
            DESTDIR = $${_PRO_FILE_PWD_}/../build/win64d
        }
    }
} else {
    CONFIG += unversioned_libname
    CONFIG(release, debug|release) {
        DESTDIR = $${_PRO_FILE_PWD_}/../build/unix
    }
    else:CONFIG(debug, debug|release){
        DESTDIR = $${_PRO_FILE_PWD_}/../build/unixd
    }
}
