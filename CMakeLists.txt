cmake_minimum_required(VERSION 3.16)
project(Unraid.USB.Creator VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Concurrent Core5Compat Gui Network Widgets)

qt_standard_project_setup()

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXE_LINKER_FLAGS "-static")


FILE(GLOB QuazipCSources libs/quazip/*.c)
FILE(GLOB QuazipCPPSources libs/quazip/*.cpp)
FILE(GLOB QuazipHSources libs/quazip/*.h)
qt_add_executable(Unraid.USB.Creator WIN32 MACOSX_BUNDLE
    creator.cpp creator.h creator.ui
    customipeditor.h
    customlineedit.h
    deviceenumerator.h
    diskwriter.cpp diskwriter.h
    downloadmanager.cpp downloadmanager.h
    jsonparser.cpp jsonparser.h
    ${QuazipCSources} ${QuazipCPPSources} ${QuazipHSources}
    main.cpp
    movingaverage.cpp movingaverage.h
    translator.cpp translator.h
)
target_link_libraries(Unraid.USB.Creator PRIVATE
    Qt::Concurrent
    Qt::Core
    Qt::Core5Compat
    Qt::Gui
    Qt::Network
    Qt::Widgets
    -static
)


# Resources:
set(resources_resource_files
    "bin/boot_16k.img.gz"
    "bin/boot_32k.img.gz"
    "bin/boot_4k.img.gz"
    "bin/boot_8k.img.gz"
    "bin/shadow"
    "bin/syslinux/ldlinux.c32"
    "bin/syslinux/libcom32.c32"
    "bin/syslinux/libutil.c32"
    "bin/syslinux/make_bootable_linux.sh"
    "bin/syslinux/make_bootable_mac.sh"
    "bin/syslinux/mboot.c32"
    "bin/syslinux/mbr.bin"
    "bin/syslinux/menu.c32"
    "bin/syslinux/syslinux"
    "bin/syslinux/syslinux.cfg"
    "bin/syslinux/syslinux.cfg-"
    "bin/syslinux/syslinux.exe"
    "bin/syslinux/syslinux_linux"
    "images/eye_off.png"
    "images/eye_on.png"
    "images/header.png"
    "images/icon_256.png"
    "images/info.png"
    "images/no1.png"
    "images/no2.png"
    "images/no3.png"
    "images/refresh.png"
    "images/winapp_icon.ico"
    "lang/flag-de_DE.png"
    "lang/flag-empty.png"
    "lang/flag-en_US.png"
    "lang/flag-es_ES.png"
    "lang/flag-fr_FR.png"
    "lang/flag-zh_CN.png"
    "lang/lang-de_DE.qm"
    "lang/lang-en_US.qm"
    "lang/lang-es_ES.qm"
    "lang/lang-fr_FR.qm"
    "lang/lang-zh_CN.qm"
)

qt_add_resources(Unraid.USB.Creator "resources"
    PREFIX
        "/"
    FILES
        ${resources_resource_files}
)

target_compile_definitions(Unraid.USB.Creator PRIVATE
    STATIC
)

FILE(GLOB ZlibCSources libs/zlib/*.c)
FILE(GLOB ZlibHSources libs/zlib/*.h)
if(WIN32)
    target_sources(Unraid.USB.Creator PUBLIC
        deviceenumerator_windows.cpp deviceenumerator_windows.h
        diskwriter_windows.cpp diskwriter_windows.h
        ${ZlibCSources} ${ZlibHSources}
        privileges.h
    )

    target_include_directories(Unraid.USB.Creator PRIVATE
        libs/zlib
    )

    target_compile_definitions(Unraid.USB.Creator PRIVATE
        QUAZIP_STATIC
    )

    target_link_libraries(Unraid.USB.Creator PRIVATE
        setupapi
    )
endif()

if(UNIX)
    target_sources(Unraid.USB.Creator PUBLIC
        diskwriter_unix.cpp diskwriter_unix.h
        privileges_unix.cpp privileges_unix.h
    )

    target_link_libraries(Unraid.USB.Creator PRIVATE
        z
    )

    target_compile_options(Unraid.USB.Creator
        -fPIC
    )
endif()

if(MACOS)
    target_sources(Unraid.USB.Creator PUBLIC
        deviceenumerator_macos.cpp deviceenumerator_macos.h
    )
endif()

if(linux_x_)
    target_sources(Unraid.USB.Creator PUBLIC
        deviceenumerator_linux.cpp deviceenumerator_linux.h
    )

    target_link_libraries(Unraid.USB.Creator PRIVATE
        blkid
    )
endif()

install(TARGETS Unraid.USB.Creator
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
    TARGET Unraid.USB.Creator
    FILENAME_VARIABLE deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
