cmake_minimum_required(VERSION 3.16)
project(quazip VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED)

qt_standard_project_setup()

qt_add_library(quazip
    *.c *.cpp
    JlCompress.h
    crypt.h
    ioapi.h
    quaadler32.h
    quachecksum32.h
    quacrc32.h
    quazip.h
    quazip_global.h
    quazipfile.h
    quazipfileinfo.h
    quazipnewinfo.h
    unzip.h
    zip.h
)
target_include_directories(quazip PUBLIC
    .
)

target_compile_definitions(quazip PUBLIC
    QUAZIP_BUILD
)

target_link_libraries(quazip PUBLIC
    Qt::Core
)

if(CONFIG(staticlib))
    target_compile_definitions(quazip PUBLIC
        QUAZIP_STATIC
    )
endif()

install(TARGETS quazip
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
