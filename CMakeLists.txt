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
        Source/UI/MainWindow.h
        Source/UI/ui_MainWindow.h
        Source/UI/MainWindow.cpp
        Source/UI/MainPages.cpp
        Source/UI/RadioPlayerWidget.h
        Source/UI/RadioPlayerWidget.cpp
        Source/UI/DeviceVisualizer.h
        Source/UI/DeviceVisualizer.cpp
        Source/UI/CyberReactorLoader.h
        Source/UI/CyberReactorLoader.cpp
        Source/UI/AboutDialog.h
        Source/UI/AboutDialog.cpp
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
        Resources/resources.qrc
)

set(UPDATE_SOURCES
        update/Main.cpp
        update/UpdateManager.cpp
        update/UpdateManager.h
        Source/UI/UpdateWindow.cpp
        Source/UI/UpdateWindow.h
)

if(WIN32)
    file(COPY "${CMAKE_SOURCE_DIR}/3rdParty/adb" DESTINATION "${CMAKE_BINARY_DIR}")
    set(PROJECT_SOURCES ${PROJECT_SOURCES} "${CMAKE_SOURCE_DIR}/Resources/adskiller.rc")
    set(UPDATE_SOURCES ${UPDATE_SOURCES} "${CMAKE_SOURCE_DIR}/Resources/update.rc")
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
    Resources/update.qrc
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

target_include_directories(application PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/Source/UI)
target_include_directories(update PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/Source/UI)

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

set(NREMOTEADDR_VALUE "")
if(DEFINED ENV{NREMOTEADDR} AND NOT "$ENV{NREMOTEADDR}" STREQUAL "")
    set(NREMOTEADDR_VALUE "$ENV{NREMOTEADDR}")
elseif(DEFINED ENV{REMOTEADDR} AND NOT "$ENV{REMOTEADDR}" STREQUAL "")
    set(NREMOTEADDR_VALUE "$ENV{REMOTEADDR}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.env")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/.env" ENV_LINES ENCODING UTF-8)
    set(LEGACY_ENV_VALUE "")
    foreach(ENV_LINE IN LISTS ENV_LINES)
        string(STRIP "${ENV_LINE}" ENV_LINE)
        if(ENV_LINE STREQUAL "" OR ENV_LINE MATCHES "^#")
            continue()
        endif()

        if(ENV_LINE MATCHES "^(export[ \t]+)?(NREMOTEADDR|REMOTEADDR)[ \t]*=[ \t]*(.*)$")
            set(NREMOTEADDR_VALUE "${CMAKE_MATCH_3}")
        elseif(NOT ENV_LINE MATCHES "=" AND LEGACY_ENV_VALUE STREQUAL "")
            set(LEGACY_ENV_VALUE "${ENV_LINE}")
        endif()
    endforeach()

    if(NREMOTEADDR_VALUE STREQUAL "")
        set(NREMOTEADDR_VALUE "${LEGACY_ENV_VALUE}")
    endif()

    string(STRIP "${NREMOTEADDR_VALUE}" NREMOTEADDR_VALUE)
    string(LENGTH "${NREMOTEADDR_VALUE}" ENV_VALUE_LENGTH)
    if(ENV_VALUE_LENGTH GREATER_EQUAL 2)
        string(SUBSTRING "${NREMOTEADDR_VALUE}" 0 1 ENV_VALUE_FIRST_CHAR)
        math(EXPR ENV_VALUE_LAST_INDEX "${ENV_VALUE_LENGTH} - 1")
        string(SUBSTRING "${NREMOTEADDR_VALUE}" ${ENV_VALUE_LAST_INDEX} 1 ENV_VALUE_LAST_CHAR)
        if((ENV_VALUE_FIRST_CHAR STREQUAL "\"" AND ENV_VALUE_LAST_CHAR STREQUAL "\"")
           OR (ENV_VALUE_FIRST_CHAR STREQUAL "'" AND ENV_VALUE_LAST_CHAR STREQUAL "'"))
            string(SUBSTRING "${NREMOTEADDR_VALUE}" 1 ${ENV_VALUE_LAST_INDEX} NREMOTEADDR_VALUE)
        endif()
    endif()
endif()

if(NOT NREMOTEADDR_VALUE STREQUAL "")
    target_compile_definitions(application PRIVATE "NREMOTEADDR=\"${NREMOTEADDR_VALUE}\"")
    target_compile_definitions(update PRIVATE "NREMOTEADDR=\"${NREMOTEADDR_VALUE}\"")
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
