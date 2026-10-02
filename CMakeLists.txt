cmake_minimum_required(VERSION 3.20)

# READ VERSION
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" APPVERSION)
string(STRIP "${APPVERSION}" APPVERSION)

project(adskiller VERSION ${APPVERSION} LANGUAGES C CXX)

set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Widgets Network Multimedia)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Multimedia)

set(PROJECT_SOURCES
        ui/MainWindow.h
        ui/ui_MainWindow.h
        ui/MainWindow.cpp
        ui/MainPages.cpp
        ui/RadioPlayerWidget.h
        ui/RadioPlayerWidget.cpp
        ui/DeviceVisualizer.h
        ui/DeviceVisualizer.cpp
        ui/CyberReactorLoader.h
        ui/CyberReactorLoader.cpp
        ui/AboutDialog.h
        ui/AboutDialog.cpp
        include/Begin.h
        include/Extension.h
        include/Network.h
        include/AppSystemTray.h
        include/Strings.h
        Source/Main.cpp
        Source/Network.cpp
        Source/Identity.cpp
        Source/AppSystemTray.cpp
        Source/Extension.cpp
        Source/Settings.cpp
        resources.qrc
)

set(UPDATE_SOURCES
        update/Main.cpp
        update/UpdateManager.cpp
        update/UpdateManager.h
        ui/UpdateWindow.cpp
        ui/UpdateWindow.h
)

if(WIN32)
    file(COPY "${CMAKE_SOURCE_DIR}/3rdParty/adb" DESTINATION "${CMAKE_BINARY_DIR}")
    set(PROJECT_SOURCES ${PROJECT_SOURCES} "${CMAKE_SOURCE_DIR}/adskiller.rc")
    set(UPDATE_SOURCES ${UPDATE_SOURCES} "${CMAKE_SOURCE_DIR}/update.rc")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/3rdParty/apple")
    file(COPY "${CMAKE_SOURCE_DIR}/3rdParty/apple" DESTINATION "${CMAKE_BINARY_DIR}")
endif()

qt_add_executable(application
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)
qt_add_executable(update
    MANUAL_FINALIZATION
    update.qrc
)

# Widgets UI
add_subdirectory(Source/widgets)
# Adb Shell I/O
add_subdirectory(Source/common/adbfront)
# Apple Shell I/O
add_subdirectory(Source/common/applefront)
# Services I/O
add_subdirectory(Source/services)

set_target_properties(application PROPERTIES OUTPUT_NAME adskiller)

target_sources(update PRIVATE ${UPDATE_SOURCES})

target_include_directories(application PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/ui)
target_include_directories(update PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/ui)

set(APP_COMPILE_DEFINITIONS
    APPVERSION="${APPVERSION}"
    ADS_VER_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
    ADS_VER_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
    ADS_VER_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
)
add_library(adskiller_build_config INTERFACE)
target_compile_definitions(adskiller_build_config INTERFACE ${APP_COMPILE_DEFINITIONS})

target_link_libraries(services PRIVATE adskiller_build_config adbfront applefront widgets)
target_link_libraries(application PRIVATE adskiller_build_config Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Multimedia adbfront applefront services widgets)
target_link_libraries(update PRIVATE adskiller_build_config Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.env")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/.env" REMOTEADDR)
string(STRIP "${REMOTEADDR}" REMOTEADDR)
target_compile_definitions(application PRIVATE NREMOTEADDR="${REMOTEADDR}")
target_compile_definitions(update PRIVATE NREMOTEADDR="${REMOTEADDR}")
endif()

if(WIN32)
  set_property(TARGET application PROPERTY WIN32_EXECUTABLE true)
  set_property(TARGET update PROPERTY WIN32_EXECUTABLE true)
endif()

include(GNUInstallDirs)
install(TARGETS application update
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(application)
qt_finalize_executable(update)
