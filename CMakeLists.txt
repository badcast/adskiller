cmake_minimum_required(VERSION 3.20)

# READ VERSION
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt" APPVERSION)
string(STRIP "${APPVERSION}" APPVERSION)

project(adskiller VERSION ${APPVERSION} LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Widgets Network Quick Qml QuickControls2)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Quick Qml QuickControls2)

set(PROJECT_SOURCES
        ui/mainwindow.h
        ui/mainwindow.cpp
        ui/icontextbutton.cpp
        ui/icontextbutton.h
        include/begin.h
        include/extension.h
        include/network.h
        include/AppSystemTray.h
        include/Strings.h
        src/main.cpp
        src/network.cpp
        src/Identity.cpp
        src/AppSystemTray.cpp
        src/extension.cpp
        src/settings.cpp
        resources.qrc
)

file(GLOB UPDATE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/update/*" "${CMAKE_CURRENT_SOURCE_DIR}/ui/update_*")

if(WIN32)
    file(COPY "${CMAKE_SOURCE_DIR}/3rdParty/adb" DESTINATION "${CMAKE_BINARY_DIR}")
    # Also copy adb to multi-config output dirs (e.g. Release/, Debug/) for MSVC
    file(COPY "${CMAKE_SOURCE_DIR}/3rdParty/adb" DESTINATION "${CMAKE_BINARY_DIR}/Release")
    file(COPY "${CMAKE_SOURCE_DIR}/3rdParty/adb" DESTINATION "${CMAKE_BINARY_DIR}/Debug")
    set(PROJECT_SOURCES ${PROJECT_SOURCES} "${CMAKE_SOURCE_DIR}/adskiller.rc")
    set(UPDATE_SOURCES ${UPDATE_SOURCES} "${CMAKE_SOURCE_DIR}/update.rc")
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    # Redirect QML module output to avoid name collision with the executable
    # on case-insensitive filesystems (macOS/Windows): URI "Adskiller" directory
    # would clash with OUTPUT_NAME "adskiller"
    set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

    qt_add_executable(application
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
    qt_add_qml_module(application
        URI Adskiller
        VERSION 1.0
        RESOURCE_PREFIX /
        QML_FILES
            qml/main.qml
            qml/pages/AuthPage.qml
            qml/pages/CabinetPage.qml
            qml/pages/LongInfoPage.qml
            qml/pages/BuyVIPPage.qml
            qml/pages/MyDevicesPage.qml
            qml/pages/DevicesPage.qml
            qml/pages/ContactFixerPage.qml
            qml/pages/FileExplorerPage.qml
            qml/components/GlassCard.qml
            qml/components/CircularProgress.qml
        DEPENDENCIES
            QtQuick
            QtQuick.Controls
            QtQuick.Layouts
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
target_link_libraries(application PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::QuickControls2 adbfront services widgets)
target_link_libraries(update PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.env")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/.env" REMOTEADDR)
string(STRIP "${REMOTEADDR}" REMOTEADDR)
target_compile_definitions(application PRIVATE NREMOTEADDR="${REMOTEADDR}")
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

if(WIN32)
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/3rdParty/adb" DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Run windeployqt after build to copy all required Qt QML modules, plugins,
# and DLLs next to the executable. This makes the build self-contained.
if(WIN32)
    get_filename_component(_qt_prefix_path "${Qt6_DIR}/../../.." ABSOLUTE)
    set(_windeployqt "${_qt_prefix_path}/bin/windeployqt.exe")
    add_custom_command(TARGET application POST_BUILD
        COMMAND "${_windeployqt}"
            --no-translations
            --no-compiler-runtime
            --qmldir "${CMAKE_SOURCE_DIR}/qml"
            "$<TARGET_FILE:application>"
        COMMENT "Running windeployqt to deploy QML modules..."
    )
endif()

qt_finalize_executable(application)
qt_finalize_executable(update)
