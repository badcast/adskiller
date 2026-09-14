 cmake_minimum_required(VERSION 3.20)

# READ VERSION
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" APPVERSION)
string(STRIP "${APPVERSION}" APPVERSION)

project(adskiller VERSION ${APPVERSION} LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Widgets Network Multimedia)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Multimedia)

set(PROJECT_SOURCES
        ui/MainWindow.h
        ui/MainWindow.ui
        ui/MainWindow.cpp
        ui/MainPages.cpp
        ui/RadioPlayerWidget.h
        ui/RadioPlayerWidget.cpp
        ui/AdbDeviceVisualizer.h
         ui/AdbDeviceVisualizer.cpp
        ui/AboutDialog.h
        ui/AboutDialog.cpp
        ui/IconTextButton.cpp
        ui/IconTextButton.h
        include/Begin.h
        include/Extension.h
        include/Network.h
        include/AppSystemTray.h
        include/Strings.h
        src/Main.cpp
        src/Network.cpp
        src/Identity.cpp
        src/AppSystemTray.cpp
        src/Extension.cpp
        src/Settings.cpp
        resources.qrc
)

file(GLOB UPDATE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/update/*" "${CMAKE_CURRENT_SOURCE_DIR}/ui/Update*")

if(WIN32)
    file(COPY "${CMAKE_SOURCE_DIR}/3rdParty/adb" DESTINATION "${CMAKE_BINARY_DIR}")
    set(PROJECT_SOURCES ${PROJECT_SOURCES} "${CMAKE_SOURCE_DIR}/adskiller.rc")
    set(UPDATE_SOURCES ${UPDATE_SOURCES} "${CMAKE_SOURCE_DIR}/update.rc")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/3rdParty/apple")
    file(COPY "${CMAKE_SOURCE_DIR}/3rdParty/apple" DESTINATION "${CMAKE_BINARY_DIR}")
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(application
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
    qt_add_executable(update
        MANUAL_FINALIZATION
        update.qrc
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET application APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(application SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(application
            ${PROJECT_SOURCES}
        )
        add_executable(update)
    endif()
endif()

add_compile_definitions(APPVERSION="${APPVERSION}" ADS_VER_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR} ADS_VER_MINOR=${CMAKE_PROJECT_VERSION_MINOR} ADS_VER_PATCH=${CMAKE_PROJECT_VERSION_PATCH})

add_subdirectory(src/widgets)
add_subdirectory(src/common/adbfront)
add_subdirectory(src/services)

set_target_properties(application PROPERTIES OUTPUT_NAME adskiller)

target_sources(update PRIVATE ${UPDATE_SOURCES})

target_include_directories(application PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/ui)
target_include_directories(update PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/ui)

target_link_libraries(services PRIVATE adbfront widgets)
target_link_libraries(application PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Multimedia adbfront services widgets)
target_link_libraries(update PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)

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
install(TARGETS application
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(application)
qt_finalize_executable(update)
