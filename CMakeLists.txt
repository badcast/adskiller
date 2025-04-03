cmake_minimum_required(VERSION 3.16)

# READ VERSION
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" ADSVERSION)
string(STRIP "${ADSVERSION}" ADSVERSION)

project(adskiller VERSION ${ADSVERSION} LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

set(PROJECT_SOURCES
        ui/mainwindow.h
        ui/mainwindow.ui
        ui/mainwindow.cpp
        include/adbtrace.h
        include/network.h
        include/packages.h
        include/adbcmds.h
        src/main.cpp
        src/packages.cpp
        src/adbtrace.cpp
        src/network.cpp
        src/malwareProcess.cpp
        resources.qrc
)

add_executable(update update/main.cpp ui/update_window.h ui/update_window.cpp ui/update_window.ui)

if(WIN32)
    file(COPY "${CMAKE_SOURCE_DIR}/3rdParty/adb" DESTINATION "${CMAKE_BINARY_DIR}")
    set(PROJECT_SOURCES ${PROJECT_SOURCES} "${CMAKE_SOURCE_DIR}/resources.rc")
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(adskiller
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET adskiller APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(adskiller SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(adskiller
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_include_directories(adskiller PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/ui)
target_include_directories(update PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/ui)

target_link_libraries(adskiller PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)
target_link_libraries(update PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)

target_compile_definitions(adskiller PRIVATE ADSVERSION="${ADSVERSION}")
target_compile_definitions(update PRIVATE APPVERSION="${ADSVERSION}")

include(GNUInstallDirs)
install(TARGETS adskiller
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(adskiller)
    qt_finalize_executable(update)
endif()
