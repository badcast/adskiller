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

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network)

set(PROJECT_SOURCES
        ui/mainwindow.h
        ui/mainwindow.ui
        ui/mainwindow.cpp
        include/network.h
        include/ProgressCircle.h
        include/SystemTray.h
        src/main.cpp
        src/network.cpp
        src/user_identity.cpp
        src/malwareProcess.cpp
        src/widget/ProgressCircle.cpp
        src/SystemTray.cpp
        resources.qrc
)

if(WIN32)
    file(COPY "${CMAKE_SOURCE_DIR}/3rdParty/adb" DESTINATION "${CMAKE_BINARY_DIR}")
    set(PROJECT_SOURCES ${PROJECT_SOURCES} "${CMAKE_SOURCE_DIR}/resources.rc")
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(adskiller
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
    qt_add_executable(update
        MANUAL_FINALIZATION
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
        add_executable(update)
    endif()
endif()

add_subdirectory(src/common/adbfront)

file(GLOB UPDATE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/update/*" "${CMAKE_CURRENT_SOURCE_DIR}/ui/update_*")
target_sources(update PRIVATE ${UPDATE_SOURCES})

target_include_directories(adskiller PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/ui)
target_include_directories(update PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/ui)

target_link_libraries(adskiller PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network adbfront)
target_link_libraries(update PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)

target_compile_definitions(adskiller PRIVATE ADS_VER_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR} ADS_VER_MINOR=${CMAKE_PROJECT_VERSION_MINOR} ADS_VER_PATCH=${CMAKE_PROJECT_VERSION_PATCH} )
target_compile_definitions(update PRIVATE APPVERSION="${ADSVERSION}")

if(WIN32)
  set_property(TARGET adskiller PROPERTY WIN32_EXECUTABLE true)
  set_property(TARGET update PROPERTY WIN32_EXECUTABLE true)
endif()

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

#message(FATAL_ERROR ${adbfront_INCLUDE_DIRS})
